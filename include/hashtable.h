#pragma once

#include "common.h"
#include "value.h"

typedef struct Entry {
    ObjString* key;
    Value value;
} Entry;

typedef struct HashTable {
    int count;
    int capacity;
    Entry* entries;
} HashTable;

void initTable(HashTable* table);
void freeTable(HashTable* table);
void copyTable(HashTable* source, HashTable* dest);
bool tableSet(HashTable* table, ObjString* key, Value value);
bool tableGet(HashTable* table, ObjString* key, Value* dest);
bool tableDelete(HashTable* table, ObjString* key);
ObjString *tableFindString(HashTable *table, const char *chars, int length, uint32_t hash);