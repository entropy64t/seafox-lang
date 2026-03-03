#include <stdio.h>
#include <string.h>

#include "fox_memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static Object* allocateObject(size_t size, ObjectType type) {
    Object* object = (Object*)reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;

    return object;
}

#define ALLOCATE_OBJ(type, objectType) (type*)allocateObject(sizeof(type), objectType)

static ObjString* allocateString(char* chars, int length) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

static ObjArray* allocateArray(Value* items, int length) {
    ObjArray* array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY);
    array->length = length;
    array->items = items;
    return array;
}

ObjString* copyString(char* chars, int length) {
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length);
}

ObjString* takeString(char* chars, int length) {
    return allocateString(chars, length);
}

ObjArray* copyArray(Value* items, int length) {
    Value* heapItems = ALLOCATE(Value, length);
    memcpy(heapItems, items, sizeof(Value) * length);

    return allocateArray(heapItems, length);
}

ObjArray* takeArray(Value* items, int length) {
    return allocateArray(items, length);
}

static void printArray(ObjArray* array) {
    printf("{ ");

    for (int i = 0; i < array->length - 1; i++) {
        printValue(array->items[i], ", ");
    }
    printValue(array->items[array->length - 1], " ");
    printf("}");
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_ARRAY:
            //printf("aaaa");
            printArray(AS_ARRAY(value));
            break;
    }
}