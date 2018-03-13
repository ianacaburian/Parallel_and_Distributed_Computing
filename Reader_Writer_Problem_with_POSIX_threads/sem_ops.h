
/* 	sem_ops.h	-	simplified semaphore operations
	in, 'Advanced UNIX Programming', M.J. Rochkind
*/
#include <sys/sem.h>

/*	semtran		will return the semaphore id (system wide) of the
			semaphore number you give. If no semaphore has been
			established for this number, one is created	*/
int semtran( int key ){	/* translate/ create semaphore key to id */
  int sid;

  if( (sid = semget( (key_t)key, 1, 0666 | IPC_CREAT )) == -1 )
    printf( "semget" );
  return( sid );
}

/*	semcall		applies the operation to the semaphore */
static void semcall( int sid, int op ) {
  struct sembuf sb;

  sb.sem_num = 0;
  sb.sem_op = op;
  sb.sem_flg = 0;
  if( semop( sid, &sb, 1 ) == -1 )
    printf( "semop" );
}

/*	P		the semaphore signal operation. sid must be the system
			wide semaphore number returned by semtran above	*/
void P( int sid ) {
  semcall( sid, -1 );
}

/*	V		the semaphore release operation. sid must be the system 
			wide semaphore number returned by semtran above. */ 
void V( int sid ) {
  semcall( sid, 1 );
}

/*	rm_sem		remove a semaphore from the system. sid must be the
			system wide semaphore returned from semtran	*/
void rm_sem( int sid ) {	/* remove semaphore */
  (void)semctl( sid, 0, IPC_RMID, 0 );
}
