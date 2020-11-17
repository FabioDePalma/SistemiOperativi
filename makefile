all: gestore student

gestore: opt.conf header.h gestore.c
	gcc -std=c89 -pedantic gestore.c -o gestore


student: opt.conf header.h student.c
	gcc -std=c89 -pedantic student.c -o student




clean:
	rm -f *.o
