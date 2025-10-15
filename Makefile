#version=20.02.2
ifndef version
$(error Please set version to the version of slurm you are compiling for)
endif

sysname=`fs sysname | cut -d\' -f2`
slurm_path=/afs/hpc2n.umu.se/lap/slurm/${version}/${sysname}
install_path=$(slurm_path)/lib/slurm

CC=gcc
CPPFLAGS=-I/lap/slurm/$(version)/include
CFLAGS=-std=gnu99 -Wall -fPIC

all: private-tmpdir.so

private-tmpdir.so: private-tmpdir.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o private-tmpdir.o -c private-tmpdir.c
	$(CC) -shared -o private-tmpdir.so private-tmpdir.o

clean:
	rm -f private-tmpdir.o private-tmpdir.so

install: private-tmpdir.so
	cp private-tmpdir.so ${install_path}
