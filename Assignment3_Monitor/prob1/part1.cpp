// Bryan Duong
// Nov 3, 2024

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

void *producer(void *id);
void *consumer(void *id);
void initializeResources(int argc, char *argv[]);
void cleanupResources();

pthread_t *prodThreads, *consThreads;
sem_t mutex, empty, full;
char *buf;
char item = 'X';
int bSize, nProds, nCons, iToProd;
int inIndex = 0, outIndex = 0;
int prodCount = 0, consCount = 0;

void *producer(void *id) {
  int *currentId = (int *)id;

  while (1) {
    sem_wait(&empty);
    sem_wait(&mutex);

    if (prodCount < iToProd) {
      buf[inIndex] = item;
      printf("p:<%d>, item: %c, at %d\n", *currentId, item, inIndex);
      inIndex = (inIndex + 1) % bSize;
      prodCount++;
    } else {
      sem_post(&mutex);
      sem_post(&full);
      return NULL;
    }

    sem_post(&mutex);
    sem_post(&full);
  }
}

void *consumer(void *id) {
  int *currentId = (int *)id;

  while (1) {
    sem_wait(&full);
    sem_wait(&mutex);

    if (consCount < iToProd) {
      char tempItem = buf[outIndex];
      buf[outIndex] = '\0';
      printf("c:<%d>, item: %c, at %d\n", *currentId, tempItem, outIndex);
      outIndex = (outIndex + 1) % bSize;
      consCount++;
    } else {
      sem_post(&mutex);
      sem_post(&empty);
      return NULL;
    }

    sem_post(&mutex);
    sem_post(&empty);
  }
}

void initializeResources(int argc, char *argv[]) {
  if (argc != 9) {
    fprintf(stderr, "Usage: %s -b <buffer_size> -p <num_producers> -c <num_consumers> -i <items_to_produce>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  bSize = atoi(argv[2]);
  nProds = atoi(argv[4]);
  nCons = atoi(argv[6]);
  iToProd = atoi(argv[8]);

  buf = (char *)malloc(sizeof(char) * bSize);
  sem_init(&mutex, 0, 1);
  sem_init(&full, 0, 0);
  sem_init(&empty, 0, bSize);

  prodThreads = (pthread_t *)malloc(sizeof(pthread_t) * nProds);
  consThreads = (pthread_t *)malloc(sizeof(pthread_t) * nCons);
}

void cleanupResources() {
  free(buf);
  free(prodThreads);
  free(consThreads);
  sem_destroy(&mutex);
  sem_destroy(&empty);
  sem_destroy(&full);
}

int main(int argc, char *argv[]) {
  initializeResources(argc, argv);

  int *prodIds = (int *)malloc(sizeof(int) * nProds);
  int *consIds = (int *)malloc(sizeof(int) * nCons);

  for (int i = 0; i < nProds; i++) {
    prodIds[i] = i + 1;
    if (pthread_create(&prodThreads[i], NULL, producer, &prodIds[i])) {
      fprintf(stderr, "Producer thread %d failed!\n", prodIds[i]);
      exit(EXIT_FAILURE);
    }
  }

  for (int i = 0; i < nCons; i++) {
    consIds[i] = i + 1;
    if (pthread_create(&consThreads[i], NULL, consumer, &consIds[i])) {
      fprintf(stderr, "Consumer thread %d failed!\n", consIds[i]);
      exit(EXIT_FAILURE);
    }
  }

  for (int i = 0; i < nProds; i++) {
    pthread_join(prodThreads[i], NULL);
  }

  for (int i = 0; i < nCons; i++) {
    pthread_join(consThreads[i], NULL);
  }

  free(prodIds);
  free(consIds);
  cleanupResources();

  return 0;
}