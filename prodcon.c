#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <memory.h>
#include <pthread.h>
#include <threads.h>

#include "prodcon.h"


struct llist_node {
    struct llist_node *next;
    char *str;
};

/**
 * pop a node off the start of the list.
 *
 * @param phead the head of the list. this will be modified by the call unless the list is empty
 * (*phead == NULL).
 * @return NULL if list is empty or a pointer to the string at the top of the list. the caller is
 * incharge of calling free() on the pointer when finished with the string.
 */
char *pop(struct llist_node **phead)
{
    if (*phead == NULL) {
        return NULL;
    }
    char *s = (*phead)->str;
    struct llist_node *next = (*phead)->next;
    free(*phead);
    *phead = next;
    return s;
}

/**
 * push a node onto the start of the list. a copy of the string will be made.
 * @param phead the head of the list. this will be modified by this call to point to the new node
 * being added for the string.
 * @param s the string to add. a copy of the string will be made and placed at the beginning of
 * the list.
 */
void push(struct llist_node **phead, const char *s)
{
    struct llist_node *new_node = malloc(sizeof(*new_node));
    new_node->next = *phead;
    new_node->str = strdup(s);
    *phead = new_node;
}

// the array of list heads, locks, and conds. the size should be equal to the number of consumers
static struct llist_node **heads;
static pthread_mutex_t *locks;
static pthread_cond_t *conds;

static pthread_t *producers;
static pthread_t *consumers;
static int finished_producing;

static assign_consumer_f assign_consumer;
static int producer_count;
static int consumer_count;

static run_producer_f run_producer;
static run_consumer_f run_consumer;

struct thread_args {
    int id;
    int argc;
    char **argv;
};

thread_local int my_consumer_number;

void queue(int consumer, const char *str)
{
    push(&heads[consumer], str);
}

void produce(const char *buffer)
{
    int hash = assign_consumer(consumer_count, buffer) % consumer_count;

    pthread_mutex_lock(&locks[hash]);

    queue(hash, buffer);
    pthread_cond_signal(&conds[hash]);

    pthread_mutex_unlock(&locks[hash]);
}

char *consume()
{
    pthread_mutex_lock(&locks[my_consumer_number]);

    while (!finished_producing && heads[my_consumer_number] == NULL) {
        pthread_cond_wait(&conds[my_consumer_number], &locks[my_consumer_number]);
    }

    char *str = pop(&heads[my_consumer_number]);

    pthread_mutex_unlock(&locks[my_consumer_number]);
    return str;
}

void* start_producer(void *args_void_p)
{
    struct thread_args *args_p = (struct thread_args*) args_void_p;
    run_producer(args_p->id, producer_count, produce, args_p->argc, args_p->argv);
}

void* start_consumer(void *args_void_p)
{
    struct thread_args *args_p = (struct thread_args*) args_void_p;

    my_consumer_number = args_p->id;
    run_consumer(args_p->id, consume, args_p->argc, args_p->argv);
}

void do_usage(char *prog)
{
    printf("USAGE: %s shared_lib consumer_count producer_count ....\n", prog);
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        do_usage(argv[0]);
    }

    char *shared_lib = argv[1];
    consumer_count = strtol(argv[2], NULL, 10);
    producer_count = strtol(argv[3], NULL, 10);

    char **new_argv = &argv[4];
    int new_argc = argc - 4;
    setlinebuf(stdout);

    if (consumer_count <= 0 || producer_count <= 0) {
        do_usage(argv[0]);
    }

    void *dh = dlopen(shared_lib, RTLD_LAZY);

    // load the producer, consumer, and assignment functions from the library
    run_producer = dlsym(dh, "run_producer");
    run_consumer = dlsym(dh, "run_consumer");
    assign_consumer = dlsym(dh, "assign_consumer");
    if (run_producer == NULL || run_consumer == NULL || assign_consumer == NULL) {
        printf("Error loading functions: prod %p cons %p assign %p\n", run_producer,
                run_consumer, assign_consumer);
        exit(2);
    }


    // initialize
    heads = calloc(consumer_count, sizeof(*heads));
    locks = calloc(consumer_count, sizeof(*locks));
    conds = calloc(consumer_count, sizeof(*conds));

    producers = malloc(producer_count * sizeof(*producers));
    consumers = malloc(consumer_count * sizeof(*consumers));
    finished_producing = 0;
    

    // startup producers and consumers
    struct thread_args **producer_args = malloc(producer_count * sizeof(*producer_args));
    for (int i = 0; i < producer_count; i++) {
        struct thread_args *args_p = malloc(sizeof(*args_p));
        args_p->id = i;
        args_p->argc = new_argc;
        args_p->argv = new_argv;
        producer_args[i] = args_p;
    }
    for (int i = 0; i < producer_count; i++) {        
        pthread_create(&producers[i], NULL, &start_producer, producer_args[i]);
    }

    struct thread_args **consumer_args = malloc(consumer_count * sizeof(*consumer_args));
    for (int i = 0; i < consumer_count; i++) {
        struct thread_args *args_p = malloc(sizeof(*args_p));
        args_p->id = i;
        args_p->argc = new_argc;
        args_p->argv = new_argv;
        consumer_args[i] = args_p;
    }
    for (int i = 0; i < consumer_count; i++) {
        pthread_create(&consumers[i], NULL, &start_consumer, consumer_args[i]);
    }


    // wait for producers to finish
    for (int i = 0; i < producer_count; i++) {
        void *retVal;
        switch (pthread_join(producers[i], &retVal)) {
            case 0: break;
            default:
                printf("Error in pthread_join on producer %d\n", i);
                exit(3);
        }
    }

    // send sentinal to consumers
    finished_producing = 1;

    // wait for consumers to finish
    for (int i = 0; i < consumer_count; i++) {
        pthread_cond_signal(&conds[i]);
        void *retVal;
        switch (pthread_join(consumers[i], &retVal)) {
            case 0: break;
            default:
                printf("Error in pthread_join on consumer %d\n", i);
                exit(4);
        }
    }

    return 0;
}