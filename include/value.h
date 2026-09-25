#pragma once

#include "common.h"
#include <stdio.h>

typedef struct Object Object;
typedef struct ObjString ObjString;
typedef struct ObjArray ObjArray;

typedef enum ValueType
{
	VAL_NUMBER,
	VAL_INTEGER,
	VAL_BOOL,
	VAL_NULL,
	VAL_OBJECT,
	VALUE_TYPE_COUNT
} ValueType;

typedef struct Value
{
	ValueType type;
	union
	{
		bool boolean;
		double number;
		long long integer;
		Object* object;
	} as;
} Value;

#define BOOL_VAL(value) ((Value) {VAL_BOOL, {.boolean = (value)}})
#define NULL_VAL ((Value) {VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value) ((Value) {VAL_NUMBER, {.number = (value)}})
#define OBJ_VAL(value) ((Value) {VAL_OBJECT, {.object = (Object*) value}})
#define INT_VAL(value) ((Value) {VAL_INTEGER, {.integer = (value)}})

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJECT(value) ((value).as.object)
#define AS_INT(value) ((value).as.integer)

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)
#define IS_INT(value) ((value).type == VAL_INTEGER)

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

void fprintValue(FILE* file, Value value, const char* end);
void printValue(Value value, const char* end);
bool valuesEqual(Value a, Value b);
bool isFalsey(Value value);