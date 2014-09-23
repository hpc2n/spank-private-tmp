
all: hpc2n-tmpdir.so

hpc2n-tmpdir.so: hpc2n-tmpdir.c
	gcc -std=gnu99 -Wall -o hpc2n-tmpdir.o -fPIC -c hpc2n-tmpdir.c
	gcc -shared -o hpc2n-tmpdir.so hpc2n-tmpdir.o 

clean:
	rm -f hpc2n-tmpdir.o hpc2n-tmpdir.so
