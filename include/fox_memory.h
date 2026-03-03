#pragma once

#include "common.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize);

void freeObjects();

#define START_CAPACITY 8
#define GROW_RATE 2

#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define GROW_CAPACITY(capacity) ((capacity) < (START_CAPACITY) ? (START_CAPACITY) : ((capacity) * GROW_RATE))

#define GROW_ARRAY(type, array, oldCount, newCount) (type*)reallocate(array, sizeof(type) * (oldCount), sizeof(type) * newCount)

#define FREE_ARRAY(type, pointer, oldCount) reallocate(pointer, sizeof(type) * (oldCount), 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)