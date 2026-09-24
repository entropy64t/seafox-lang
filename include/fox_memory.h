#pragma once

#include "common.h"
#include "object.h"
#include "value.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize);

void freeObjects();

void markObject(Object* object);

void markValue(Value value);

void collectGarbage();

#define START_CAPACITY 8
#define GROW_RATE 2
#define GC_HEAP_GROW_FACTOR 2

#define ALLOCATE(type, count) (type*) reallocate(NULL, 0, sizeof(type) * (count))

#define GROW_CAPACITY(capacity) ((capacity) < (START_CAPACITY) ? (START_CAPACITY) : ((capacity) * GROW_RATE))

#define GROW_ARRAY(type, array, oldCount, newCount) (type*) reallocate(array, sizeof(type) * (oldCount), sizeof(type) * newCount)

#define FREE_ARRAY(type, pointer, oldCount) reallocate(pointer, sizeof(type) * (oldCount), 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)