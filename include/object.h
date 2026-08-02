#pragma once

#include "common.h"
#include "value.h"
#include "chunk.h"

typedef enum ObjectType
{
	OBJ_STRING,
	OBJ_ARRAY,
	OBJ_FUNCTION,
	OBJ_TYPE_COUNT
} ObjectType;

typedef struct Object
{
	ObjectType type;
	Object* next;
} Object;

typedef struct ObjString
{
	Object object;
	int length;
	char* chars;
	uint32_t hash;
} ObjString;

typedef struct ObjArray
{
	Object object;
	int length;
	Value* items;
} ObjArray;

typedef struct ObjFunction
{
	Object obj;
	int arity;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

static inline bool isObjType(Value value, ObjectType type) {
	return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#define OBJ_TYPE(value) (AS_OBJECT(value)->type)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_ARRAY(value) isObjType(value, OBJ_ARRAY)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)

#define AS_STRING(value) ((ObjString*) AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjString*) AS_OBJECT(value))->chars)
#define AS_ARRAY(value) ((ObjArray*) AS_OBJECT(value))
#define AS_FUNCTION(value) ((ObjFunction*) AS_OBJECT(value))

ObjString* copyString(char* chars, int length);
ObjString* takeString(char* chars, int length);
ObjArray* copyArray(Value* items, int length);
ObjArray* takeArray(Value* items, int length);
ObjFunction* newFunction(void);
void printObject(Value value);