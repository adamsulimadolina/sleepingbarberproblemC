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

struct List *rejected = NULL;
struct List *queue = NULL;

volatile int queue_length = 0;
int chairs_number = 10;
int customer_time = 8;
int haircut_time = 2;
int rejected_number = 0;
bool debug = false;
bool finished = false;
volatile int being_cut = -1;

sem_t *call_customer;
sem_t wake_barber;
pthread_mutex_t mutex_customer;
pthread_mutex_t mutex_queue;

void WaitTime(int time)
{
	int x = (rand()%time) * (rand()%1000000) + 1000000;
	usleep(x);
}

void WriteRejected()
{
	printf("Customers that did not enter waiting room: ");
	struct List *temp = rejected;
	while(temp != NULL)
	{
		printf("%d ", temp -> customer_id);
		temp = temp -> next;
	}
	printf("\n");
}

void WriteWaiting()
{
	printf("Customers that are waiting in waiting room: ");
	struct List *temp = queue;
	while(temp != NULL)
	{
		printf("%d ", temp -> customer_id);
		temp = temp -> next;
	}
	printf("\n");
}

void PlaceNextRejected(int id)
{
	struct List *new = (struct List*)malloc(sizeof(struct List));
	new -> customer_id = id;
	new -> next = NULL;
	struct List *temp = rejected;
	if(temp == NULL)
	{
		rejected = new;
	}
	else
	{
		while(temp -> next != NULL)
		{
			temp = temp -> next;
		}
		temp -> next = new;
	}
    WriteRejected();
}

void PlaceNextWaiting(int id)
{
	struct List *new = (struct List*)malloc(sizeof(struct List));
	new -> customer_id = id;
	new -> next = NULL;
	struct List *temp = queue;
	if(temp == NULL)
	{
		queue = new;
	}
	else
	{
		while(temp -> next != NULL)
		{
			temp = temp -> next;
		}
		temp -> next = new;
	}
	printf("Res: %d WRoom: %d/%d [in: %d] - place in waiting room has been taken.\n",
		rejected_number, queue_length, chairs_number, being_cut);
	if(debug == true)
	{
		WriteWaiting();
	}
}

int PopWaiting()
{
    struct List *temp = queue;
    if(temp == NULL)
    {
        return -1;
    }
    int id = temp -> customer_id;
	queue = temp -> next;
    free(temp);
	return id;
}

void *Customer (void *customer_id)
{
	// WaitTime(customer_time);
	int id = *(int*)customer_id;
	pthread_mutex_lock(&mutex_queue);
	if(queue_length<chairs_number)
	{
		queue_length++;
		PlaceNextWaiting(id);
		pthread_mutex_unlock(&mutex_queue);
		sem_post(&wake_barber);
		sem_wait(&call_customer[id]);
	}
	else
	{
		rejected_number++;
		printf("Res: %d WRoom: %d/%d [in: %d] - customer did not enter.\n",
			rejected_number, queue_length, chairs_number, being_cut);
		if(debug == true)
		{
			PlaceNextRejected(id);
		}
		pthread_mutex_unlock(&mutex_queue);
	}
}

void *Barber()
{
	int id;
	while(finished == false)
	{
		sem_wait(&wake_barber);
		pthread_mutex_lock(&mutex_queue);
		queue_length--;
		id = PopWaiting();
		being_cut = id;
		printf("Res: %d WRoom: %d/%d [in: %d] - starting haircutting.\n",
			rejected_number, queue_length, chairs_number, being_cut);
		pthread_mutex_unlock(&mutex_queue);
		sem_post(&call_customer[id]);
		// WaitTime(haircut_time);
		pthread_mutex_lock(&mutex_queue);
		printf("Res: %d WRoom: %d/%d [in: %d] - haircut finished.\n",
			rejected_number, queue_length, chairs_number, being_cut);
		pthread_mutex_unlock(&mutex_queue);
	}
	printf("Barber is going to his home.\n");
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	static struct option parameters[] =
	{
		{"customer", required_argument, NULL, 'k'},
		{"chair", required_argument, NULL, 'r'},
		{"time_c", required_argument, NULL, 'c'},
		{"time_b", required_argument, NULL, 'b'},
		{"debug", no_argument, NULL, 'd'}
	};
	int customers_number = 20;
	int choice = 0;
	while((choice = getopt_long(argc, argv, "k:r:c:f:d",parameters,NULL)) != -1)
	{
		switch(choice)
		{
			case 'k': // number of customers
						customers_number = atoi(optarg);
						break;
			case 'r': // number of chairs in waiting room
						chairs_number = atoi(optarg);
						break;
			case 'c': // frequency of appending new customer
						customer_time = atoi(optarg);
						break;
			case 'b': // time of single haircut
						haircut_time = atoi(optarg);
						break;
			case 'd':
						debug=true;
						break;
		}
	}

	pthread_t *customersThreads = (pthread_t *)malloc(sizeof(pthread_t) * customers_number);
	pthread_t barberThread;

	int *array = (int *)malloc(sizeof(int) * customers_number);
    call_customer = (sem_t *)malloc(sizeof(sem_t) * customers_number);

	int i;
	for(i=0; i<customers_number; i++)
	{
		array[i] = i;
        sem_init(&call_customer[i], 0, 0);
	}

	sem_init(&wake_barber, 0, 0);

	pthread_mutex_init(&mutex_customer, NULL);
	pthread_mutex_init(&mutex_queue, NULL);

	pthread_create(&barberThread, NULL, Barber, NULL);

	for(i=0; i<customers_number; ++i)
	{
		pthread_create(&customersThreads[i], NULL, Customer, (void *)&array[i]);
	}

	for(i=0; i<customers_number; ++i)
	{
		pthread_join(customersThreads[i], NULL);
	}

	finished = true;
	pthread_join(barberThread, NULL);
	pthread_mutex_destroy(&mutex_customer);
	pthread_mutex_destroy(&mutex_queue);
    for(i=0; i<customers_number; ++i)
    {
        sem_destroy(&call_customer[i]);
    }
	sem_destroy(&wake_barber);
	free(rejected);
	free(queue);
	return 0;
}
