#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_LINE_SIZE 256

//Hillis Steele Approach with a barrier created by condition variables

//Globals:

int *input; //shared resource among all threads
//Create 2D array
int *finishedarray;
int n = 0; //shared resource among all threads
int numthreads = 0; //shared resource among all threads
int threadcounter = 0; //Threadcounter, atomic variable shared among all threads
int maxnumthreads = 0; //Maximum number of threads
pthread_mutex_t lock; //Mutex lock

//Global Condition Variables:
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;

//Struct of arguments casted to a void pointer for each thread to use in the calculateprefixsum() function.
struct args_t
{
	int start;
	int numth;
};

//Read in input from the argument file into an array of integer pointers
void read_input_vector(const char* filename, int n, int* array)
{
  FILE *fp;
  char *line = malloc(MAX_LINE_SIZE+1);
  size_t len = MAX_LINE_SIZE;
  ssize_t read;

  fp = strcmp(filename, "-") ? fopen(filename, "r") : stdin;

  assert(fp != NULL && line != NULL);

  int index = 0;

  while((read = getline(&line, &len, fp)) != -1)
  {
	array[index] = atoi(line);
	index++;
  }

  free(line);
  fclose(fp);
}

//Barrier function 
void barrier()
{
	//Critical section below prevents the likelihood of a race condition from occurring
	//printf("Waiting for mutex\n");
	//Use a mutex lock to lock the thread counter resource so that outside threads cannot write to it at the same time as the thread that has obtained the lock.
	pthread_mutex_lock(&lock);
	//When a thread enters, it increments the threadcounter to reflect the current number of threads that have entered the critical section.
	threadcounter+=1;
	//printf("Threadcounter is %d\n", threadcounter);
	//If less than the total number of threads have arrived, then the current thread entering must wait on condition variable cond1 and release the mutex lock so that other threads can enter the critical section.
	if(threadcounter < maxnumthreads)
	{
    	pthread_cond_wait(&cond1, &lock); //When a thread waits on condition variable 1, it unlocks the mutex so that other threads can then enter and increment the thread counter
	}
	else if(threadcounter == maxnumthreads) //If the current thread is the last thread to enter the critical section, reset the threadcounter to 0 to reflect that all the threads have entered, and broadcast with condition variable cond1 that all threads currently waiting or blocked by condition variable cond1 should continue execution into the next depth/level of computation of the Hillis Steele algorithm. 
	{
    	threadcounter = 0;
    	pthread_cond_broadcast(&cond1); //Signals all threads blocked by condition 1 to become unblocked and continue execution. This only happens when the last thread arrives.
	}
	
	pthread_mutex_unlock(&lock); //Unlock the mutex lock so that all threads can continue execution into the next depth/level of computation of the Hillis Steele algoirthm. 
}

//Calculate the prefix sum, which contains the critical section
void* calculateprefixsum(void* args)
{
	//Algorithm is not locked so that all threads can enter the prefix sum calculation at a similar time and allow for concurrent computations to occur.
	
	//Unpack the args_t struct and nitilaize start and numth
	struct args_t* structcopy = (struct args_t*) args;
	int start = (structcopy)->start;
	int numth = (structcopy)->numth;
	//printf("Thread %d starting\n", start);

	//Fill the first level of computation of the finishedarray with the values of input according to the logic of the Hillis Steele algorithm
	for(int k = start; k < n; k+=numth)
	{
    	finishedarray[k] = input[k];
	}

	//Iterate over each row of the finished array (equivalent to iterating over each levle of computation of the Hillis Steele algorithm)
	for(int i = 0; i < (log2(n)); i++)
	{
    	//Have each thread call the barrier function and wait at the barrier until all thread have arrived at the barrier. Then, all threads can concurrently enter the next depth/level of computation of the Hillis Steele algorithm.
	barrier();
	//i left-shifted by 1 equals 2^i, which is the interval between entries that are summed in the Hillis Steele algorithm
    	int two_i = 1 << i;

	//Iterate over each column where each entry jumps by the number of threads. This mechanism ensures that all threads access and sum their assigned indexes without overwriting each other, and that all values at column indexes are accounted for in the choice of dropping down a number or summing with a number to the left. 
    	for(int j = start; j < n; j+=numth)
    	{	//If the current index is less than 2^i to the right of the start index, then drop down the entry from the current level of computation/row to the next level of computation/row.
        	if(j < two_i)
        	{
            	finishedarray[((i + 1) * n) + j] = finishedarray[(i * n) + j];
        	}
        	else //Else, set the value at the index located exactly one row below (next level of computation/depth down) to the sum of the current entry and the entry located 2^i to the left on the current level of computation.
        	{
            	finishedarray[((i+1) * n) + j] = finishedarray[(i * n) + j] + finishedarray[(i * n) + (j-two_i)];
        	}
    	}
	}

	return NULL;
}

//Print the prefix sum
void* printPrefixSum()
{
	//Obtain the last row of finishedarray since this is where the finished prefix sum calculation is stored.
	int row = ((int) ceil(log2(n)));

	//For each entry in the last row of the prefix sum calculation, print the entry.
	for(int i = 0; i < n; i++)
	{
    	printf("%d\n", finishedarray[(row * n) + i]); //In the 2D finishedarray, access the corresponding entry by referencing a quantity containing the current row multipled by the length of that row, and then adding i (how far into the current level of computation/row) the current entry is located.
    	fflush(stdout);
	}

	return NULL;
}

int main(int argc, char* argv[])
{
  char* filename;

  //If 4 arguments are entered on the command line, then store 3 of those command line arguments in filename, n, and numthreads.
  if(argc == 4)
  {
  	//argv[0] is a pointer to the name of the program being run.
  	filename = argv[1];//filename pointer
  	n = atoi(argv[2]); //size of the input vector = number of lines in the file
  	numthreads = atoi(argv[3]); //Third argument specifies number of threads to use for computing the solution
  	//printf("Num threads %d\n", numthreads);
  	//printf("Num lines in file %d\n", n);
  }
  else //If 4 command line arguments are not entered on the command line, then exit with EXIT_FAILURE.
  {
  	exit(EXIT_FAILURE);
  }

  if(n < 2) //If there are less than two numbers for the prefix sum, then we cannot compute, and so we need to exit with EXIT_FAILURE.
  {
 	exit(EXIT_FAILURE);
  }

  pthread_t threadlist[numthreads];

  //Read in the contents (integers in this case) of filename into input in the read_input_vector function
  input = malloc(sizeof(int) * n);
  read_input_vector(filename, n, input);

  //Initialize finishedarray as a 2D array of integers:
  finishedarray = malloc(sizeof(int) * n * (1 + ((int) ceil(log2(n))))); //Convert log2(n) to an integer equivalent using ceil() and an integer cast

  //Initialize the mutex lock:
  pthread_mutex_init(&lock, NULL);

  //Set the maximum number of threads to the number of threads read in on the command line:
  maxnumthreads = numthreads;

  //Initialize condition variable cond1:
  if(pthread_cond_init(&cond1, NULL) != 0)
  {
  	perror("pthread_cond_init() error");
  	exit(1);
  }

  //Create each thread from threadlist using a loop, and send each thread to the calculate prefixsum function with a void pointer to args:
  for(int x = 0; x < numthreads; x++)
  {
	struct args_t *args = malloc(sizeof(struct args_t)); //Create a new args_t struct and allocate memory for it on the heap
	args->numth = numthreads; //Set the numth field to the number of threads
	args->start = x; //Set the start index field to the current value of x in the iteration

	int result = pthread_create(&threadlist[x], NULL, calculateprefixsum, (void*) args); //pthread_create only takes a (void*) argument for arguments to the calculateprefixsum function, so the struct type is casted to a void pointer.
	
	//If the thread cannot be created, then output this message to the terminal
	if(result != 0)
	{
    	printf("\nThread cannot be created : [%s]", strerror(result));
	}
  }

  //Join each thread to allow the main function to continue execution once all threads are finished with their individual executions of the Hillis Steele Algorithm.
  for(int y = 0; y < numthreads; y++)
  {
  	pthread_join(threadlist[y], NULL); 
  }

  //Destroy the mutex lock since all threads created to compute parts of the prefix_sum calculation have terminated:
  pthread_mutex_destroy(&lock);

  //Print the Prefix Sum:
  printPrefixSum();

  //Destroy the condition variable
  if(pthread_cond_destroy(&cond1) != 0)
  {
  	perror("pthread_cond_destroy() error");
  	exit(2);
  }

  //Memory management; free input and finishedarray from the heap
  free(input);
  free(finishedarray);
  return EXIT_SUCCESS;
}
