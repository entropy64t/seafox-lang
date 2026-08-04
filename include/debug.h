#pragma once

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);

void debugLog(const char* str);
void debugUnlog();
void print(const char* str);

bool disassembleChunkToFile(Chunk* chunk, const char* name, const char* path);

int disassembleInstruction(FILE* out, Chunk* chunk, int offset);