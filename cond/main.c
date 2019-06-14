#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

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
bool *next_cut;
bool finished = false;
volatile int being_cut = -1;

pthread_cond_t *call_customer;
pthread_cond_t wake_barber;
pthread_cond_t empty_chair;

pthread_mutex_t *mutex_next;
pthread_mutex_t mutex_queue;
pthread_mutex_t mutex_print;
pthread_mutex_t mutex_rejected_number;

void WaitTime(int time)
{
	int x = (rand()%time) * (rand()%1000000) + 1000000;
	usleep(x);
}

void WriteRejected()
{
	printf("\n----------------Customers that did not enter waiting room: ");
	struct List *temp = rejected;
	while(temp != NULL)
	{
		printf("%d ", temp -> customer_id);
		temp = temp -> next;
	}
	printf("\n\n");
}

void WriteWaiting()
{
	printf("\n---------------Customers that are waiting in waiting room: ");
	struct List *temp = queue;
	while(temp != NULL)
	{
		printf("%d ", temp -> customer_id);
		temp = temp -> next;
	}
	printf("\n\n");
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
	pthread_mutex_lock(&mutex_print);
	printf("Res: %d WRoom: %d/%d [in: %d] - place in waiting room has been taken.\n",
		rejected_number, queue_length, chairs_number, being_cut);
	pthread_mutex_unlock(&mutex_print);

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
		pthread_cond_broadcast(&wake_barber);
		pthread_mutex_unlock(&mutex_queue);

		pthread_mutex_lock(&mutex_next[id]);
		while (next_cut[id] != true)
		{
			pthread_cond_wait(&call_customer[id], &mutex_next[id]);
		}
		pthread_mutex_unlock(&mutex_next[id]);
	}
	else
	{
		pthread_mutex_unlock(&mutex_queue);
		pthread_mutex_lock(&mutex_rejected_number);
		rejected_number++;
		pthread_mutex_unlock(&mutex_rejected_number);
		pthread_mutex_lock(&mutex_print);
		printf("Res: %d WRoom: %d/%d [in: %d] - customer did not enter.\n",
			rejected_number, queue_length, chairs_number, being_cut);
		if(debug == true)
		{
			PlaceNextRejected(id);
		}
		pthread_mutex_unlock(&mutex_print);
	}
}

void *Barber()
{
	int id;
	while(finished == false)
	{
		pthread_mutex_lock(&mutex_queue);
		while (queue_length <= 0 && finished == false)
		{
			pthread_cond_wait(&wake_barber, &mutex_queue);
		}
		if(finished == false)
		{
			queue_length--;
			id = PopWaiting();
			pthread_mutex_unlock(&mutex_queue);
			pthread_mutex_lock(&mutex_next[id]);
			next_cut[id] = true;
			pthread_cond_broadcast(&call_customer[id]);
			pthread_mutex_unlock(&mutex_next[id]);
			pthread_mutex_lock(&mutex_print);
			being_cut = id;
			printf("Res: %d WRoom: %d/%d [in: %d] - starting haircutting.\n",
				rejected_number, queue_length, chairs_number, being_cut);
			pthread_mutex_unlock(&mutex_print);

			// WaitTime(haircut_time);

			pthread_mutex_lock(&mutex_print);
			printf("Res: %d WRoom: %d/%d [in: %d] - haircut finished.\n",
				rejected_number, queue_length, chairs_number, being_cut);
			pthread_mutex_unlock(&mutex_print);
		}
		else
		{
			pthread_mutex_unlock(&mutex_queue);
			printf("No more customers.\n");
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
		{"chair", required_argument, NULL, 'r'},
		{"time_c", required_argument, NULL, 'c'},
		{"time_b", required_argument, NULL, 'b'},
		{"debug", no_argument, NULL, 'd'}
	};
	int customers_number = 20;
	int choice = 0;
	int error;
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
	if(customersThreads == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		return 0;
	}
	pthread_t barberThread;

	int *array = (int *)malloc(sizeof(int) * customers_number);
	if(array == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		return 0;
	}
	next_cut = (bool *)malloc(sizeof(bool) * customers_number);
	if(next_cut == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		return 0;
	}
    call_customer = (pthread_cond_t *)malloc(sizeof(pthread_cond_t) * customers_number);
	if(call_customer == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		return 0;
	}
	mutex_next = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * customers_number);
	if(mutex_next == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		return 0;
	}
	int i;
	for(i=0; i<customers_number; i++)
	{
		array[i] = i;
		next_cut[i] = false;
		error = pthread_cond_init(&call_customer[i], NULL);
  		if(error != 0)
		{
			printf("%s %d\n", "Conditional variable initialization error:", error);
			return 0;
		}
		error = pthread_mutex_init(&mutex_next[i], NULL);
		if(error != 0)
		{
			printf("%s %d\n", "Conditional variable initialization error:", error);
			return 0;
		}
	}

	error = pthread_cond_init(&wake_barber, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Conditional variable initialization error:", error);
		return 0;
	}
	error = pthread_cond_init(&empty_chair, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Conditional variable initialization error:", error);
		return 0;
	}
	error = pthread_mutex_init(&mutex_queue, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex initialization error:", error);
		return 0;
	}
	// error = pthread_mutex_init(&mutex_next, NULL);
	// if(error != 0)
	// {
	// 	printf("%s %d\n", "Mutex initialization error:", error);
	// 	return 0;
	// }
	error = pthread_mutex_init(&mutex_print, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex initialization error:", error);
		return 0;
	}
	error = pthread_mutex_init(&mutex_rejected_number, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex initialization error:", error);
		return 0;
	}

	error = pthread_create(&barberThread, NULL, Barber, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Thread creation error:", error);
		return 0;
	}

	sleep(1);

	for(i=0; i<customers_number; ++i)
	{
		error = pthread_create(&customersThreads[i], NULL, Customer, (void *)&array[i]);
		if(error != 0)
		{
			printf("%s %d\n", "Thread creation error:", error);
			return 0;
		}
	}
	for(i=0; i<customers_number; ++i)
	{
		error = pthread_join(customersThreads[i], NULL);
		if(error != 0)
		{
			printf("%s %d\n", "Thread join error:", error);
			return 0;
		}
	}
	finished = true;

	// signaling the barber so they can check variable finished and go home
	pthread_mutex_lock(&mutex_queue);
	pthread_cond_broadcast(&wake_barber);
	pthread_mutex_unlock(&mutex_queue);

	error = pthread_join(barberThread, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Thread join error:", error);
		return 0;
	}
	error = pthread_mutex_destroy(&mutex_queue);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex destruction error:", error);
		return 0;
	}
	error = pthread_mutex_destroy(&mutex_print);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex destruction error:", error);
		return 0;
	}
	error = pthread_mutex_destroy(&mutex_rejected_number);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex destruction error:", error);
		return 0;
	}
    for(i=0; i<customers_number; ++i)
    {
        error = pthread_cond_destroy(&call_customer[i]);
		if(error != 0)
		{
			printf("%s %d\n", "Conditional variable destruction error:", error);
			return 0;
		}
		error = pthread_mutex_destroy(&mutex_next[i]);
		if(error != 0)
		{
			printf("%s %d\n", "Mutex destruction error:", error);
			return 0;
		}
    }
	error = pthread_cond_destroy(&wake_barber);
	if(error != 0)
	{
		printf("%s %d\n", "Conditional variable destruction error:", error);
		return 0;
	}
	error = pthread_cond_destroy(&empty_chair);
	if(error != 0)
	{
		printf("%s %d\n", "Conditional variable destruction error:", error);
		return 0;
	}
	free(rejected);
	free(queue);
	return 0;
}
