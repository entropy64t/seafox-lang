#include <stdlib.h>
#include <stdio.h>

#include "fox_memory.h"
#include "object.h"
#include "vm.h"
#include "value.h"

static void freeObject(Object* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            FREE_ARRAY(Value, array->items, array->length);
            FREE(ObjArray, object);
            break;
        }
    }
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL)
        exit(1);
    return result;
}

void freeObjects() {
    Object* object = vm.objects;

    while (object != NULL) {
        Object* next = object->next;
        freeObject(object);
        object = next;
    }
}