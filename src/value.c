#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "value.h"
#include "fox_memory.h"
#include "object.h"

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

void printValue(Value value, const char* end) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NULL:
            printf("null");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJECT:
            printObject(value);
            break;
        default:
            printf("error-type");
            break;
    }

    printf(end);
}

static bool objectsEqual(Object* a, Object* b) {
    switch (a->type)
    {
        case OBJ_STRING: {
            return a == b; // pointer equality, because strings are interned
            //ObjString *aString = a;
            //ObjString* bString = b;
            //return aString->length == bString->length && memcmp(aString->chars, bString->chars, aString->length) == 0;
        }
        case OBJ_ARRAY: {
            ObjArray* aArr = a;
            ObjArray* bArr = b;
            if (aArr->length != bArr->length)
                return false;
            for (int i = 0; i < aArr->length; i++) {
                if (!valuesEqual(aArr->items[i], bArr->items[i]))
                    return false;
            }
            return true;
        }
    }
    
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type)
        return false;
    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:    
            return true;
        case VAL_NUMBER: 
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJECT: {
            Object* objA = AS_OBJECT(a);
            Object* objB = AS_OBJECT(b);
            return objectsEqual(objA, objB);
        }
        default:         
            return false; // Unreachable.
    }
}

bool isFalsey(Value value) {
    if (value.type == VAL_NULL)
        return true;
    if (value.type == VAL_BOOL)
        return !AS_BOOL(value);

    return false;
}