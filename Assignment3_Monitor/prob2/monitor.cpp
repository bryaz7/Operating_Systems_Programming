// Bryan Duong
// Nov 3, 2024

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>

// Condition variable structure
typedef struct {
  int count;
  sem_t counter;
} ConditionVariable;

// give up exclusive access to monitor and suspend appropriate thread
// implement either Hoare or Mesa paradigm
void wait(ConditionVariable *condVar) {
  condVar->count--;
  if (condVar->count < 0) {
    sem_wait(&(condVar->counter));
  }
}

// unblock suspended thread at head of queue
// implement either Hoare or Mesa paradigm
void signal(ConditionVariable *condVar) {
  condVar->count++;
  if (condVar->count <= 0) {
    sem_post(&(condVar->counter));
  }
}