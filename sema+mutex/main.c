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

sem_t customersReadyToHaircut; // 0 - no one in waiting room, >0 - number of customers waiting for haircut
sem_t isBarberFree; // 0 - busy, 1 - free
pthread_mutex_t chair; // locks chair every time when customer is getting his haircut, frees upon finishing
pthread_mutex_t waitingRoom; // lock waiting room to protect from races

int chairs = 10; // number of free chairs in waiting room
int waitingRoomSize = 10; // number of all chairs in waiting room
int peopleRejected = 0; // number of ppl who didn't find place in waiting room
int haircutTime = 2; // time of single haircut (1s --> haircutTime)
int customerTime = 8; // after customerTime, customer enters waiting room (1 --> customerTime)
bool debug = false; // boolean variable which allows to write lists
bool finished = false; // boolean variable which determine if barber finished his work
int activeCustomer = -1; // variable which contains active customer's id, -1 if no one is active


void WaitTime(int time)
{
	int x = (rand()%time) * (rand()%1000000) + 1000000;
	usleep(x);
}

struct List *rejected = NULL;
struct List *waiting = NULL;

void WriteRejected()
{
	struct List *temp = rejected;
	printf("Customers that did not enter waiting room: ");
	while(temp!=NULL)
	{
		printf("%d ", temp->customer_id);
		temp = temp->next;
	}
	printf("\n");
}

void WriteWaiting()
{
	struct List *temp = waiting;
	printf("Customers that are waiting in waiting room: ");
	while(temp!=NULL)
	{
		printf("%d ", temp->customer_id);
		temp = temp->next;
	}
	printf("\n");
}

void PlaceNextRejected(int id)
{
	struct List *temp = (struct List*)malloc(sizeof(struct List));
	temp->customer_id = id;
	temp->next = rejected;
	rejected = temp;
	WriteRejected();
}

void PlaceNextWaiting(int id)
{
	struct List *temp = (struct List*)malloc(sizeof(struct List));
	temp->customer_id = id;
	temp->next = waiting;
	waiting = temp;
	WriteWaiting();
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
	WriteWaiting();
}

void *Customer (void *customer_id)
{
	WaitTime(customerTime);
	int id = *(int*)customer_id;
	pthread_mutex_lock(&waitingRoom);
	if(chairs>0)
	{
		chairs--;
		printf("Res: %d WRomm: %d/%d [in: %d] - place in waiting room has been taken.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		if(debug == true)
		{
			PlaceNextWaiting(id);
		}
		sem_post(&customersReadyToHaircut); // signal for barber that customer is in waiting waitingRoom
		pthread_mutex_unlock(&waitingRoom);
		sem_wait(&isBarberFree); // waiting for finishing current haircut
		pthread_mutex_lock(&chair); // next customer on the chair
		activeCustomer = id;
		printf("Res: %d WRomm: %d/%d [in: %d] - starting haircutting.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		if(debug == true)
		{
			RemoveCustomer(id);
		}
	}
	else
	{
		pthread_mutex_unlock(&waitingRoom);
		peopleRejected++;
		printf("Res: %d WRomm: %d/%d [in: %d] - customer did not enter.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		if(debug==true)
		{
			PlaceNextRejected(id);
		}
	}
}

void *Barber()
{
	while(!finished)
	{
		if(!finished)
		{
			sem_wait(&customersReadyToHaircut);
			pthread_mutex_lock(&waitingRoom);
			chairs++;
			pthread_mutex_unlock(&waitingRoom);
			sem_post(&isBarberFree);
			WaitTime(haircutTime);
			printf("Res: %d WRomm: %d/%d [in: %d] - haircut finished.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
			pthread_mutex_unlock(&chair);
		}
	}
	printf("Barber is going to his home.\n");
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	static struct option parameters[] =
	{
		{"customer", required_argument, NULL, 'k'},
		{"chairs", required_argument, NULL, 'r'},
		{"time_c", required_argument, NULL, 'c'},
		{"time_b", required_argument, NULL, 'b'},
		{"debug", no_argument, NULL, 'd'}
	};
	int numberOfCustomers = 20;
	int choice = 0;
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
	pthread_t *customersThreads = malloc(sizeof(pthread_t)*numberOfCustomers);
	pthread_t barberThread;
	int *array = malloc(sizeof(int)*numberOfCustomers);
	int i;
	for(i=0; i<numberOfCustomers; i++)
	{
		array[i] = i;
	}
	sem_init(&customersReadyToHaircut,0,0);
	sem_init(&isBarberFree,0,0);

	pthread_mutex_init(&chair, NULL);
	pthread_mutex_init(&waitingRoom, NULL);
	pthread_create(&barberThread, NULL, Barber, NULL);

	for(i=0; i<numberOfCustomers; ++i)
	{
		pthread_create(&customersThreads[i], NULL, Customer, (void *)&array[i]);
	}
	for(i=0; i<numberOfCustomers; ++i)
	{
		pthread_join(customersThreads[i], NULL);
	}
	finished = true;
	pthread_join(barberThread, NULL);
	pthread_mutex_destroy(&chair);
	pthread_mutex_destroy(&waitingRoom);
	sem_destroy(&customersReadyToHaircut);
	sem_destroy(&isBarberFree);
	free(rejected);
	free(waiting);
	return 0;
}
