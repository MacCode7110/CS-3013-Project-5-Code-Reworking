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
int *inputpreviouslevelcopy;
int n = 0; //shared resource among all threads
int numthreads = 0; //shared resource among all threads
int threadcounter = 0;
pthread_mutex_t lock;
sem_t s1;
sem_t s2;
//Debugging Variables:
int* val1 = 0;
int* val2 = 0;
int val22 = 0;
int val11 = 0;

//Global Condition Variables:
int maxnumthreads;

//Global Prefix Sum Algorithm Variables
int prefixsumparallelcalccomplete = 0; //Used to tell a thread when the prefix sum calculation is complete
double numsteps = 0; //Used to keep track of the number of individual sums

//Global Hillis Steele Algorithm Variable:
int addacrossinterval = 1;

struct args_t
{
    int startindex;
    int endindex;
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

//Calculate the prefix sum, which contains the critical section
void* calculateprefixsum(void* args)
{
	struct args_t* structcopy = (struct args_t*) args;
	int startindexcopy = (structcopy)->startindex;
	int endindexcopy = (structcopy)->endindex;

	//printf("%d", endindex);
	//Two-Phase Barrier: wait for all the threads to arrive and for all the threads to execute the critical section

	//Barrier 1
	pthread_mutex_lock(&lock);
	threadcounter+= 1;

	if(threadcounter == maxnumthreads)
	{
    	sem_wait(&s2); //The value of the semaphore decrements by 1 regardless of if the thread waits at it
    	sem_post(&s1); //Thread 2 will post to s1, the value of the semaphore increments by 1
	}

	pthread_mutex_unlock(&lock);

	sem_wait(&s1); //Thread 1 stops/waits because s1's initial value is less than or equal to 0
	sem_post(&s1); //The thread that goes through always frees the one behind it

	//Critical Section
	if(prefixsumparallelcalccomplete == 0)
	{
		//Determine if the prefix sum calculation has finished by seeing if the number of steps = log2(number of numbers to sum)
	    if(numsteps == log2(n))
		{
	    	prefixsumparallelcalccomplete = 1;
		}
	    else if((startindexcopy + addacrossinterval) > (n - 1))
        {
        	//Update variables for next level of computation
        	addacrossinterval = addacrossinterval * 2;

        	for(int x = 0; x < n; x++)
        	{
        		inputpreviouslevelcopy[x] = input[x];
        	}
        }
        else
        {
        	input[endindexcopy] = inputpreviouslevelcopy[startindexcopy] + inputpreviouslevelcopy[endindexcopy];
        	numsteps++;
        }
	}

	//Barrier 2
	pthread_mutex_lock(&lock);
	threadcounter -= 1;

	if(threadcounter == 0)
	{
    	sem_wait(&s1); //s1's value is 1 when thread 1 goes through, so thread 2 does not wait here
    	sem_post(&s2);
	}

	pthread_mutex_unlock(&lock);

	sem_wait(&s2); //s2's value gets decremented to 0 by thread 1. s2's value gets decremented to 0 again by thread 2
	sem_post(&s2); //s2's value gets incremented to 1 by thread 1. s2's value gets incremented to 1 again by thread 2

	return NULL;
}

//Print the prefix sum
void* printPrefixSum()
{
	for(int i = 0; i < n; i++)
	{
    	printf("%d\n", input[i]);
    	fflush(stdout);
	}

	return NULL;
}

int main(int argc, char* argv[])
{
  char* filename;
  int startindex = 0;
  int endindex = addacrossinterval;

  if(argc == 4)
  {
  	//argv[0] is a pointer to the name of the program being run.
  	filename = argv[1];//filename pointer
  	n = atoi(argv[2]); //size of the input vector = number of lines in the file
  	//Getting a seg fault right above
  	numthreads = atoi(argv[3]); //Third argument specifies number of threads to use for computing the solution
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
  //Calculate the prefix sum using a version of the Hillis and Steele Algorithm

  //Initialize the mutex lock and semaphores:
  pthread_mutex_init(&lock, NULL);
  sem_init(&s1, 0, 0);
  sem_init(&s2, 0, 2);

  //Set the condition variable to the maximum number of threads:
  maxnumthreads = numthreads;

  //Initialize the inputpreviouslevelcopy:
  inputpreviouslevelcopy = malloc(sizeof(int) * n);

  for(int z = 0; z < n; z++)
  {
	  inputpreviouslevelcopy[z] = input[z];
  }

  //Create each thread using a loop:
  for(int x = 0; x < numthreads; x++)
  {
	struct args_t *args = malloc(sizeof(struct args_t));
	args->startindex = startindex;
	args->endindex = endindex;

 	int result = pthread_create(&threadlist[x], NULL, calculateprefixsum, (void*) args); //pthread_create only takes a (void*) argument for arguments to the calculateprefixsum function.

 	if(result != 0)
 	{
 		printf("\nThread cannot be created : [%s]", strerror(result));
 	}

 	free(args);

 	startindex+=1;
 	endindex+=1;
  }

  //Join each thread to allow the main function to continue execution once all threads are finished with their individual executions of the critical section.
  for(int y = 0; y < numthreads; y++)
  {
  	pthread_join(threadlist[y], NULL); //Join does not affect the deadlock
  }

  //Destroy the mutex lock since all threads created for the prefix_sum calculation have terminated:
  pthread_mutex_destroy(&lock);

  //Print the Prefix Sum:
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, printPrefixSum, NULL);

  //Block the calling thread until thread_id terminates. Then, the calling thread can resume execution.
  pthread_join(thread_id, NULL);

  free(inputpreviouslevelcopy);
  free(input);
  return EXIT_SUCCESS;
}
