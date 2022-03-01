
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
int threadcounter = 0;
int numsummationsperthread = 0;
int maxnumthreads = 0;
pthread_mutex_t lock;

//Global Condition Variables:
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;

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

void barrier()
{
	//Critical section below prevents the likelihood of a race condition from occurring
	//printf("Waiting for mutex\n");
	pthread_mutex_lock(&lock);
	threadcounter+=1;
	//printf("Threadcounter is %d\n", threadcounter);
	if(threadcounter < maxnumthreads)
	{
    	pthread_cond_wait(&cond1, &lock); //When a thread waits on condition variable 1, it unlocks the mutex so that other threads can then enter and increment the thread counter
	}
	else if(threadcounter == maxnumthreads)
	{
    	threadcounter = 0;
    	pthread_cond_broadcast(&cond1); //Signals all threads blocked by condition 1 to become unblocked and continue execution. This only happens when the last thread arrives.
	}

	pthread_mutex_unlock(&lock);
}

//Calculate the prefix sum, which contains the critical section
void* calculateprefixsum(void* args)
{
	//Algorithm is not locked so that all threads can enter the prefix sum calculation at a similar time and allow for concurrent computations to occur.

	struct args_t* structcopy = (struct args_t*) args;
	int start = (structcopy)->start;
	int numth = (structcopy)->numth;
	//printf("Thread %d starting\n", start);

	for(int k = start; k < n; k+=numth)
	{
    	finishedarray[k] = input[k];
	}

	//barrier();

	for(int i = 0; i < (log2(n)); i++)
	{
    	barrier();

    	int two_i = 1 << i;

    	for(int j = start; j < n; j+=numth)
    	{
        	if(j < two_i)
        	{
            	finishedarray[((i + 1) * n) + j] = finishedarray[(i * n) + j];
        	}
        	else
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
	int row = ((int) ceil(log2(n)));

	for(int i = 0; i < n; i++)
	{
    	printf("%d\n", finishedarray[(row * n) + i]);
    	fflush(stdout);
	}

	return NULL;
}

int main(int argc, char* argv[])
{
  char* filename;

  if(argc == 4)
  {
  	//argv[0] is a pointer to the name of the program being run.
  	filename = argv[1];//filename pointer
  	n = atoi(argv[2]); //size of the input vector = number of lines in the file
  	//Getting a seg fault right above
  	numthreads = atoi(argv[3]); //Third argument specifies number of threads to use for computing the solution
  	//printf("Num threads %d\n", numthreads);
  	//printf("Num lines in file %d\n", n);
  }
  else
  {
  	exit(EXIT_FAILURE);
  }

  if(n < 2) //If there are less than two numbers for the prefix sum, then we cannot compute, and so we need to exit with EXIT_FAILURE.
  {
 	exit(EXIT_FAILURE);
  }

  pthread_t threadlist[numthreads];

  //Read in the contents (integers in this case) of the first argument file in the read_input_vector function
  input = malloc(sizeof(int) * n);
  read_input_vector(filename, n, input);

  //Initialize finishedarray:
  finishedarray = malloc(sizeof(int) * n * (1 + ((int) ceil(log2(n)))));

  //Calculate the prefix sum using a version of the Hillis and Steele Algorithm

  //Initialize the mutex lock and semaphores:
  pthread_mutex_init(&lock, NULL);

  //Set the condition variable to the maximum number of threads:
  maxnumthreads = numthreads;

  //Calculate the number of summations per thread
  numsummationsperthread = (int) (n / maxnumthreads) + (n % maxnumthreads == 0 ? 0 : 1); //? is a tertiary operator, if (n % maxnumthreads == 0) evaluates to true, then add 0, else add 1.

  //Initialize condition variables:
  if(pthread_cond_init(&cond1, NULL) != 0)
  {
  	perror("pthread_cond_init() error");
  	exit(1);
  }

  //Create each thread using a loop:
  for(int x = 0; x < numthreads; x++)
  {
	struct args_t *args = malloc(sizeof(struct args_t));
	args->numth = numthreads;
	args->start = x;

	int result = pthread_create(&threadlist[x], NULL, calculateprefixsum, (void*) args); //pthread_create only takes a (void*) argument for arguments to the calculateprefixsum function.

	if(result != 0)
	{
    	printf("\nThread cannot be created : [%s]", strerror(result));
	}

	//free(args);
  }

  //Join each thread to allow the main function to continue execution once all threads are finished with their individual executions of the critical section.
  for(int y = 0; y < numthreads; y++)
  {
  	pthread_join(threadlist[y], NULL); //Join does not affect the deadlock
  }

  //Destroy the mutex lock since all threads created for the prefix_sum calculation have terminated:
  pthread_mutex_destroy(&lock);

  //Print the Prefix Sum:
  printPrefixSum();

  //Destroy the condition variable
  if(pthread_cond_destroy(&cond1) != 0)
  {
  	perror("pthread_cond_destroy() error");
  	exit(2);
  }

  free(input);
  free(finishedarray);
  return EXIT_SUCCESS;
}
