#pragma once

#include "common.h"
#include "value.h"

// constant: 1 byte
// long constant: 3 bytes
// TODO : reorder for readability
typedef enum Opcode
{
	OP_CONSTANT_8,
	OP_CONSTANT_24,

	OP_NEGATE,

	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_MODULO,

	OP_NULL,
	OP_TRUE,
	OP_FALSE,

	OP_NOT,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_IS,

	OP_ARRAY,
	OP_INDEX_GET,
	OP_INDEX_SET,

	OP_ITERATOR_GET,

	OP_PRINT,

	OP_POP,

	OP_DEFINE_GLOBAL,
	OP_GET_GLOBAL,
	OP_SET_GLOBAL,

	OP_CLOSE_UPVALUE,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,

	OP_GET_LOCAL,
	OP_SET_LOCAL,

	OP_JUMP,
	OP_LOOP,
	OP_JUMP_IF_TRUE,
	OP_JUMP_IF_FALSE,
	OP_CONDITIONAL,

	OP_CALL,
	OP_CLOSURE,
	OP_RETURN,

	OP_CLASS,
	OP_SET_PROPERTY,
	OP_GET_PROPERTY
} Opcode;

typedef struct Chunk
{
	byte* code;

	int* bytecodeLengths;
	int lineCapacity;

	int count;
	int capacity;
	ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, byte b, int line);
void freeChunk(Chunk* chunk);
void appendChunk(Chunk* target, const Chunk* source);
int addConstant(Chunk* chunk, Value constant);
int writeConstant(Chunk* chunk, Value constant, int line);
int getLine(Chunk* chunk, int index);
bool writeChunkToFile(Chunk* chunk, const char* path);