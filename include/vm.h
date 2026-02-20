#pragma once

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef enum InterpretResult
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct VMachine
{
    Chunk* chunk;
    byte* ip;

    Value stack[STACK_MAX];
    Value* stackTop;
} VMachine;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);

void push(Value v);
Value pop();