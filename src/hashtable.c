#include <stdlib.h>
#include <string.h>

#include "fox_memory.h"
#include "object.h"
#include "value.h"
#include "hashtable.h"

#define TABLE_MAX_LOAD 0.75

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    // uint32_t index = key->hash % capacity;
    // opt: capacity = 2^n => % cap <=> & (cap - 1)
    uint32_t index = key->hash & (capacity - 1);
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_NULL(entry->value)) {
                return (tombstone != NULL ? tombstone : entry);
            }
            else if (tombstone == NULL) {
                // new tombstone
                tombstone = entry;
            }
        }
        else if (entry->key == key) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static void adjustCapacity(HashTable* table, int capacity) {
    Entry *entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

void initTable(HashTable* table) {
    table->capacity = 0;
    table->count = 0;
    table->entries = NULL;
}

void freeTable(HashTable* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

void copyTable(HashTable* source, HashTable* dest) {
    for (int i = 0; i < source->capacity; i++) {
        Entry *entry = &source->entries[i];
        if (entry->key != NULL) {
            tableSet(dest, entry->key, entry->value);
        }
    }
}

bool tableSet(HashTable* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);

    bool isNew = entry->key == NULL;
    if (isNew && IS_NULL(entry->value)) // dont increment if inserting into tombstone
        table->count++;

    entry->key = key;
    entry->value = value;
    return isNew;
}

bool tableGet(HashTable* table, ObjString* key, Value* dest) {
    if (table->count == 0)
        return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    *dest = entry->value;
    return true;
}

bool tableDelete(HashTable* table, ObjString* key) {
    if (table->count == 0)
        return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    entry->key = NULL;
    entry->value = NULL_VAL; // NUMBER_VAL(2137); // tombstone value
    return true;
}

ObjString* tableFindString(HashTable* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0)
        return NULL;

    uint32_t index = hash & (table->capacity - 1);

    for (;;) {
        Entry* entry = &table->entries[index];

        if (entry->key == NULL) {
            if (IS_NULL(entry->value)) {
                return NULL;
            }
        }
        else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }
        index = (index + 1) & (table->capacity - 1);
    }
}