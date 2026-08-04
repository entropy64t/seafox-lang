#include <stdlib.h>
#include <stdio.h>

#include "chunk.h"
#include "fox_memory.h"
#include "debug.h"

void initChunk(Chunk* chunk) {
	chunk->capacity = 0;
	chunk->count = 0;
	chunk->code = NULL;
	// chunk->lines = NULL;

	chunk->bytecodeLengths = NULL;
	chunk->lineCapacity = 0;

	initValueArr(&chunk->constants);
}

void writeChunk(Chunk* chunk, byte b, int line) {
	if (chunk->count >= chunk->capacity) {
		int oldCap = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(oldCap);
		chunk->code = GROW_ARRAY(byte, chunk->code, oldCap, chunk->capacity);
	}
	while (line >= chunk->lineCapacity) {
		int oldCap = chunk->lineCapacity;
		chunk->lineCapacity = GROW_CAPACITY(oldCap);
		chunk->bytecodeLengths = GROW_ARRAY(int, chunk->bytecodeLengths, oldCap, chunk->lineCapacity);
		// init all past old cap to 0
		for (int i = oldCap; i < chunk->lineCapacity; i++)
			chunk->bytecodeLengths[i] = 0;
	}

	chunk->code[chunk->count] = b;
	chunk->bytecodeLengths[line]++;
	chunk->count++;
}

void freeChunk(Chunk* chunk) {
	FREE_ARRAY(byte, chunk->code, chunk->capacity);
	FREE_ARRAY(int, chunk->bytecodeLengths, chunk->capacity);
	freeValueArr(&chunk->constants);
	initChunk(chunk);
}

// append `source` to the end of `target`
void appendChunk(Chunk* target, const Chunk* source) {
	int constantOffset = target->constants.count;

	for (int i = 0; i < source->count;) {
		byte op = source->code[i];

		writeChunk(target, op, getLine(source, i));

		switch (op) {
			case OP_CONSTANT_8:
				{
					byte index = source->code[i + 1];
					writeChunk(target, index + constantOffset, getLine(source, i + 1));
					i += 2;
					break;
				}

			case OP_CONSTANT_24:
				{
					int index =
						((int) source->code[i + 1] << 16) |
						((int) source->code[i + 2] << 8) |
						(int) source->code[i + 3];

					index += constantOffset;

					writeChunk(target, (index >> 16) & 0xff, getLine(source, i + 1));
					writeChunk(target, (index >> 8) & 0xff, getLine(source, i + 2));
					writeChunk(target, index & 0xff, getLine(source, i + 3));

					i += 4;
					break;
				}

			case OP_DEFINE_GLOBAL:
			case OP_GET_GLOBAL:
			case OP_SET_GLOBAL:
			case OP_GET_LOCAL:
			case OP_SET_LOCAL:
			case OP_CALL:
				writeChunk(target, source->code[i + 1], getLine(source, i + 1));
				i += 2;
				break;

			case OP_JUMP:
			case OP_JUMP_IF_TRUE:
			case OP_JUMP_IF_FALSE:
			case OP_LOOP:
				writeChunk(target, source->code[i + 1], getLine(source, i + 1));
				writeChunk(target, source->code[i + 2], getLine(source, i + 2));
				i += 3;
				break;

			default:
				i += 1;
				break;
		}
	}

	for (int i = 0; i < source->constants.count; i++) {
		addConstant(target, source->constants.values[i]);
	}
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
	// printf("Get line from bytecode index %d\n", index);

	int currLine = 0;
	int currIndex = 0;
	while (currIndex <= index) {
		// printf("Line %d has length %d", currLine, chunk->bytecodeLengths[currLine]);
		currIndex += chunk->bytecodeLengths[currLine++];
	}
	return currLine - 1;
}

bool writeChunkToFile(Chunk* chunk, const char* path) {
	FILE* f = fopen(path, "wb");
	if (f == NULL)
		return false;

	/* Magic */
	fwrite("SFB0", 1, 4, f);

	/* Bytecode */
	uint32_t codeCount = chunk->count;
	fwrite(&codeCount, sizeof(codeCount), 1, f);
	fwrite(chunk->code, sizeof(byte), codeCount, f);

	/* Constants */
	uint32_t constantCount = chunk->constants.count;
	fwrite(&constantCount, sizeof(constantCount), 1, f);

	for (uint32_t i = 0; i < constantCount; i++) {
		Value value = chunk->constants.values[i];
		fwrite(&value, sizeof(Value), 1, f);
	}

	fclose(f);
	return true;
}