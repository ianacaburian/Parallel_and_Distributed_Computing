COMPILER = gcc
CFLAGS = -Wall -lcrypto -g
EXES = parallel_search_keyspace
all: ${EXES}

search_func.o: search_func.c
	${COMPILER} ${CFLAGS} -c search_func.c

parallel_search_keyspace:  parallel_search_keyspace.c search_func.o
	${COMPILER} ${CFLAGS} parallel_search_keyspace.c -o parallel_search_keyspace

clean:
	rm -f *.o *~ ${EXES}
