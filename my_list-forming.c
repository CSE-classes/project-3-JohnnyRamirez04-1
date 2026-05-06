/*
  my_list-forming.c

  Optimized version of list-forming.c.

  Two changes drive the speedup:

  1) Each thread builds its own LOCAL list of K nodes without touching
     any shared state. After the local list is complete it is spliced
     onto the global list in a single critical section. Each thread
     therefore acquires the mutex exactly once instead of K times,
     which collapses lock-acquisition overhead and contention from
     O(num_threads * K) down to O(num_threads).

  2) The original program uses pthread_mutex_trylock inside a tight
     while(1) loop. When a thread cannot acquire the lock it spins,
     burning a CPU and starving the lock holder of cycles. We use
     pthread_mutex_lock so a contending thread sleeps until the lock
     is released; with O(num_threads) acquisitions instead of
     O(num_threads * K) the wait path is barely exercised at all.

  Compile:
      gcc my_list-forming.c -o my_list-forming -lpthread -D_GNU_SOURCE
  Run:
      ./my_list-forming <num_threads>
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sched.h>

#define K 200 /* generate a data node K times in each thread */

struct Node
{
    int data;
    struct Node *next;
};

struct list
{
    struct Node *header;
    struct Node *tail;
};

pthread_mutex_t mutex_lock;
struct list *List;

void bind_thread_to_cpu(int cpuid) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask)) {
        fprintf(stderr, "sched_setaffinity");
        exit(EXIT_FAILURE);
    }
}

struct Node* generate_data_node()
{
    struct Node *ptr = (struct Node *)malloc(sizeof(struct Node));
    if (NULL != ptr) {
        ptr->next = NULL;
    } else {
        printf("Node allocation failed!\n");
    }
    return ptr;
}

void *producer_thread(void *arg)
{
    bind_thread_to_cpu(*((int*)arg));

    struct Node *local_head = NULL;
    struct Node *local_tail = NULL;
    int counter = 0;

    /* build a local list of K nodes with no locking */
    while (counter < K) {
        struct Node *ptr = generate_data_node();
        if (NULL != ptr) {
            ptr->data = 1;
            if (local_head == NULL) {
                local_head = local_tail = ptr;
            } else {
                local_tail->next = ptr;
                local_tail = ptr;
            }
        }
        ++counter;
    }

    /* splice the local list onto the global list in one critical section */
    if (local_head != NULL) {
        pthread_mutex_lock(&mutex_lock);
        if (List->header == NULL) {
            List->header = local_head;
            List->tail   = local_tail;
        } else {
            List->tail->next = local_head;
            List->tail       = local_tail;
        }
        pthread_mutex_unlock(&mutex_lock);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int i, num_threads;
    int NUM_PROCS;
    int *cpu_array = NULL;
    struct Node *tmp, *next;
    struct timeval starttime, endtime;

    if (argc < 2) {
        printf("Usage: %s <num_threads>\n", argv[0]);
        return 1;
    }

    num_threads = atoi(argv[1]);
    pthread_t producer[num_threads];

    NUM_PROCS = sysconf(_SC_NPROCESSORS_CONF);
    if (NUM_PROCS > 0) {
        cpu_array = (int *)malloc(NUM_PROCS * sizeof(int));
        if (cpu_array == NULL) {
            printf("Allocation failed!\n");
            exit(0);
        }
        for (i = 0; i < NUM_PROCS; i++)
            cpu_array[i] = i;
    }

    pthread_mutex_init(&mutex_lock, NULL);

    List = (struct list *)malloc(sizeof(struct list));
    if (NULL == List) {
        printf("End here\n");
        exit(0);
    }
    List->header = List->tail = NULL;

    gettimeofday(&starttime, NULL);

    for (i = 0; i < num_threads; i++) {
        pthread_create(&(producer[i]), NULL, producer_thread,
                       &cpu_array[i % NUM_PROCS]);
    }

    for (i = 0; i < num_threads; i++) {
        if (producer[i] != 0) {
            pthread_join(producer[i], NULL);
        }
    }

    gettimeofday(&endtime, NULL);

    /* free the global list */
    if (List->header != NULL) {
        next = tmp = List->header;
        while (tmp != NULL) {
            next = tmp->next;
            free(tmp);
            tmp = next;
        }
    }
    free(List);

    if (cpu_array != NULL)
        free(cpu_array);

    pthread_mutex_destroy(&mutex_lock);

    printf("Total run time is %ld microseconds.\n",
           (endtime.tv_sec - starttime.tv_sec) * 1000000 +
           (endtime.tv_usec - starttime.tv_usec));
    return 0;
}
