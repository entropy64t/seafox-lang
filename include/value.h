#pragma once

#include "common.h"

typedef double Value;

typedef struct ValueArray
{
    int count;
    int capacity;
    Value* values;
} ValueArray;
typedef ValueArray Varr;

void initValueArr(ValueArray* array);
void writeValueArr(ValueArray* array, Value value);
void freeValueArr(ValueArray* array);
void printValue(Value value);