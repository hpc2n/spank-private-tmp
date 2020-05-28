version=20.02.2
sysname=`fs sysname| cut -d\' -f2`

all: private-tmpdir.so

private-tmpdir.so: private-tmpdir.c
	gcc -I/lap/slurm/${version}/include -std=gnu99 -Wall -o private-tmpdir.o -fPIC -c private-tmpdir.c
	gcc -shared -o private-tmpdir.so private-tmpdir.o

clean:
	rm -f private-tmpdir.o private-tmpdir.so

install: private-tmpdir.so
	cp private-tmpdir.so /afs/hpc2n.umu.se/lap/slurm/${version}/${sysname}/lib/slurm/
