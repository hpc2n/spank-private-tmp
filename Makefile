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
LIBS=private-tmpdir.so

all: $(LIBS)

%.so: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $*.o -c $*.c
	$(CC) -shared -o $@ $*.o

clean:
	rm -f $(LIBS:.so=.o) $(LIBS)

install: $(LIBS)
	cp $(LIBS) ${install_path}
