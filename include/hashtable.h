#pragma once

#include "common.h"
#include "value.h"

typedef struct HashTable {
    int count;
    int capacity;
    Entry* entries;
};

typedef struct Entry {
    ObjString* key;
    Value value;
};