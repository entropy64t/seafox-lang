#pragma once

#include "common.h"
#include "value.h"
#include "chunk.h"
#include <stdio.h>

typedef enum ObjectType
{
	OBJ_STRING,
	OBJ_ARRAY,
	OBJ_FUNCTION,
	OBJ_NATIVE_FN,
	OBJ_CLOSURE,
	OBJ_UPVALUE,
	OBJ_SEAFOX_TYPE,
	OBJ_ITERATOR,
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
	int defaultsCount;
	int upvalueCount;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

typedef bool (*NativeFn)(int argCount, Value* args, Value* out);
typedef struct ObjNativeFn
{
	Object obj;
	NativeFn function;
	char* name;
	int arity;
} ObjNativeFn;

typedef struct ObjUpvalue
{
	Object obj;
	Value closed;
	Value* location;
	struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct ObjClosure
{
	Object obj;
	ObjFunction* function;
	ObjUpvalue** upvalues;
	int upvalueCount;
} ObjClosure;

typedef struct ObjSeafoxType
{
	Object obj;
	ValueType value;
	ObjectType object;
	ObjString* name;
	ObjNativeFn* function; // this gets called when doing Type()
} ObjSeafoxType;

typedef struct ObjIterator
{
	Object obj;
	Value* pointer;
	ObjArray* container;
} ObjIterator;

extern const char* types[VALUE_TYPE_COUNT];
extern const char* objtypes[OBJ_TYPE_COUNT];

static inline bool isObjType(Value value, ObjectType type) {
	return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#define OBJ_TYPE(value) (AS_OBJECT(value)->type)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_ARRAY(value) isObjType(value, OBJ_ARRAY)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE_FN)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_SEAFOX_TYPE(value) isObjType(value, OBJ_SEAFOX_TYPE)
#define IS_ITERATOR(value) isObjType(value, OBJ_ITERATOR)

#define AS_STRING(value) ((ObjString*) AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjString*) AS_OBJECT(value))->chars)
#define AS_ARRAY(value) ((ObjArray*) AS_OBJECT(value))
#define AS_FUNCTION(value) ((ObjFunction*) AS_OBJECT(value))
#define AS_NATIVE(value) ((ObjNativeFn*) AS_OBJECT(value))
#define AS_UPVALUE(value) ((ObjUpvalue*) AS_OBJECT(value))
#define AS_CLOSURE(value) ((ObjClosure*) AS_OBJECT(value))
#define AS_SEAFOX_TYPE(value) ((ObjSeafoxType*) AS_OBJECT(value))
#define AS_ITERATOR(value) ((ObjIterator*) AS_OBJECT(value))

ObjString* copyString(char* chars, int length);
ObjString* takeString(char* chars, int length);
ObjArray* copyArray(Value* items, int length);
ObjArray* takeArray(Value* items, int length);
ObjArray* newArray(int length);
ObjFunction* newFunction(void);
ObjNativeFn* newNativeFn(NativeFn function, const char* name, int arity);
ObjUpvalue* newUpvalue(Value* slot);
ObjClosure* newClosure(ObjFunction* function);
ObjSeafoxType* newType(char* name, ValueType value, ObjectType obj);
ObjIterator* makeIterator(ObjArray* container);
ObjIterator* incrementIterator(ObjIterator* source);
Value seafoxType(Value value);
void fprintObject(FILE* file, Value value);
void printObject(Value value);