#ifndef __TEST_H__
#define __TEST_H__

#include <semaphore.h>

struct shared {
	sem_t mutex;
	int count;
} shared;

#endif