
COMPILER = gcc
CFLAGS = -Wall -pedantic -pthread -g
COBJS = sem_ops.h
CEXES = a2

all: ${CEXES}

a2 : a2.c ${COBJS}
	${COMPILER} ${CFLAGS} a2.c -o a2

%.o: %.c %.h  makefile
	${COMPILER} ${CFLAGS} $< -c

clean:
	rm -f *.o *~ ${CEXES}
