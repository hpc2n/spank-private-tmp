/*
 * Spank plugin private-tmpdir (c) HPC2N.umu.se
 * Author: Magnus Jonsson <magnus@hpc2n.umu.se>
 * Author: Lars Viklund <lars@hpc2n.umu.se>
 * Author: Ake Sandgren <ake@hpc2n.umu.se>
 * Author: Pär Lindfors <paran@nsc.liu.se>
 */

/* Needs to be defined before first invocation of features.h so enable
 * it early. */
#define _GNU_SOURCE		/* See feature_test_macros(7) */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <sys/mount.h>
#include <slurm/spank.h>
#include <unistd.h>
#include <sched.h>

SPANK_PLUGIN(private-tmpdir, 1);

// Default
#define MAX_BIND_DIRS 16

// Globals
static int init_opts = 0;
static int binded = 0;
static char pbase[PATH_MAX + 1] = "";
static uid_t uid = (uid_t) - 1;
static gid_t gid = (gid_t) - 1;
static uint32_t jobid;
static uint32_t restartcount;

static char *bases[MAX_BIND_DIRS];
static char *base_paths[MAX_BIND_DIRS];
static char *bind_dirs[MAX_BIND_DIRS];
static char *bind_paths[MAX_BIND_DIRS];
static int bind_count = 0;
static int base_count = 0;
// Globals

static int _tmpdir_bind(spank_t sp, int ac, char **av);
static int _tmpdir_cleanup(spank_t sp, int ac, char **av);
static int _tmpdir_init(spank_t sp, int ac, char **av);
static int _tmpdir_init_opts(spank_t sp, int ac, char **av);

/*
 *  Called from both srun and slurmd.
 */
int slurm_spank_init(spank_t sp, int ac, char **av)
{
	return _tmpdir_init_opts(sp, ac, av);
}

int slurm_spank_exit(spank_t sp, int ac, char **av)
{
	return _tmpdir_cleanup(sp, ac, av);
}

int slurm_spank_job_prolog(spank_t sp, int ac, char **av)
{
	int rc, i;
	if (_tmpdir_init(sp, ac, av))
		return -1;
	for (i = 0; i < base_count; i++) {
		if (mkdir(base_paths[i], 0700)) {
			slurm_error("private-tmpdir: mkdir(\"%s\",0700): %m",
				    base_paths[i]);
			return -1;
		}
		if (chown(base_paths[i], uid, gid)) {
			slurm_error("private-tmpdir: chown(%s,%u,%u): %m",
				    base_paths[i], uid, gid);
			return -1;
		}
	}
	for (i = 0; i < bind_count; i++) {
		if (mkdir(bind_paths[i], 0700)) {
			slurm_error("private-tmpdir: mkdir(\"%s\",0700): %m",
				    bind_paths[i]);
			return -1;
		}
		if (chown(bind_paths[i], uid, gid)) {
			slurm_error("private-tmpdir: chown(%s,%u,%u): %m",
				    bind_paths[i], uid, gid);
			return -1;
		}
	}
	rc = _tmpdir_bind(sp, ac, av);
	_tmpdir_cleanup(sp, ac, av);
	return rc;
}

int slurm_spank_init_post_opt(spank_t sp, int ac, char **av)
{
	return _tmpdir_bind(sp, ac, av);
}

static int _tmpdir_bind(spank_t sp, int ac, char **av)
{
	int i;

	// only on cluster nodes
	if (!spank_remote(sp))
		return 0;
	// We have done this already
	if (binded)
		return 0;
	// Don't do this anymore
	binded = 1;

	// Init dirs
	if (_tmpdir_init(sp, ac, av))
		return -1;

	// Make / share (propagate) mounts (same as mount --make-rshared /)
	if (mount("", "/", "dontcare", MS_REC | MS_SHARED, "")) {
		slurm_error
		    ("private-tmpdir: failed to 'mount --make-rshared /' for job: %u, %m",
		     jobid);
		return -1;
	}
	// Create our own namespace
	if (unshare(CLONE_NEWNS)) {
		slurm_error
		    ("private-tmpdir: failed to unshare mounts for job: %u, %m",
		     jobid);
		return -1;
	}
	// Make / slave (same as mount --make-rslave /)
	if (mount("", "/", "dontcare", MS_REC | MS_SLAVE, "")) {
		slurm_error
		    ("private-tmpdir: failed to 'mount --make-rslave /' for job: %u, %m",
		     jobid);
		return -1;
	}
	// mount --bind bind_paths[i] bind_dirs[i]
	for (i = 0; i < bind_count; i++) {
		slurm_debug("private-tmpdir: mounting: %s %s", bind_paths[i],
			    bind_dirs[i]);
		if (mount(bind_paths[i], bind_dirs[i], "none", MS_BIND, NULL)) {
			slurm_error
			    ("private-tmpdir: failed to mount %s for job: %u, %m",
			     bind_dirs[i], jobid);
			return -1;
		}
	}
	return 0;
}

static int _tmpdir_cleanup(spank_t sp, int ac, char **av)
{
	char *prev_base = NULL;
	int i;

	for (i = 0; i < MAX_BIND_DIRS; i++) {
		if (bases[i] != prev_base) {
			prev_base = bases[i];
			free(bases[i]);
		}
		free(base_paths[i]);
		free(bind_dirs[i]);
		free(bind_paths[i]);
	}
	return 0;
}

static int _tmpdir_init(spank_t sp, int ac, char **av)
{
	int n;

	// if length(pbase) > 0, we have already bin here..
	if (pbase[0] != '\0')
		return 0;

	if (_tmpdir_init_opts(sp, ac, av))
		return 0;

	// Get JobID
	if (spank_get_item(sp, S_JOB_ID, &jobid) != ESPANK_SUCCESS) {
		slurm_error("private-tmpdir: Failed to get jobid from SLURM");
		return -1;
	}
	// Get UID
	if (spank_get_item(sp, S_JOB_UID, &uid) != ESPANK_SUCCESS) {
		slurm_error("private-tmpdir: Unable to get job's user id");
		return -1;
	}
	// Get GID
	if (spank_get_item(sp, S_JOB_GID, &gid) != ESPANK_SUCCESS) {
		slurm_debug("private-tmpdir: Unable to get job's group id");
		gid = 0;
	}
	// Get Restart count
	if (spank_get_item(sp, S_SLURM_RESTART_COUNT, &restartcount) !=
	    ESPANK_SUCCESS) {
		slurm_debug
		    ("private-tmpdir: Unable to get job's restart count");
		restartcount = 0;
	}
	// Init base paths
	for (int i = 0, j = 0; i < bind_count; i++) {
		if (i == 0 || bases[i] != bases[i - 1]) {
			n = snprintf(pbase, sizeof(pbase), "%s.%u.%u", bases[i],
				     jobid, restartcount);
			if (n < 0 || n > sizeof(pbase) - 1) {
				slurm_error
				    ("private-tmpdir: \"%s.%u.%u\" too large. Aborting",
				     bases[i], jobid, restartcount);
				return -1;
			}
			base_paths[j] = strndup(pbase, sizeof(pbase));
			j++;
		}
		// Init bind dirs path(s)
		bind_paths[i] =
		    malloc(strlen(pbase) + strlen(bind_dirs[i]) + 2);
		if (!bind_paths[i]) {
			slurm_error
			    ("private-tmpdir: Can't malloc bind_paths[i]: %m");
			return -1;
		}
		char *tmp = strdup(bind_dirs[i]);
		if (!tmp) {
			slurm_error
			    ("private-tmpdir: Can't strdup bind_dirs[i]: %m");
			return -1;
		}
		for (int j = 1; j < strlen(tmp); j++) {
			if (tmp[j] == '/') {
				tmp[j] = '_';
			}
		}
		n = snprintf(bind_paths[i], PATH_MAX, "%s%s", pbase, tmp);
		if (n < 0 || n > PATH_MAX - 1) {
			slurm_error
			    ("private-tmpdir: \"%s/%s\" too large. Aborting",
			     pbase, tmp);
			free(tmp);
			return -1;
		}
		free(tmp);
	}
	return 0;
}

static int _tmpdir_init_opts(spank_t sp, int ac, char **av)
{
	char *base = "";
	int i;

	if (init_opts)
		return 0;
	init_opts = 1;

	// for each argument in plugstack.conf
	for (i = 0; i < ac; i++) {
		if (strncmp("base=", av[i], 5) == 0) {
			const char *optarg = av[i] + 5;
			if (!strlen(optarg)) {
				slurm_error
				    ("private-tmpdir: no argument given to base= option");
				return -1;
			}
			base = strdup(optarg);
			if (!base) {
				slurm_error("private-tmpdir: can't malloc :-(");
				return -1;
			}
			base_count++;
			continue;
		}
		if (strncmp("mount=", av[i], 6) == 0) {
			// mount= before base=, use default value
			if (base_count == 0) {
				base = strdup("/tmp/slurm");
				if (!base) {
					slurm_error
					    ("private-tmpdir: can't malloc :-(");
					return -1;
				}
				base_count++;
			}
			const char *optarg = av[i] + 6;
			if (bind_count == MAX_BIND_DIRS) {
				slurm_error
				    ("private-tmpdir: Reached MAX_BIND_DIRS (%d)",
				     MAX_BIND_DIRS);
				return -1;
			}
			if (!strlen(optarg)) {
				slurm_error
				    ("private-tmpdir: no argument given to mount= option");
				return -1;
			}
			if (optarg[0] != '/') {
				slurm_error
				    ("private-tmpdir: mount= option must start with a '/': (%s)",
				     optarg);
				return -1;
			}
			bases[bind_count] = base;
			bind_dirs[bind_count] = strdup(optarg);
			if (!bind_dirs[bind_count]) {
				slurm_error("private-tmpdir: can't malloc :-(");
				return -1;
			}
			bind_count++;
			continue;
		}
		slurm_error("private-tmpdir: Invalid option \"%s\"", av[i]);
		return -1;
	}
	return 0;
}
