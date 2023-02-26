#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include<sys/time.h>
#include <string.h>
#include <sys/shm.h>

#define MICRO_SEC_IN_SEC 1000000

static int algo(int n){
	if(n % 2 == 0){
		return n / 2;
	}else{
		return 3*n + 1;
	}
}

int main(int argc, char **argv)
{
	int input = 0;
	while (input<1){
		printf("Enter a positive integer: ");
		scanf("%d", &input);	
	}

	int c[] = {input, input*2, input*3};

	int *s;
	int shmid;

	pid_t pid;
	struct timeval start, end;

	for (int i=0; i<3; i++){
		pid = fork();

		if(pid<0){
			printf("Error");
			exit(1);	
		}else if (pid == 0){
			printf("\nChild Process: working with Collatz function %d\n", c[i]);
			
			shmid = shmget((key_t)1234, 3*sizeof(int), IPC_CREAT);
			if (shmid == -1) {
				fprintf(stderr, "write shmget failed\n");
				exit(EXIT_FAILURE);
			}

			s = (int *)shmat(shmid, 0, 0);
			if(s == (void *)-1){
				fprintf(stderr, "write shmat failed\n");
				exit(EXIT_FAILURE);	
			}

			gettimeofday(&start, NULL);

			int counter = 0;
			int response = c[i];
			printf("Response: %d, ", response);
			while(response != 1){
				response = algo(response);
				printf("%d, ", response);
				counter++;
			}
			printf("\nSteps for input %d: %d", c[i], counter);
			s[i] = counter;


			printf("\nWriting to memory succesful\n");
			shmdt((void *) s);

			gettimeofday(&end, NULL);
			printf("\nElapsed time for child process with input %d: %ld micro sec \n\n\n",  c[i], ((end.tv_sec * MICRO_SEC_IN_SEC + end.tv_usec) - (start.tv_sec * MICRO_SEC_IN_SEC + start.tv_usec)));

			exit(EXIT_SUCCESS);
		}
	}

	for(int i=0; i<3; i++){
		int status = 0;
		pid_t childpid = wait(&status);
	}
	
	shmid = shmget((key_t)1234, 3*sizeof(int), IPC_EXCL);
	if (shmid == -1) {
		fprintf(stderr, "read shmget failed\n");
		exit(EXIT_FAILURE);
	}

	s = shmat(shmid, 0, SHM_RDONLY);
	if(s == (void *)-1){
		fprintf(stderr, "read shmat failed\n");
		exit(EXIT_FAILURE);	
	}
	
	printf("\nRead from memory succesful\n");

	int stepsSum = 0;
	int currentMin = 0;
	int currentMax = 0;
	for(int i =0; i<3; i++){
		if(s[i] < currentMin || currentMin == 0){
			currentMin = s[i];			
		}
		if(s[i] > currentMax || currentMax == 0){
			currentMax = s[i];
		}
		stepsSum += s[i];
	}
	printf("\nMinimum Steps: %d\n", currentMin);
	printf("\nMaximum Steps: %d\n", currentMax);
	printf("\nAverage Steps: %d\n", stepsSum / 3);

	shmdt((void *) s);

	return(0);
}
