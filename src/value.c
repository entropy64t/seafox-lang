#include <stdlib.h>
#include <stdio.h>

#include "value.h"
#include "memory.h"

void initValueArr(ValueArray* array) {
    array->capacity = 0;
    array->count = 0;
    array->values = NULL;
}

void writeValueArr(ValueArray* array, Value v) {
    if (array->count >= array->capacity) {
        int oldCap = array->capacity;
        array->capacity = GROW_CAPACITY(oldCap);
        array->values = GROW_ARRAY(Value, array->values, oldCap, array->capacity);
    }

    array->values[array->count] = v;
    array->count++;
}

void freeValueArr(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArr(array);
}

void printValue(Value value) {
    printf("%g", value);
}