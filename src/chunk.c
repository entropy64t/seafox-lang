#include <stdlib.h>

#include "chunk.h"
#include "fox_memory.h"

void initChunk(Chunk* chunk) {
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = NULL;
    // chunk->lines = NULL;

    chunk->lineCounts = NULL;
    chunk->lineCapacity = 0;
    
    initValueArr(&chunk->constants);
}

void writeChunk(Chunk* chunk, byte b, int line) {
    if (chunk->count >= chunk->capacity) {
        int oldCap = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCap);
        chunk->code = GROW_ARRAY(byte, chunk->code, oldCap, chunk->capacity);
    }
    if (line >= chunk->lineCapacity) {
        int oldCap = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCap);
        chunk->lineCounts = GROW_ARRAY(int, chunk->lineCounts, oldCap, chunk->lineCapacity);
    }

    chunk->code[chunk->count] = b;
    chunk->lineCounts[line]++;
    chunk->count++;
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(byte, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lineCounts, chunk->capacity);
    freeValueArr(&chunk->constants);
    initChunk(chunk);
}

int addConstant(Chunk* chunk, Value constant) {
    writeValueArr(&chunk->constants, constant);
    return chunk->constants.count - 1;
}

int writeConstant(Chunk* chunk, Value constant, int line) {
    if (chunk->constants.count < 256) {
        writeChunk(chunk, OP_CONSTANT_8, line);
        writeChunk(chunk, chunk->constants.count, line);
    }
    else {
        writeChunk(chunk, OP_CONSTANT_24, line);
        byte constant[3];
        int count = chunk->constants.count;
        constant[0] = count;
        constant[1] = count >> 8;
        constant[2] = count >> 16;
        // printf("%d %d\n", count, constant[0] + (constant[1] << 8) + (constant[2] << 16));
        writeChunk(chunk, constant[2], line);
        writeChunk(chunk, constant[1], line);
        writeChunk(chunk, constant[0], line);
    }

    writeValueArr(&chunk->constants, constant);

    return chunk->constants.count - 1;
}

int getLine(Chunk* chunk, int index) {
    int currLine = 0;
    int currIndex = 0;
    while (currIndex <= index) {
        currIndex += chunk->lineCounts[currLine++];
    }
    return currLine - 1;
}