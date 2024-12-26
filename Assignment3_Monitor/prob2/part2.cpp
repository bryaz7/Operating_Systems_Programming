// Bryan Duong
// Nov 3, 2024

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

void *producer(void *id);
void *consumer(void *id);
char randAlpha();

pthread_t *prodThreads, *consThreads;

sem_t mutex, empty, full;

char *buf;
int bufSize, numProds, numCons, iToProd;
int inIdx = 0, outIdx = 0, prodCount = 0, consCount = 0;
int totalProduced = 0;
int done = 0;

void *producer(void *id) {
  int *currentId = (int *) id;
  char alpha;

  while (1) {
    sem_wait(&empty);
    sem_wait(&mutex);

    if (prodCount < iToProd) {
      alpha = randAlpha();
      buf[inIdx] = alpha;
      printf("p:<%d>, item: %c, at %d\n", *currentId, alpha, inIdx);
      inIdx = (inIdx + 1) % bufSize;
      prodCount++;
      totalProduced++;
    } else {
      done = 1;
      sem_post(&mutex);
      sem_post(&full);
      sem_post(&empty);
      return 0;
    }

    sem_post(&mutex);
    sem_post(&full);
  }
}

void *consumer(void *id) {
  int *currentCid = (int *) id;

  while (1) {
    sem_wait(&full);
    sem_wait(&mutex);

    if (consCount < totalProduced) {
      char tempItem = buf[outIdx];
      buf[outIdx] = '\0';
      printf("c:<%d>, item: %c, at %d\n", *currentCid, tempItem, outIdx);
      outIdx = (outIdx + 1) % bufSize;
      consCount++;
    } else if (done) {
      sem_post(&mutex);
      sem_post(&full);
      return 0;
    }

    sem_post(&mutex);
    sem_post(&empty);
  }
}

char randAlpha() {
  const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  char randomLetter = alphabet[random() % 52];
  return randomLetter;
}

int main(int argc, char *argv[]) {
  bufSize = atoi(argv[2]);
  numProds = atoi(argv[4]);
  numCons = atoi(argv[6]);
  iToProd = atoi(argv[8]);

  buf = (char *) malloc(sizeof(char) * bufSize);
  sem_init(&mutex, 0, 1);
  sem_init(&empty, 0, bufSize);
  sem_init(&full, 0, 0);

  prodThreads = new pthread_t[numProds];
  consThreads = new pthread_t[numCons];

  int *pidList = new int[numProds];
  int *cidList = new int[numCons];

  for (int i = 0; i < numProds; i++) {
    pidList[i] = i + 1;
    if (pthread_create(&prodThreads[i], NULL, producer, &pidList[i])) {
      fprintf(stderr, "Creation of producer thread %d failed!\n", pidList[i]);
      return -1;
    }
  }
  for (int i = 0; i < numCons; i++) {
    cidList[i] = i + 1;
    if (pthread_create(&consThreads[i], NULL, consumer, &cidList[i])) {
      fprintf(stderr, "Creation of consumer thread %d failed!\n", cidList[i]);
      return -1;
    }
  }

  for (int i = 0; i < numProds; i++) {
    pthread_join(prodThreads[i], NULL);
  }
  for (int i = 0; i < numCons; i++) {
    sem_post(&full);
    pthread_join(consThreads[i], NULL);
  }

  free(buf);
  delete[] prodThreads;
  delete[] consThreads;
  delete[] pidList;
  delete[] cidList;

  sem_destroy(&mutex);
  sem_destroy(&empty);
  sem_destroy(&full);

  return 0;
}