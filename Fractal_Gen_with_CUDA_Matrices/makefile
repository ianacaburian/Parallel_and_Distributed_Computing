COMPILER = nvcc
CFLAGS = -I /usr/local/cuda-7.5/samples/common/inc 
COBJS = bmpfile.o
EXES = a4
all: ${EXES}

a4: a4.cu ${COBJS}
	${COMPILER} ${CFLAGS} a4.cu ${COBJS} -o a4

bmpfile.o: bmpfile.c
	gcc -Wall -c bmpfile.c

%.o: %.c %.h  makefile
	${COMPILER} ${CFLAGS} $< -c 

clean:
	rm -f *.o *~ ${EXES} ${CFILES}
