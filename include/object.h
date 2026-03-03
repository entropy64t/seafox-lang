#pragma once

#include "common.h"
#include "value.h"

typedef enum ObjectType
{
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_TYPE_COUNT
} ObjectType;

struct Object 
{
    ObjectType type;
    Object* next;
};

struct ObjString 
{
    Object object;
    int length;
    char* chars;
    uint32_t hash;
};

struct ObjArray
{
    Object object;
    int length;
    Value* items;
};

static inline bool isObjType(Value value, ObjectType type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#define OBJ_TYPE(value) (AS_OBJECT(value)->type)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_ARRAY(value) isObjType(value, OBJ_ARRAY)

#define AS_STRING(value) ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJECT(value))->chars)
#define AS_ARRAY(value) ((ObjArray*)AS_OBJECT(value))

ObjString* copyString(char *chars, int length);
ObjString* takeString(char *chars, int length);
ObjArray* copyArray(Value *items, int length);
ObjArray* takeArray(Value *items, int length);
void printObject(Value value);