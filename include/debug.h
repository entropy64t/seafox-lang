#pragma once

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);
void debugLog(const char *str);
void debugUnlog();
void print(const char* str);