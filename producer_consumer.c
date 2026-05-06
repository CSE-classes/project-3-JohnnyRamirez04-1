/*
  producer_consumer.c
  Classic producer-consumer using a circular buffer of 5 chars and
  condition variables. Producer reads characters from "message.txt"
  one by one and writes them into the buffer. Consumer pulls them
  out in order and prints them.
*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define BUFFER_SIZE 5

char buffer[BUFFER_SIZE];
int in = 0;          /* next slot for producer */
int out = 0;         /* next slot for consumer */
int count = 0;       /* number of items currently in buffer */
int producer_done = 0;

pthread_mutex_t mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  not_full  = PTHREAD_COND_INITIALIZER;
pthread_cond_t  not_empty = PTHREAD_COND_INITIALIZER;

void *producer(void *arg)
{
	FILE *fp = fopen("message.txt", "r");
	if (fp == NULL) {
		printf("ERROR: can't open message.txt!\n");
		pthread_exit(NULL);
	}

	int ch;
	while ((ch = fgetc(fp)) != EOF) {
		pthread_mutex_lock(&mutex);
		while (count == BUFFER_SIZE) {
			pthread_cond_wait(&not_full, &mutex);
		}
		buffer[in] = (char)ch;
		in = (in + 1) % BUFFER_SIZE;
		count++;
		pthread_cond_signal(&not_empty);
		pthread_mutex_unlock(&mutex);
	}

	fclose(fp);

	/* signal end of stream so consumer can drain and exit */
	pthread_mutex_lock(&mutex);
	producer_done = 1;
	pthread_cond_signal(&not_empty);
	pthread_mutex_unlock(&mutex);

	pthread_exit(NULL);
}

void *consumer(void *arg)
{
	while (1) {
		pthread_mutex_lock(&mutex);
		while (count == 0 && !producer_done) {
			pthread_cond_wait(&not_empty, &mutex);
		}
		if (count == 0 && producer_done) {
			pthread_mutex_unlock(&mutex);
			break;
		}
		char ch = buffer[out];
		out = (out + 1) % BUFFER_SIZE;
		count--;
		pthread_cond_signal(&not_full);
		pthread_mutex_unlock(&mutex);

		putchar(ch);
		fflush(stdout);
	}

	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	pthread_t prod_tid, cons_tid;

	pthread_create(&prod_tid, NULL, producer, NULL);
	pthread_create(&cons_tid, NULL, consumer, NULL);

	pthread_join(prod_tid, NULL);
	pthread_join(cons_tid, NULL);

	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&not_full);
	pthread_cond_destroy(&not_empty);

	printf("\n");
	return 0;
}
