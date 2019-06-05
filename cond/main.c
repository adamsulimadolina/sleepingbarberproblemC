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

pthread_cond_t freeChairs; // free chairs in waiting room
pthread_cond_t wakeBarber; // signal to wake up barber
pthread_cond_t finishedHaircut; // signal to customer, that barber finished his haircut
pthread_cond_t isBarberFree; // variable that determines if barber is free at this moment
bool finished = false; // boolean variable that checks if every customer was haircuted
bool isChairTaken = false; // is someone on the barber's chair
bool isBarberSleep = false;
pthread_mutex_t chair; // locks chair every time when customer is getting his haircut, frees upon finishing
pthread_mutex_t waitingRoom; // lock waiting room to protect from races
pthread_mutex_t finishedCustomer; // customer after haircut
pthread_mutex_t barberState; // protects barber state

int chairs = 10; // number of free chairs in waiting room
int waitingRoomSize = 10; // number of all chairs in waiting room
int peopleRejected = 0; // number of ppl who didn't find place in waiting room
int haircutTime = 2; // time of single haircut (1s --> haircutTime)
int customerTime = 8; // after customerTime, customer enters waiting room (1 --> customerTime)
bool debug = false; // boolean variable which allows to write lists
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
		pthread_mutex_unlock(&waitingRoom);
		pthread_mutex_lock(&chair);
		if(isChairTaken)
		{
			pthread_cond_wait(&isBarberFree, &chair);
		}
		isChairTaken = true;
		pthread_mutex_unlock(&chair);
		pthread_mutex_lock(&waitingRoom);
		chairs++;
		printf("Res: %d WRomm: %d/%d [in: %d] - starting haircutting.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		activeCustomer = id;
		if(debug == true)
		{
			RemoveCustomer(id);
		}
		printf("Res: %d WRomm: %d/%d [in: %d] - current customer is getting his haircut.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		pthread_mutex_unlock(&waitingRoom); // next customer on the chair

		pthread_cond_signal(&wakeBarber);

		pthread_mutex_lock(&finishedCustomer);
		pthread_cond_wait(&finishedHaircut, &finishedCustomer);
		finished = false;
		pthread_mutex_unlock(&finishedCustomer);

		pthread_mutex_lock(&chair);
		isChairTaken = false;
		pthread_mutex_unlock(&chair);
		pthread_cond_signal(&isBarberFree);
	}
	else
	{
		peopleRejected++;
		printf("Res: %d WRomm: %d/%d [in: %d] - customer did not enter.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		if(debug==true)
		{
			PlaceNextRejected(id);
		}
		pthread_mutex_unlock(&waitingRoom);
	}
}

void *Barber()
{
	pthread_mutex_lock(&barberState);
	while(!finished)
	{
		pthread_cond_wait(&wakeBarber,&barberState);
		pthread_mutex_unlock(&barberState);
		if(!finished)
		{
			WaitTime(haircutTime);
			//printf("Res: %d WRomm: %d/%d [in: %d] - haircut finished.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
			pthread_cond_signal(&finishedHaircut);
		}
		else printf("No customers, time to go home...\n");
	}
	printf("Barber is going to his home.\n");
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	sstatic struct option parameters[] =
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

	pthread_cond_init(&freeChairs, NULL);
	pthread_cond_init(&isBarberFree, NULL);
	pthread_cond_init(&wakeBarber, NULL);
	pthread_cond_init(&finishedHaircut, NULL);

	pthread_mutex_init(&barberState, NULL);
	pthread_mutex_init(&finishedCustomer, NULL);
	pthread_mutex_init(&chair, NULL);
	pthread_mutex_init(&waitingRoom, NULL);

	pthread_create(&barberThread, 0, Barber, 0);

	for(i=0; i<numberOfCustomers; ++i)
	{
		pthread_create(&customersThreads[i], 0, Customer, (void *)&array[i]);
	}
	for(i=0; i<numberOfCustomers; ++i)
	{
		pthread_join(customersThreads[i], 0);
	}
	finished = true;
	pthread_cond_signal(&wakeBarber);
	pthread_join(barberThread, 0);

	pthread_cond_destroy(&freeChairs);
	pthread_cond_destroy(&isBarberFree);
	pthread_cond_destroy(&wakeBarber);
	pthread_cond_destroy(&finishedHaircut);

	pthread_mutex_destroy(&barberState);
	pthread_mutex_destroy(&finishedCustomer);
	pthread_mutex_destroy(&chair);
	pthread_mutex_destroy(&waitingRoom);

	free(rejected);
	free(waiting);
	return 0;
}
