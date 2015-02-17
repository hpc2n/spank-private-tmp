all: private-tmpdir.so

private-tmpdir.so: private-tmpdir.c
	gcc -std=gnu99 -Wall -o private-tmpdir.o -fPIC -c private-tmpdir.c
	gcc -shared -o private-tmpdir.so private-tmpdir.o

clean:
	rm -f private-tmpdir.o private-tmpdir.so
