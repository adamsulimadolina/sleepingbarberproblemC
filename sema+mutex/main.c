#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>

struct List
{
	int customer_id;
	struct List *next;
};

sem_t customer; // 0 - no one in waiting room, >0 - number of customers waiting for haircut
sem_t barber; // 0 - busy, 1 - free
//pthread_mutex_t chair; // locks chair every time when customer is getting his haircut, frees upon finishing
pthread_mutex_t waitingRoom; // lock waiting room to protect from races

int tmp=0;
int chairs = 10; // number of free chairs in waiting room
int waitingRoomSize = 10; // number of all chairs in waiting room
int peopleRejected = 0; // number of ppl who didn't find place in waiting room
int haircutTime = 2; // time of single haircut (1s --> haircutTime)
int customerTime = 8; // after customerTime, customer enters waiting room (1 --> customerTime)
bool debug = false; // boolean variable which allows to write lists
bool finished = false; // boolean variable which determine if barber finished his work
int activeCustomer = -1; // variable which contains active customer's id, -1 if no one is active


struct List *rejected = NULL;
struct List *waiting = NULL;

void WriteRejected()
{
	struct List *temp = rejected;
	printf("\nCustomers that did not enter waiting room: ");
	while(temp!=NULL)
	{
		printf("%d ", temp->customer_id);
		temp = temp->next;
	}
	printf("\n\n");
}

void WriteWaiting()
{
	struct List *temp = waiting;
	printf("\nCustomers that are waiting in waiting room: ");
	while(temp!=NULL)
	{
		printf("%d ", temp->customer_id);
		temp = temp->next;
	}
	printf("\n\n");
}

void PlaceNextRejected(int id)
{
	struct List *temp = (struct List*)malloc(sizeof(struct List));
	temp->customer_id = id;
	temp->next = rejected;
	rejected = temp;
	if(debug==true)WriteRejected();
}

void PlaceNextWaiting(int id)
{
	struct List *temp = (struct List*)malloc(sizeof(struct List));
	temp->customer_id = id;
	temp->next = waiting;
	waiting = temp;
	if(debug==true)WriteWaiting();
}

void RemoveCustomer(int id)
{
	struct List *temp = waiting;
	struct List *pop = waiting;
	while(temp!=NULL)
	{
		if(temp->customer_id==id)
		{
			if(temp->customer_id == waiting->customer_id)
			{
				waiting = waiting->next;
				free(temp);
			}
			else
			{
				pop->next = temp->next;
				free(temp);
			}
			break;
		}
		pop = temp;
		temp = temp->next;
	}
	if(debug==true)WriteWaiting();
}

int Top()
{
	struct List *tmp = waiting;
	while(tmp->next!=NULL)
	{
		tmp=tmp->next;
	}
	return tmp->customer_id;
}

void *Customer (void *customer_id)
{
	int id = *(int*)customer_id;
	pthread_mutex_lock(&waitingRoom);
	if(chairs>0)
	{
		chairs--;
		printf("Res: %d WRoom: %d/%d [in: %d] - place in waiting room has been taken.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		PlaceNextWaiting(id);
		if(debug == true)
		{
			WriteWaiting();
		}
		sem_post(&customer);
		pthread_mutex_unlock(&waitingRoom);
		sem_wait(&barber);
	}
	else
	{
		peopleRejected++;
		printf("Res: %d WRoom: %d/%d [in: %d] - customer did not enter.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		PlaceNextRejected(id);
		pthread_mutex_unlock(&waitingRoom);
	}
}

void *Barber()
{
	while(!finished)
	{
		sem_wait(&customer);
		if(!finished){
			pthread_mutex_lock(&waitingRoom);
			activeCustomer = Top();
			tmp++;
			chairs++;
			printf("Res: %d WRoom: %d/%d [in: %d] - getting haircut.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
			sem_post(&barber);
			pthread_mutex_unlock(&waitingRoom);
			RemoveCustomer(activeCustomer);
		}
		else
		{
			printf("\nBarber is going to his home. \nToday he earned some money, because he did haircut for %d people.\n",tmp);
		}
	}
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	int status;
	int numberOfCustomers = 20;
	int choice = 0;
	pthread_t barberThread;
	pthread_t *customersThreads;
	int *array;

	static struct option parameters[] =
	{
		{"customer", required_argument, NULL, 'k'},
		{"chairs", required_argument, NULL, 'r'},
		{"time_c", required_argument, NULL, 'c'},
		{"time_b", required_argument, NULL, 'b'},
		{"debug", no_argument, NULL, 'd'}
	};

	while((choice = getopt_long(argc, argv, "k:r:c:f:d",parameters,NULL)) != -1)
	{
		switch(choice)
		{
			case 'k': // number of customers
						numberOfCustomers = atoi(optarg);
						break;
			case 'r': // number of chairs in waiting room
						chairs = atoi(optarg);
						waitingRoomSize = atoi(optarg);
						break;
			case 'c': // frequency of appending new customer
						customerTime = atoi(optarg);
						break;
			case 'b': // time of single haircut
						haircutTime = atoi(optarg);
						break;
			case 'd':
						debug=true;
						break;
		}
	}

	if((customersThreads = malloc(sizeof(pthread_t)*numberOfCustomers))==NULL)
	{
		perror("\n\nEXIT -> Can't allocate memory for customer threads");
		exit(EXIT_FAILURE);
	}


	if((array = malloc(sizeof(int)*numberOfCustomers))==NULL)
	{
		perror("\n\nEXIT -> Can't allocate memory for threads array.");
		exit(EXIT_FAILURE);
	}
	int i;
	for(i=0; i<numberOfCustomers; i++)
	{
		array[i] = i;
	}
	status = sem_init(&customer,0,0);
	if(status != 0)
	{
		perror("\n\nEXIT -> Can't create customer semaphore");
		exit(EXIT_FAILURE);
	}
	status = sem_init(&barber,0,0);
	if(status != 0)
	{
		perror("\n\nEXIT -> Can't create barber semaphore");
		exit(EXIT_FAILURE);
	}
	status = pthread_mutex_init(&waitingRoom, NULL);
	if(status != 0)
	{
		perror("\n\nEXIT -> Can't create waitingRoom mutex");
		exit(EXIT_FAILURE);
	}
	status = pthread_create(&barberThread, NULL, Barber, NULL);
	if(status != 0)
	{
		perror("\n\nEXIT -> Can't create barber thread");
		exit(EXIT_FAILURE);
	}
	for(i=0; i<numberOfCustomers; ++i)
	{

		status = pthread_create(&customersThreads[i], NULL, Customer, (void *)&array[i]);
		if(status != 0)
		{
			perror("\n\nEXIT -> Can't create customer thread");
			exit(EXIT_FAILURE);
		}
	}
	for(i=0; i<numberOfCustomers; ++i)
	{
		status = pthread_join(customersThreads[i], NULL);
		if(status != 0)
		{
			perror("\n\nEXIT -> Can't join customer thread");
			exit(EXIT_FAILURE);
		}
	}
	finished = true;
	sem_post(&customer);
	status = pthread_join(barberThread, NULL);
	if(status != 0)
	{
		perror("\n\nEXIT -> Can't join barber thread");
		exit(EXIT_FAILURE);
	}
	pthread_mutex_destroy(&waitingRoom);
	sem_destroy(&customer);
	sem_destroy(&barber);
	free(rejected);
	free(waiting);
	return 0;
}
