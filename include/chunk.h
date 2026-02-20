#pragma once

#include "common.h"
#include "value.h"

// constant: 1 byte
// long constant: 3 bytes
typedef enum Opcode
{
    OP_RETURN,
    OP_CONSTANT_8,
    OP_CONSTANT_24,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
} Opcode;

typedef struct Chunk
{
    byte* code;

    int* lineCounts;
    int lineCapacity;

    int count;
    int capacity;
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, byte b, int line);
void freeChunk(Chunk* chunk);
int addConstant(Chunk* chunk, Value constant);
int writeConstant(Chunk* chunk, Value constant, int line);
int getLine(Chunk* chunk, int index);