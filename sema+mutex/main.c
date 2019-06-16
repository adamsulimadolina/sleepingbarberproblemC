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

sem_t customer; // post when customer comes to waiting room
sem_t barber; // post when barber finishes haircut
pthread_mutex_t waitingRoom; // lock waiting room to protect from races
int error;
int customersCounter=0; // customers Counter
int chairs = 10; // number of free chairs in waiting room
int waitingRoomSize = 10; // number of all chairs in waiting room
int peopleRejected = 0; // number of peiple who didn't find place in waiting room
bool debug = false; // boolean variable which allows to write lists
bool finished = false; // boolean variable which determine if barber finished his work
int activeCustomer = -1; // variable which contains active customer's id, -1 if no one is active

struct List *rejected = NULL; // list of customers that did not enter waiting room
struct List *waiting = NULL; // list of customers that entered waiting room

void WriteList(int choice) //function that prints list of customers/rejected people
{
	struct List *temp = NULL;
	if(choice == 0)
	{
		printf("\nCustomers that did not enter waiting room: ");
		temp = rejected;
	}
	if(choice == 1)
	{
		printf("\nCustomers that are waiting in the waiting room: ");
		temp = waiting;
	}
	while(temp!=NULL)
	{
		printf("%d ", temp->customer_id);
		temp = temp->next;
	}
	printf("\n\n");
}

void PlaceNextRejected(int id) // places next rejected person on the list
{
	if(debug==true)
	{
		struct List *temp = (struct List*)malloc(sizeof(struct List));
		if(temp == NULL)
		{
			perror("Can't allocate memory for PlaceNextRejected");
			exit(EXIT_FAILURE);
		}
		temp->customer_id = id;
		temp->next = rejected;
		rejected = temp;
		WriteList(0);
	}
}

void PlaceNextWaiting(int id) // places next person in waiting room
{
	struct List *temp = (struct List*)malloc(sizeof(struct List));
	if(temp == NULL)
	{
		perror("Can't allocate memory for PlaceNextWaiting");
		exit(EXIT_FAILURE);
	}
	temp->customer_id = id;
	temp->next = waiting;
	waiting = temp;
	if(debug==true)WriteList(1);
}

void RemoveCustomer(int id) // removes customer from waiting room, he is getting haircut
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
				temp = NULL;
				free(temp);
			}
			else
			{
				pop->next = temp->next;
				temp = NULL;
				free(temp);
			}
			break;
		}
		pop = temp;
		temp = temp->next;
	}
	if(debug==true)WriteList(1);
}

int Top() // returns id of actual customer
{
	struct List *tmp = waiting;
	while(tmp->next!=NULL)
	{
		tmp=tmp->next;
	}
	return tmp->customer_id;
}
void WaitTime(int time)
{
	int x = (rand()%time) * (rand()%1000000) + 1000000;
	usleep(x);
}

void *Customer (void *customer_id)
{
	//WaitTime(6);
	int id = *(int*)customer_id;
	error = pthread_mutex_lock(&waitingRoom); // waiting room lock
	if(error!=0)
	{
		perror("EXIT -> Error with locking waiting room");
		exit(EXIT_FAILURE);
	}
	if(chairs>0)
	{
		chairs--;
		printf("Res: %d WRoom: %d/%d [in: %d] - place in waiting room has been taken.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		PlaceNextWaiting(id); // adding customer to queue
		error = sem_post(&customer); // signal for barber that he have customer
		if(error!=0)
		{
			perror("EXIT -> Error with posting customer semaphore");
			exit(EXIT_FAILURE);
		}
		error = pthread_mutex_unlock(&waitingRoom); // waiting room unlock
		if(error!=0)
		{
			perror("EXIT -> Error with unlocking waiting room");
			exit(EXIT_FAILURE);
		}

		error = sem_wait(&barber); // waiting for signal from barber that he finished haircut
		if(error!=0)
		{
			perror("EXIT -> Error with waiting for barber semaphore");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		peopleRejected++;
		printf("Res: %d WRoom: %d/%d [in: %d] - customer did not enter.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
		PlaceNextRejected(id); // adding person to rejected list
		error = pthread_mutex_unlock(&waitingRoom); // waiting room unlock
		if(error!=0)
		{
			perror("EXIT -> Error with unlocking waiting room");
			exit(EXIT_FAILURE);
		}
	}
}

void *Barber()
{
	while(!finished) // while he did not finished job for today
	{
		error = sem_wait(&customer); // waiting for signal, that he has ready customer
		if(error!=0)
		{
			perror("EXIT -> Error with waiting for customer semaphore");
			exit(EXIT_FAILURE);
		}
		if(!finished){
			error = pthread_mutex_lock(&waitingRoom); // waiting room lock
			if(error!=0)
			{
				perror("EXIT -> Error with locking waiting room");
				exit(EXIT_FAILURE);
			}
			activeCustomer = Top(); // setting active customer to first from top of the queue
			customersCounter++;
			chairs++; // frees one of the waiting room chairs
			printf("Res: %d WRoom: %d/%d [in: %d] - starting haircut.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
			RemoveCustomer(activeCustomer); // removing customer from waiting room queue
			sem_post(&barber); // signal that he finished haircut
			if(error!=0)
			{
				perror("EXIT -> Error with posting barber semaphore");
				exit(EXIT_FAILURE);
			}
			pthread_mutex_unlock(&waitingRoom); // waiting room unlock
			if(error!=0)
			{
				perror("EXIT -> Error with unlocking waiting room");
				exit(EXIT_FAILURE);
			}
			//WaitTime(4);
			error = pthread_mutex_lock(&waitingRoom); // waiting room lock
			if(error!=0)
			{
				perror("EXIT -> Error with locking waiting room");
				exit(EXIT_FAILURE);
			}
			printf("Res: %d WRoom: %d/%d [in: %d] - haircut finished.\n", peopleRejected, waitingRoomSize-chairs, waitingRoomSize, activeCustomer);
			activeCustomer = -1;
			pthread_mutex_unlock(&waitingRoom); // waiting room unlock
			if(error!=0)
			{
				perror("EXIT -> Error with unlocking waiting room");
				exit(EXIT_FAILURE);
			}


		}
		else
		{
			printf("\nBarber is going to his home. \nToday he earned some money, because he did haircut for %d people.\n",customersCounter);
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
		{"debug", no_argument, NULL, 'd'}
	};

	while((choice = getopt_long(argc, argv, "k:f:d",parameters,NULL)) != -1)
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

	int i, k;
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
	status = pthread_mutex_destroy(&waitingRoom);
	if(status != 0)
	{
		perror("\n\nEXIT -> Can't destroy waiting room mutex");
		exit(EXIT_FAILURE);
	}
	status = sem_destroy(&customer);
	if(status != 0)
	{
		perror("\n\nEXIT -> Can't destroy customer semaphore");
		exit(EXIT_FAILURE);
	}
	status = sem_destroy(&barber);
	if(status != 0)
	{
		perror("\n\nEXIT -> Can't destroy barber semaphore");
		exit(EXIT_FAILURE);
	}
	free(waiting);
	free(rejected);
	return 0;
}
