/*
 * Author: Christian Caburian
 * Student No.: 2201334075
 * Unit: COSC330
 * Desc: 
 * Program will find a full key for decrypting a plain text file given a 
 * partial key, a cipher text file, and a plain text file.
 * The keyspace is searched by multiple processes in parallel arranged 
 * in a ring.
 * If found, the key will be passed from process to process around the 
 * ring to the mother process where it is outputted.

 * How to run:
 * A makefile is included to compile the program in the console.
 * The program is run by executing with the number of processes and
 * partial key inputted.
 * For example, to run a search that uses 64 processes and the 
 * partial key, L17hhOCMtHI8L6m67Twgo8Dx7n0jD, type the following in 
 * the console:
 * >parallel_search_keyspace 64 L17hhOCMtHI8L6m67Twgo8Dx7n0jD
 */

#include <stdio.h>		/* for fprintf  */
#include <stdlib.h>		/* for exit     */
#include <unistd.h>		/* for fork     */
#include "search_func.c"
#define BUFFSIZE 32

/*
 * Parse command line arguments.
 */
int parse_args(int argc, char *argv[], int *np)
{
	if ((argc != 3) || ((*np = atoi (argv[1])) <= 0)) {
		fprintf (stderr, "Usage: %s nprocs\n", argv[0]);
		return (-1);
	};
	return (0);
}
/*
 * Create ring structure to arrange processes.
 */
int make_trivial_ring()
{
	int fd[2];
	if (pipe (fd) == -1)
		return (-1);
	if ((dup2(fd[0], STDIN_FILENO) == -1) || 
	    (dup2(fd[1], STDOUT_FILENO) == -1))
		return (-2);
	if ((close(fd[0]) == -1) || (close(fd[1]) == -1))
		return (-3);
	return (0);
}
/*
 * Adds/inserts a process into the ring
 */
int add_new_node(int *pid)
{
	int fd[2];
	if (pipe(fd) == -1)
		return (-1);
	if ((*pid = fork ()) == -1)
		return (-2);
	if (*pid > 0 && dup2(fd[1], STDOUT_FILENO) < 0)
		return (-3);
	if (*pid == 0 && dup2(fd[0], STDIN_FILENO) < 0)
		return (-4);
	if ((close(fd[0]) == -1) || (close(fd[1]) == -1))
		return (-5);
	return (0);
}
/*
 * Writes pipe (input) data to the process buff.
 */
void get_integers(unsigned char buff[])
{
	if (read(STDIN_FILENO, buff, BUFFSIZE) < 0) 
		fprintf(stderr, "Read of bytes failed");
}
/*
 * Writes process buff data to the pipe (output).
 */
int send_numbers(unsigned char buff[])
{
	int bytes, len;
	len = strlen (buff);
	if ((bytes = write (STDOUT_FILENO, buff, len)) != len) {
		fprintf(stderr,
		        "Write of %d bytes failed, only sent %d bytes\n",
			len, bytes);
		return -1;
	} else {
		return bytes;
	}
}
/*
 * Reports the key (if found) to the console.
 */
void key_found (unsigned char buff[])
{
	fprintf (stderr,
		 "P1 (PID#%d) has received the key: ",
		 (int)getppid());
	int i;
	for (i = 0; i < BUFFSIZE; i++) {
		fprintf (stderr, "%c", buff[i]);
	}
	fprintf (stderr, "\n");
}
int main (int argc, char *argv[])
{
	int i;				/* number of this process (starting with 1)   */
	int childpid;			/* indicates process should spawn another     */
	int nprocs;			/* total number of processes in ring          */
	unsigned char buff[BUFFSIZE];	/* buff will either pass the key or a message */

	// Input reading.
	if (parse_args (argc, argv, &nprocs) < 0)
		exit (EXIT_FAILURE);

	// Process creation.
	if (make_trivial_ring () < 0) {
		perror ("Could not make trivial ring");
		exit (EXIT_FAILURE);
	}
	for (i = 1; i < nprocs; i++) {
		if (add_new_node (&childpid) < 0) {
			perror ("Could not add new node to ring");
			exit (EXIT_FAILURE);
		}
		if (childpid)
			break;
	}
	{
	// Ring process code.
	if (i == 1) {
	// Mother process.
		/* search_func() = Result of keysearch:
		 * * Key found = 1
		 * * Key not found = 0
		 * * Encrypt error = -1
		 * * Partial key too long = -2
		 */
		int valid_search = search_func(argv, i, nprocs, buff);
		if (valid_search == -2) {
			fprintf(stderr,
				"Error: Partial key too long.\n");
			exit(EXIT_FAILURE);
		}
		else if (valid_search <= 0) {
			// setting buff[0] = 1 signals that the 
			// mother process failed to find the key.
			buff[0] = 1;
		}
		send_numbers(buff);
		buff[0] = 0;
		while (strlen(buff) == 0) {
			get_integers (buff);
		}
		if ((strlen(buff) == 1) && (buff[0] == 1)) {
			// if the mother process receives its own
			// signal that it failed, then the ring has failed.
			fprintf(stderr, "Search has failed.\n");
		} else {
			key_found(buff);
		}
	} else {
	// child processes
		if (search_func(argv, i, nprocs, buff) == 1) {
			send_numbers(buff);
		} else {
			buff[0] = 0;
			while (strlen(buff) == 0) {
				get_integers (buff);
			}
			send_numbers (buff);
		}
	}
	exit (EXIT_SUCCESS);
	}
}	/* end of main program here */
