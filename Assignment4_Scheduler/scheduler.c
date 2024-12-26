// Bryan Duong
// Edited date: 2024-11-22

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

#include "scheduler.h"
#include "worker.h"

/*
 * Define the extern global variables here.
 */

#define CLOCK CLOCK_REALTIME  // Calendar time as a value of time_t
#define QUANTUM 1             // Define quantum time in seconds

sem_t queue_sem;            /* semaphore for scheduler queue */
thread_info_list sched_queue; /* list of current workers */

static int quit = 0;
static timer_t timer;
static thread_info_t *currentThread = NULL;
static long wait_times = 0;
static long run_times = 0;
static int completed = 0;
static int thread_count = 0;

// Structure to specify when a timer expires
struct itimerspec timerspec;

// Structures to change the action taken by a process on receipt of a specific signal
struct sigaction saction1, saction2, saction3;

// Structure for notification from asynchronous routines
struct sigevent sevent;

static void exit_error(int); /* helper function */
static void wait_for_queue();

/*
 * Update the worker's current running time.
 * This function is called every time the thread is suspended.
 */
void update_run_time(thread_info_t *info) {
    struct timespec now;
    clock_gettime(CLOCK, &now);
    info->run_time += time_difference(&now, &info->resume_time);
    info->suspend_time = now;
}

/*
 * Update the worker's current waiting time.
 * This function is called every time the thread resumes.
 */
void update_wait_time(thread_info_t *info) {
    struct timespec now;
    clock_gettime(CLOCK, &now);
    info->wait_time += time_difference(&now, &info->suspend_time);
    info->resume_time = now; 
}

static void init_sched_queue(int queue_size) {
    // Notify via signal when timer expires
    memset(&sevent, 0, sizeof(struct sigevent));
    sevent.sigev_notify = SIGEV_SIGNAL;
    sevent.sigev_signo = SIGALRM;
    sevent.sigev_value.sival_ptr = &timer;

    // Initialize the timer
    if (timer_create(CLOCK, &sevent, &timer) == -1) {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    /* Set up a semaphore to restrict access to the queue */
    sem_init(&queue_sem, 0, queue_size);

    /* Initialize the scheduler queue */
    sched_queue.head = sched_queue.tail = NULL;
    pthread_mutex_init(&sched_queue.lock, NULL);
}

/*
 * Signal a worker thread that it can resume.
 */
static void resume_worker(thread_info_t *info) {
    printf("Scheduler: resuming %lu.\n", info->thrid);

    /*
     * Signal the worker thread that it can resume
     */
    pthread_kill(info->thrid, SIGUSR2);

    /* Update the wait time for the thread */
    update_wait_time(info);
}

/* Send a signal to the thread, telling it to kill itself */
void cancel_worker(thread_info_t *info) {
    /* Send a signal to the thread, telling it to kill itself */
    pthread_kill(info->thrid, SIGTERM);

    /* Update global wait and run time info */
    wait_times += info->wait_time;
    run_times += info->run_time;
    completed++;

    /* Update schedule queue */
    leave_scheduler_queue(info);

    if (completed >= thread_count) {
        sched_yield(); /* Let other threads terminate */
        printf("The total wait time is %f seconds.\n", (float)wait_times / 1000000);
        printf("The total run time is %f seconds.\n", (float)run_times / 1000000);
        printf("The average wait time is %f seconds.\n", (float)wait_times / 1000000 / thread_count);
        printf("The average run time is %f seconds.\n", (float)run_times / 1000000 / thread_count);
    }
}

/*
 * Signals a worker thread that it should suspend.
 */
static void suspend_worker(thread_info_t *info) {
    printf("Scheduler: suspending %lu.\n", info->thrid);

    /* Update the run time for the thread */
    update_run_time(info);

    /* Update quanta remaining */
    info->quanta--;

    /* Decide whether to cancel or suspend thread */
    if (info->quanta > 0) {
        /*
         * Thread still running: suspend.
         * Signal the worker thread that it should suspend.
         */
        pthread_kill(info->thrid, SIGUSR1);

        /* Update Schedule queue */
        list_remove(&sched_queue, info->le);
        list_insert_tail(&sched_queue, info->le);
    } else {
        /* Thread done: cancel */
        cancel_worker(info);
    }
}

/*
 * This is the scheduling algorithm
 * Pick the next worker thread from the available list
 */
static thread_info_t *next_worker() {
    if (completed >= thread_count)
        return NULL;

    wait_for_queue();
    printf("Scheduler: scheduling.\n");

    /* Return the thread_info_t for the next thread to run */
    return sched_queue.head->info;
}

void timer_handler(int sig, siginfo_t *si, void *uc) {
    thread_info_t *info = NULL;

    /* Once the last worker has been removed, we're done */
    if (list_size(&sched_queue) == 0) {
        quit = 1;
        return;
    }

    /* Suspend the current worker */
    if (currentThread)
        suspend_worker(currentThread);

    /* Resume the next worker */
    info = next_worker();

    /* Update currentThread */
    currentThread = info;

    if (info)
        resume_worker(info);
    else
        quit = 1;
}

/*
 * Set up the signal handlers for SIGALRM, SIGUSR1, and SIGTERM.
 */
void setup_sig_handlers() {
    /* Setup timer handler for SIGALRM signal in scheduler */
    memset(&saction1, 0, sizeof(saction1));
    saction1.sa_sigaction = timer_handler;
    saction1.sa_flags = SA_SIGINFO;
    sigaction(SIGALRM, &saction1, NULL);

    /* Setup cancel handler for SIGTERM signal in workers */
    memset(&saction2, 0, sizeof(saction2));
    saction2.sa_sigaction = cancel_thread;
    saction2.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &saction2, NULL);

    /* Setup suspend handler for SIGUSR1 signal in workers */
    memset(&saction3, 0, sizeof(saction3));
    saction3.sa_sigaction = suspend_thread;
    saction3.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &saction3, NULL);
}

/*******************************************************************************
 *
 *
 *
 ******************************************************************************/

/*
 * Waits until there are workers in the scheduling queue.
 */
static void wait_for_queue() {
    while (!list_size(&sched_queue)) {
        printf("Scheduler: waiting for workers.\n");
        sched_yield();
    }
}

/*
 * Runs at the end of the program just before exit.
 */
static void clean_up() {
    /*
     * Destroy any mutexes/condition variables/semaphores that were created.
     * Free any malloc'd memory not already free'd
     */
    sem_destroy(&queue_sem);
    pthread_mutex_destroy(&sched_queue.lock);
}

/*
 * Prints the program help message.
 */
static void print_help(const char *progname) {
    printf("usage: %s <num_threads> <queue_size> <i_1, i_2 ... i_numofthreads>\n", progname);
    printf("\tnum_threads: the number of worker threads to run\n");
    printf("\tqueue_size: the number of threads that can be in the scheduler at one time\n");
    printf("\ti_1, i_2 ...i_numofthreads: the number of quanta each worker thread runs\n");
}

/*
 * Prints an error summary and exits.
 */
static void exit_error(int err_num) {
    fprintf(stderr, "failure: %s\n", strerror(err_num));
    exit(1);
}

/*
 * Creates the worker threads.
 */
static void create_workers(int thread_count, int *quanta) {
    int i = 0;
    int err = 0;

    for (i = 0; i < thread_count; i++) {
        thread_info_t *info = (thread_info_t *)malloc(sizeof(thread_info_t));
        info->quanta = quanta[i];

        if ((err = pthread_create(&info->thrid, NULL, start_worker, (void *)info)) != 0) {
            exit_error(err);
        }
        printf("Main: detaching worker thread %lu.\n", info->thrid);
        pthread_detach(info->thrid);

        /* Initialize the time variables for each thread for performance evaluation */
        info->run_time = 0;
        info->wait_time = 0;

        clock_gettime(CLOCK, &info->suspend_time);
        clock_gettime(CLOCK, &info->resume_time);
    }
}

/*
 * Runs the scheduler.
 */
static void *scheduler_run(void *unused) {
    // Initialize timerspec
    memset(&timerspec, 0, sizeof(struct itimerspec));
    timerspec.it_value.tv_sec = QUANTUM;
    timerspec.it_interval.tv_sec = QUANTUM;

    wait_for_queue();

    /* Start the timer */
    if (timer_settime(timer, 0, &timerspec, NULL) == -1) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }

    /* Keep the scheduler thread alive */
    while (!quit)
        sched_yield();

    return NULL;
}

/*
 * Starts the scheduler.
 * Returns 0 on success or exits program on failure.
 */
static int start_scheduler(pthread_t *thrid) {
    int err = 0;

    if ((err = pthread_create(thrid, NULL, scheduler_run, NULL)) != 0) {
        exit_error(err);
    }

    return err;
}

/*
 * Reads the command line arguments and starts the scheduler & worker threads.
 */
int smp5_main(int argc, const char **argv) {
    int queue_size = 0;
    int ret_val = 0;
    int *quanta, i;
    pthread_t sched_thread;

    /* Check the arguments */
    if (argc < 3) {
        print_help(argv[0]);
        exit(0);
    }

    thread_count = atoi(argv[1]);
    queue_size = atoi(argv[2]);
    quanta = (int *)malloc(sizeof(int) * thread_count);
    if (argc != 3 + thread_count) {
        print_help(argv[0]);
        exit(0);
    }

    for (i = 0; i < thread_count; i++)
        quanta[i] = atoi(argv[i + 3]);

    printf("Main: running %d workers with queue size %d for quanta:\n", thread_count, queue_size);
    for (i = 0; i < thread_count; i++)
        printf(" %d", quanta[i]);
    printf("\n");

    /* Setup the signal handlers for scheduler and workers */
    setup_sig_handlers();

    /* Initialize anything that needs to be done for the scheduler queue */
    init_sched_queue(queue_size);

    /* Create a thread for the scheduler */
    start_scheduler(&sched_thread);

    /* Create the worker threads and returns */
    create_workers(thread_count, quanta);

    /* Wait for scheduler to finish */
    printf("Main: waiting for scheduler %lu.\n", sched_thread);
    pthread_join(sched_thread, (void **)&ret_val);

    /* Clean up our resources */
    clean_up();

    /* This will wait for all other threads */
    pthread_exit(0);
}

long time_difference(const struct timespec *time1, const struct timespec *time2) {
    return (time1->tv_sec - time2->tv_sec) * 1000000 + (time1->tv_nsec - time2->tv_nsec) / 1000;
}
