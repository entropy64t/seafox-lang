#pragma once

#include "chunk.h"
#include "value.h"
#include "hashtable.h"
#include "object.h"

#define FRAMES_MAX 1024
#define STACK_MAX (FRAMES_MAX * LOCALS_COUNT)

typedef enum InterpretResult
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct CallFrame
{
	ObjClosure* closure;
	byte* ip;
	Value* slots;
} CallFrame;

typedef struct VM
{
	Chunk* chunk;
	byte* ip;

	CallFrame frames[FRAMES_MAX];
	int frameCount;

	Value stack[STACK_MAX];
	Value* stackTop;
	Object* objects;
	ObjUpvalue* openUpvalues;

	HashTable strings;
	HashTable globals;
} VM;

extern VM vm;

void initVM();
void freeVM();
void runtimeError(const char* format, ...);
InterpretResult interpret(const char* source, char* bytecodePath, const char* traceFile);

void push(Value v);
Value pop();