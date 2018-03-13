/*
 * Assignment 2
 * Author: Christian Caburian
 * Student No.: 2201334075
 * Unit: COSC330
 
 * Desc: 
 * Program will create a random number of writer threads, that either
 * increment or decrement a shared counter.
 * A random number of reader threads will be created to read the counter.
 * The counter is protected from being accessed by any thread while a 
 * write is taking place. When a read is taking place, only reader
 * threads are allowed access to the counter.

 * How to run:
 * A makefile is included to compile the program in the console.
 * The program is run by executing the file 'a2'.
 */
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include "sem_ops.h"

/*
 * Shared data struct, accessed by all threads.
 */
typedef struct { 
  int sum;                        // Shared counter.  
  int read_count;                 // Readers currently accessing the sum.   
  int count_lock, data_lock;      // Semaphores to protect sum and read_count.
  int num_inc, num_dec, num_read; // Number of thread types.
  int last_incid, last_decid;     // Log the last incrementer/decrementer.
}	shared_t;
/*
 * Thread data struct, accessed by a single thread.
 */
typedef struct {
  int id;
  shared_t *shared_ptr;
}	params_t;
/*
 * Incrementer thread entry-point function; increments the sum.
 */
void *inc_func(void *arg) 
{ 
  params_t inc_t = *((params_t *)arg);
  int d_lock = inc_t.shared_ptr->data_lock;

  // Enter critical section.
  P(d_lock);

  // Simulate thread scheduler delays.
  srand(time(NULL));
  sleep(rand() % 3); 

  // Access and write to the shared data.
  inc_t.shared_ptr->sum++;
  printf("Incrementer %d set sum = %d\n", inc_t.id, inc_t.shared_ptr->sum);

  // Exit critical section.
  inc_t.shared_ptr->last_incid = inc_t.id;
  V(d_lock);
  
  pthread_exit(NULL);
}
/*
 * Decrementer thread entry-point function; decrements the sum.
 */
void *dec_func(void *arg) 
{
  params_t dec_t = *((params_t *)arg);
  int d_lock = dec_t.shared_ptr->data_lock;

  // Enter critical section.
  P(d_lock);

  // Simulate thread scheduler delays.
  srand(time(NULL));
  sleep(rand() % 3); 

  // Access and write to the shared data.
  dec_t.shared_ptr->sum--;
  printf("Decrementer %d set sum = %d\n", dec_t.id, dec_t.shared_ptr->sum);

  // Exit critical section.
  dec_t.shared_ptr->last_decid = dec_t.id;
  V(d_lock);
  
  pthread_exit(NULL);
}
/*
 * Reader thread entry-point function; prints the sum.
 */
void *read_func(void *arg) 
{ 
  params_t read_t = *((params_t *)arg);
  int c_lock = read_t.shared_ptr->count_lock;
  int d_lock = read_t.shared_ptr->data_lock;

  // Protect read_count from readers while incrementing.
  P(c_lock);
  read_t.shared_ptr->read_count++;

  // Protect data from write-access for the first reader. 
  if (read_t.shared_ptr->read_count == 1)
    P(d_lock);
    
  // Release read_count after incrementing.
  V( c_lock );

  // Simulate thread scheduler delays.
  srand(time(NULL));
  sleep(rand() % 3);

  // Access and read the shared data.
  printf("Reader %d got %d\n", read_t.id, read_t.shared_ptr->sum);

  // Protect read_count from readers while decrementing.
  P(c_lock);
  read_t.shared_ptr->read_count--;

  // Release data for write-access for the last reader. 
  if (read_t.shared_ptr->read_count == 0)
    V(d_lock);
    
  // Release read_count after decrementing.
  V(c_lock);

  pthread_exit(NULL);
}

int main(void) 
{
  int i; 

  // Initialize shared data.
  shared_t data;
  data.sum = 0;
  data.read_count = 0;
  
  // Initialize semaphores.
  data.count_lock = semtran(IPC_PRIVATE);
  data.data_lock = semtran(IPC_PRIVATE);
  V(data.count_lock); 
  V(data.data_lock);  
  
  // Randomize number of thread types.
  srand(time(NULL));
  data.num_inc = rand() % 10;
  srand(time(NULL));
  data.num_dec = rand() % 10;
  srand(time(NULL));
  data.num_read = rand() % 20;

  // Initialize thread data arrays.
  pthread_t inc_tid[data.num_inc], dec_tid[data.num_dec], rd_tid[data.num_read];
  params_t inc_d[data.num_inc], dec_d[data.num_dec], read_d[data.num_read];

  // Assign thread data.
  for (i=0; i<data.num_read; i++) {
    read_d[i].id = i;
	  read_d[i].shared_ptr = &data;
  }
  for (i=0; i<data.num_inc; i++) {
    inc_d[i].id = i;
	  inc_d[i].shared_ptr = &data;
  }
  for (i=0; i<data.num_dec; i++) {
    dec_d[i].id = i;
	  dec_d[i].shared_ptr = &data;
  }

  // Create threads.
  for (i=0; i<data.num_inc; i++) {
    if (pthread_create(&inc_tid[i], NULL, inc_func, (void *)&inc_d[i])) {
      perror("Could not create incrementer");
      exit(EXIT_FAILURE);
    }
  }
  for (i=0; i<data.num_dec; i++) {
    if (pthread_create(&dec_tid[i], NULL, dec_func, (void *)&dec_d[i])) {
      perror("Could not create decrementer");
      exit(EXIT_FAILURE);
    }
  }
  for (i=0; i<data.num_read; i++) {
    if (pthread_create(&rd_tid[i], NULL, read_func, (void *)&read_d[i])) {
      perror("Could not create reader");
      exit(EXIT_FAILURE);
    }
  }

  // Thread completion.
  for (i=0; i<data.num_inc; i++) 
    pthread_join(inc_tid[i], NULL);
  for (i=0; i<data.num_dec; i++) 
    pthread_join(dec_tid[i], NULL);
  for (i=0; i<data.num_read; i++) 
    pthread_join(rd_tid[i], NULL);

  // Remove semaphores.
  rm_sem(data.count_lock);
  rm_sem(data.data_lock);

  // Print final data state.
  printf("There were %d readers, %d incrementers, and %d decrementers.\n", 
          data.num_read, data.num_inc, data.num_dec);
  printf("The final state of the data is:\n");
  printf("\tLast incrementer: %d\n", data.last_incid);
  printf("\tLast decrementer: %d\n", data.last_decid);
  printf("\tTotal writers: %d\n", data.num_inc + data.num_dec);
  printf("\tSum: %d\n", data.sum);

  exit(EXIT_SUCCESS);
  return 0;
}
