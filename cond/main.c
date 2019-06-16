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

int error;

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
	if(new == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		exit(EXIT_FAILURE);
	}
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
	if(new == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		exit(EXIT_FAILURE);
	}
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
	// czas przed wejściem klienta do salonu fryzjerskiego
	WaitTime(customer_time);
	int id = *(int*)customer_id;
	error = pthread_mutex_lock(&mutex_queue);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex lock error:", error);
		exit(EXIT_FAILURE);
	}
	if(queue_length<chairs_number)
	{
		queue_length++;
		PlaceNextWaiting(id);
		// klient zajął miejsce i wysyła sygnał fryzjerowi, że jest gotowy na strzyżenie
		error = pthread_cond_broadcast(&wake_barber);
		if(error != 0)
		{
			printf("%s %d\n", "Conditional variable broadcast error:", error);
			exit(EXIT_FAILURE);
		}
		error = pthread_mutex_unlock(&mutex_queue);
		if(error != 0)
		{
			printf("%s %d\n", "Mutex unlock error:", error);
			exit(EXIT_FAILURE);
		}

		error = pthread_mutex_lock(&mutex_next[id]);
		if(error != 0)
		{
			printf("%s %d\n", "Mutex lock error:", error);
			exit(EXIT_FAILURE);
		}
		while (next_cut[id] != true)
		{
			// klient czeka na wezwanie na fotel fryzjerski
			pthread_cond_wait(&call_customer[id], &mutex_next[id]);
		}
		error = pthread_mutex_unlock(&mutex_next[id]);
		if(error != 0)
		{
			printf("%s %d\n", "Mutex unlock error:", error);
			exit(EXIT_FAILURE);
		}
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
		error = pthread_mutex_unlock(&mutex_queue);
		if(error != 0)
		{
			printf("%s %d\n", "Mutex unlock error:", error);
			exit(EXIT_FAILURE);
		}
	}
}

void *Barber()
{
	int id;
	while(finished == false)
	{
		error = pthread_mutex_lock(&mutex_queue);
		if(error != 0)
		{
			printf("%s %d\n", "Mutex lock error:", error);
			exit(EXIT_FAILURE);
		}
		while (queue_length <= 0 && finished == false)
		{
			pthread_cond_wait(&wake_barber, &mutex_queue);
			// fryzjer został obudzony
		}
		if(finished == false)
		{
			queue_length--;
			id = PopWaiting();
			error = pthread_mutex_lock(&mutex_next[id]);
			if(error != 0)
			{
				printf("%s %d\n", "Mutex lock error:", error);
				exit(EXIT_FAILURE);
			}
			next_cut[id] = true;
			// fryzjer wzywa pierwszego klienta z kolejki na fotel
			error = pthread_cond_broadcast(&call_customer[id]);
			if(error != 0)
			{
				printf("%s %d\n", "Conditional variable broadcast error:", error);
				exit(EXIT_FAILURE);
			}
			error = pthread_mutex_unlock(&mutex_next[id]);
			if(error != 0)
			{
				printf("%s %d\n", "Mutex unlock error:", error);
				exit(EXIT_FAILURE);
			}
			being_cut = id;
			printf("Res: %d WRoom: %d/%d [in: %d] - starting haircutting.\n",
				rejected_number, queue_length, chairs_number, being_cut);
			if(debug == true)
			{
				WriteWaiting();
			}
			error = pthread_mutex_unlock(&mutex_queue);
			if(error != 0)
			{
				printf("%s %d\n", "Mutex unlock error:", error);
				exit(EXIT_FAILURE);
			}

			// w tym momencie trwa strzyżenie
			WaitTime(haircut_time);

			error = pthread_mutex_lock(&mutex_print);
			if(error != 0)
			{
				printf("%s %d\n", "Mutex lock error:", error);
				exit(EXIT_FAILURE);
			}
			printf("Res: %d WRoom: %d/%d [in: %d] - haircut finished.\n",
				rejected_number, queue_length, chairs_number, being_cut);
			being_cut = -1;
			error = pthread_mutex_unlock(&mutex_print);
			if(error != 0)
			{
				printf("%s %d\n", "Mutex unlock error:", error);
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			error = pthread_mutex_unlock(&mutex_queue);
			if(error != 0)
			{
				printf("%s %d\n", "Mutex unlock error:", error);
				exit(EXIT_FAILURE);
			}
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
		exit(EXIT_FAILURE);
	}
	pthread_t barberThread;

	int *array = (int *)malloc(sizeof(int) * customers_number);
	if(array == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		exit(EXIT_FAILURE);
	}
	next_cut = (bool *)malloc(sizeof(bool) * customers_number);
	if(next_cut == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		exit(EXIT_FAILURE);
	}
    call_customer = (pthread_cond_t *)malloc(sizeof(pthread_cond_t) * customers_number);
	if(call_customer == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		exit(EXIT_FAILURE);
	}
	mutex_next = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * customers_number);
	if(mutex_next == NULL)
	{
		printf("%s %s\n", "Malloc error:", strerror(errno));
		exit(EXIT_FAILURE);
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
			exit(EXIT_FAILURE);
		}
		error = pthread_mutex_init(&mutex_next[i], NULL);
		if(error != 0)
		{
			printf("%s %d\n", "Conditional variable initialization error:", error);
			exit(EXIT_FAILURE);
		}
	}

	error = pthread_cond_init(&wake_barber, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Conditional variable initialization error:", error);
		exit(EXIT_FAILURE);
	}
	error = pthread_cond_init(&empty_chair, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Conditional variable initialization error:", error);
		exit(EXIT_FAILURE);
	}
	error = pthread_mutex_init(&mutex_queue, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex initialization error:", error);
		exit(EXIT_FAILURE);
	}
	// error = pthread_mutex_init(&mutex_next, NULL);
	// if(error != 0)
	// {
	// 	printf("%s %d\n", "Mutex initialization error:", error);
	// 	exit(EXIT_FAILURE);
	// }
	error = pthread_mutex_init(&mutex_print, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex initialization error:", error);
		exit(EXIT_FAILURE);
	}

	error = pthread_create(&barberThread, NULL, Barber, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Thread creation error:", error);
		exit(EXIT_FAILURE);
	}

	for(i=0; i<customers_number; ++i)
	{
		error = pthread_create(&customersThreads[i], NULL, Customer, (void *)&array[i]);
		if(error != 0)
		{
			printf("%s %d\n", "Thread creation error:", error);
			exit(EXIT_FAILURE);
		}
	}
	for(i=0; i<customers_number; ++i)
	{
		error = pthread_join(customersThreads[i], NULL);
		if(error != 0)
		{
			printf("%s %d\n", "Thread join error:", error);
			exit(EXIT_FAILURE);
		}
	}
	finished = true;

	// signaling the barber so they can check variable finished and go home

	error = pthread_mutex_lock(&mutex_queue);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex lock error:", error);
		exit(EXIT_FAILURE);
	}
	error = pthread_cond_broadcast(&wake_barber);
	if(error != 0)
	{
		printf("%s %d\n", "Conditional variable broadcast error:", error);
		exit(EXIT_FAILURE);
	}
	error = pthread_mutex_unlock(&mutex_queue);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex unlock error:", error);
		exit(EXIT_FAILURE);
	}

	error = pthread_join(barberThread, NULL);
	if(error != 0)
	{
		printf("%s %d\n", "Thread join error:", error);
		exit(EXIT_FAILURE);
	}
	error = pthread_mutex_destroy(&mutex_queue);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex destruction error:", error);
		exit(EXIT_FAILURE);
	}
	error = pthread_mutex_destroy(&mutex_print);
	if(error != 0)
	{
		printf("%s %d\n", "Mutex destruction error:", error);
		exit(EXIT_FAILURE);
	}

    for(i=0; i<customers_number; ++i)
    {
        error = pthread_cond_destroy(&call_customer[i]);
		if(error != 0)
		{
			printf("%s %d\n", "Conditional variable destruction error:", error);
			exit(EXIT_FAILURE);
		}
		error = pthread_mutex_destroy(&mutex_next[i]);
		if(error != 0)
		{
			printf("%s %d\n", "Mutex destruction error:", error);
			exit(EXIT_FAILURE);
		}
    }
	error = pthread_cond_destroy(&wake_barber);
	if(error != 0)
	{
		printf("%s %d\n", "Conditional variable destruction error:", error);
		exit(EXIT_FAILURE);
	}
	error = pthread_cond_destroy(&empty_chair);
	if(error != 0)
	{
		printf("%s %d\n", "Conditional variable destruction error:", error);
		exit(EXIT_FAILURE);
	}
	free(rejected);
	free(queue);
	exit(EXIT_SUCCESS);
}
