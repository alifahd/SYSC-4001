#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "filter_com.h"
#include "semun.h"

static int set_semvalue(int);
static void del_semvalue(int);
static int semaphore_p(int);
static int semaphore_v(int);

static int sem_id;

static int set_semvalue(int num)
{
	union semun sem_union;
	sem_union.val = 1;
	if (semctl(sem_id, num, SETVAL, sem_union) == -1)
	{
		return 0;
	}
	return(1);
}

static void del_semvalue(int num)
{
	union semun sem_union;
	if (semctl(sem_id, num, IPC_RMID, sem_union) == -1)
	{
		fprintf(stderr, "Failed to delete semaphore\n");
	}
}

static int semaphore_p(int num)
{
	struct sembuf sem_b;
	sem_b.sem_num = num;
	sem_b.sem_op = -1;
	sem_b.sem_flg = SEM_UNDO;
	if (semop(sem_id, &sem_b, 1) == -1)
	{
		fprintf(stderr, "semaphore_p failed\n");
		return(0);
	}
	return(1);
}

static int semaphore_v(int num)
{
	struct sembuf sem_b;
	sem_b.sem_num = num;
	sem_b.sem_op = 1;
	sem_b.sem_flg = SEM_UNDO;
	if (semop(sem_id, &sem_b, 1) == -1)
	{
		fprintf(stderr, "semaphore_v failed\n");
		return(0);
	}
	return(1);
}

/*
	swaps elements in the array at the specified indexes
*/
void swapElems(struct sharedArr *arr, int index1, int index2){
	int temp = arr->shared[index1];
	arr->shared[index1] = arr->shared[index2];
	arr->shared[index2] = temp;
}

/*
	determines if two elements should be swapped
	return true if ele1 is a digit and ele2 is a letter otherwise false
*/
bool checkSwap(char ele1, char ele2){
	if(isdigit(ele1) && isalpha(ele2)){
		return true;
	}else if((isdigit(ele1) && isdigit(ele2))){
		if(ele1 > ele2){
			return true;
		}
	}else if((isalpha(ele1) && isalpha(ele2))){
		if(ele1 > ele2){
			return true;
		}
	}
	return false;
}

/*
	loop through array and comapre elements
	if there is a digit then a letter -> return false otherwise return true
*/
bool checkSorted(struct sharedArr *arr){
	for(int i = 1; i < 7; i++){
		if (checkSwap(arr->shared[i-1], arr->shared[i])){
			return false;
		}
	}
	return true;
}

/*
	print an array
*/
void printArray(char inputArr[]){
	printf("\n");
	for(int i = 0; i < 7; i++){
		printf("%c ", inputArr[i]);
	}
	printf("\n");
}

/*
	if debug is on, print the message
*/
void printMessage(bool debug, char msg[]){
	if(debug){
		printf("\n%s", msg);
	}
}

int main(){
	char input = '!';
	char str[20];
	char inputArr[7];
	bool debug;
	bool sorted = false;
	int P1, P2, P3;
	int shmid;
	
	
	//ask user if they want to run the program in debug mode
	printf("Should debug mode be used? (y/n): ");	
	scanf("%s", str);

	if(strcmp(str, "y") == 0){
		debug = true;
	}else{
		debug = false;
	}
	
	//put input in array
	for(int i = 0; i < 7; i++){ //loop 7 times
		printf("Enter a distict alphanumeric character:");
		while(!isalnum(input)){
			input = getc(stdin);
		}
		inputArr[i] = input;
		input = '!';//resets to non-alphanumeric
	}
	printArray(inputArr);//prints input array
	
	void *sharedMemory = (void *)0; //create array in shared memory
	
	struct sharedArr *filterArray;
	
	//memory
	printMessage(debug, "creating array in shared memory");
	shmid = shmget((key_t)2156, sizeof(char[7]), 0666 | IPC_CREAT); //create memory
	if (shmid == -1) {
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}
	sharedMemory = shmat(shmid, (void *)0, 0);
	if (sharedMemory == (void *)-1) {
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}
	
	filterArray = (struct sharedArr *)sharedMemory;//put array in memory

	for(int j = 0; j < 7; j++){// fill array in memory with input values
		filterArray->shared[j] = inputArr[j];
	}
	printArray(filterArray->shared);
	
	//create 2 semaphores
	//one for P1/P2 critical section
	//one for P2/P3 critical section
	sem_id = semget((key_t)1234, 2, 0666 | IPC_CREAT);
	if (!set_semvalue(0) || !set_semvalue(1))
	{
		fprintf(stderr, "Failed to initialize semaphores\n");
		exit(EXIT_FAILURE);
	}
	
	//fork 3 child processes
	P1 = fork();
	P2 = fork();
	P3 = fork();

	while(!sorted){
		if ((P1 == -1) || (P2 == -1) || (P3 == -1))
		{
			perror("FAILED TO FORK");
			exit(EXIT_FAILURE);
		}
		else if(P1 > 0){
			if(checkSwap(filterArray->shared[0], filterArray->shared[1])){
				swapElems(filterArray, 0, 1);
				printMessage(debug, "Process P1: performed swapping");
				printArray(filterArray->shared);
			}else{
				printMessage(debug, "Process P1: No swapping");
			}
			
			//critical section sorting
			if (!semaphore_p(0)){
				exit(EXIT_FAILURE);
			}			
			if(checkSwap(filterArray->shared[1], filterArray->shared[2])){
				swapElems(filterArray, 1, 2);
				printMessage(debug, "Process P1: performed swapping");
				printArray(filterArray->shared);
			}else{
				printMessage(debug, "Process P1: No swapping");
			}
			if (!semaphore_v(0)){ //signal semaphore 1
				exit(EXIT_FAILURE);
			}
		}
		//all of process two is critical since it shares with semaphore 1 and seampahore 2
		else if(P2 > 0){
			//waiting for semaphore 1
			if (!semaphore_p(0)){
				exit(EXIT_FAILURE);
			}
			if(checkSwap(filterArray->shared[2], filterArray->shared[3])){
				swapElems(filterArray, 2, 3);
				printMessage(debug, "Process P2: performed swapping");
				printArray(filterArray->shared);
			}else{
				printMessage(debug, "Process P2: No swapping");
			}
			//signal semaphore 1
			if (!semaphore_v(0)){
				exit(EXIT_FAILURE);
			}
			
			//waiting for semaphore 2
			if (!semaphore_p(1)){
				exit(EXIT_FAILURE);
			}
			if(checkSwap(filterArray->shared[3], filterArray->shared[4])){
				swapElems(filterArray, 3, 4);
				printMessage(debug, "Process P2: performed swapping");
				printArray(filterArray->shared);
			}else{
				printMessage(debug, "Process P2: No swapping");
			}
			//signal semaphore 2
			if (!semaphore_v(1)){
				exit(EXIT_FAILURE);
			}
		}
		else if(P3 > 0){
			//waiting for semaphore 2
			if (!semaphore_p(1)){
				exit(EXIT_FAILURE);
			}			
			if(checkSwap(filterArray->shared[4], filterArray->shared[5])){
				swapElems(filterArray, 4, 5);
				printMessage(debug, "Process P3: performed swapping");
				printArray(filterArray->shared);
			}else{
				printMessage(debug, "Process P3: No swapping");
			}			
			//signal semaphore 2
			if (!semaphore_v(1)){
				exit(EXIT_FAILURE);
			}
			
			if(checkSwap(filterArray->shared[5], filterArray->shared[6])){
				swapElems(filterArray, 5, 6);
				printMessage(debug, "Process P3: performed swapping");
				printArray(filterArray->shared);
			} else{
				printMessage(debug, "Process P3: No swapping");
			}
		}
		sorted = checkSorted(filterArray);
	}
	
	printArray(filterArray->shared);

	//detach and delete shared memory
	if(shmdt(sharedMemory) == -1){
		fprintf(stderr, "shmdt failed\n");
		exit(EXIT_FAILURE);
	}
	//delete semaphores
	//del_semvalue(0);
	//del_semvalue(1);
	exit(EXIT_SUCCESS);	
}

