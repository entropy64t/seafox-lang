#include <stdio.h>

#include "debug.h"
#include "value.h"

static int simpleInstruction(FILE* out, const char* name, int offset) {
	fprintf(out, "%s\n", name);
	return offset + 1;
}

static int byteInstruction(FILE* out, const char* name, Chunk* chunk, int offset) {
	uint8_t slot = chunk->code[offset + 1];

	fprintf(out, "%-16s %4u\n", name, slot);

	return offset + 2;
}

static int jumpInstruction(FILE* out, const char* name, int sign, Chunk* chunk, int offset) {
	uint16_t jump =
		((uint16_t) chunk->code[offset + 1] << 8) |
		chunk->code[offset + 2];

	fprintf(out,
			"%-16s %4d -> %d\n",
			name,
			offset,
			offset + 3 + sign * jump);

	return offset + 3;
}

static int constantInstruction(FILE* out, const char* name, Chunk* chunk, int offset, int operandSize) {
	int index = 0;

	for (int i = 0; i < operandSize; i++) {
		index <<= 8;
		index |= chunk->code[offset + 1 + i];
	}

	fprintf(out, "%-16s %4d '", name, index);

	fprintValue(out, chunk->constants.values[index], "");

	fprintf(out, "'\n");

	return offset + 1 + operandSize;
}

int disassembleInstruction(FILE* out, Chunk* chunk, int offset) {
	fprintf(out, "%04d ", offset);

	int line = getLine(chunk, offset);

	if (offset > 0 &&
		line == getLine(chunk, offset - 1)) {
		fprintf(out, "   | ");
	}
	else {
		fprintf(out, "%4d ", line);
	}

	byte instruction = chunk->code[offset];

	switch (instruction) {
		case OP_RETURN:
			return simpleInstruction(out, "RETURN", offset);

		case OP_CONSTANT_8:
			return constantInstruction(out, "CONSTANT_8", chunk, offset, 1);

		case OP_CONSTANT_24:
			return constantInstruction(out, "CONSTANT_24", chunk, offset, 3);

		case OP_TRUE:
			return simpleInstruction(out, "TRUE", offset);

		case OP_FALSE:
			return simpleInstruction(out, "FALSE", offset);
		case OP_NULL:
			return simpleInstruction(out, "NULL", offset);
		case OP_NEGATE:
			return simpleInstruction(out, "NEGATE", offset);
		case OP_NOT:
			return simpleInstruction(out, "NOT", offset);
		case OP_ADD:
			return simpleInstruction(out, "ADD", offset);
		case OP_SUBTRACT:
			return simpleInstruction(out, "SUBTRACT", offset);
		case OP_MULTIPLY:
			return simpleInstruction(out, "MULTIPLY", offset);
		case OP_DIVIDE:
			return simpleInstruction(out, "DIVIDE", offset);
		case OP_EQUAL:
			return simpleInstruction(out, "EQUAL", offset);
		case OP_GREATER:
			return simpleInstruction(out, "GREATER", offset);
		case OP_LESS:
			return simpleInstruction(out, "LESS", offset);
		case OP_ARRAY:
			return simpleInstruction(out, "ARRAY", offset);
		case OP_INDEX_GET:
			return simpleInstruction(out, "INDEX_ACCESS_GET", offset);
		case OP_INDEX_SET:
			return simpleInstruction(out, "INDEX_ACCESS_SET", offset);
		case OP_PRINT:
			return simpleInstruction(out, "PRINT", offset);
		case OP_POP:
			return simpleInstruction(out, "POP", offset);
		case OP_DEFINE_GLOBAL:
			return constantInstruction(out, "GLOBAL_DEFINE", chunk, offset, 1);
		case OP_GET_GLOBAL:
			return constantInstruction(out, "GLOBAL_GET", chunk, offset, 1);
		case OP_SET_GLOBAL:
			return constantInstruction(out, "GLOBAL_SET", chunk, offset, 1);
		case OP_GET_LOCAL:
			return byteInstruction(out, "LOCAL_GET", chunk, offset);
		case OP_SET_LOCAL:
			return byteInstruction(out, "LOCAL_SET", chunk, offset);
		case OP_JUMP:
			return jumpInstruction(out, "JUMP", 1, chunk, offset);
		case OP_LOOP:
			return jumpInstruction(out, "LOOP", -1, chunk, offset);
		case OP_JUMP_IF_TRUE:
			return jumpInstruction(out, "JUMP_IF_TRUE", 1, chunk, offset);
		case OP_JUMP_IF_FALSE:
			return jumpInstruction(out, "JUMP_IF_FALSE", 1, chunk, offset);
		case OP_CALL:
			return byteInstruction(out, "CALL", chunk, offset);
		default:
			fprintf(out, "Unknown opcode %u\n", instruction);
			return offset + 1;
	}
}

static void disassembleChunkInternal(FILE* out, Chunk* chunk, const char* name) {
	fprintf(out, "=== %s ===\n", name);

	for (int offset = 0; offset < chunk->count;) {
		offset = disassembleInstruction(out, chunk, offset);
	}
}

void disassembleChunk(Chunk* chunk, const char* name) {
	disassembleChunkInternal(stdout, chunk, name);
}

bool disassembleChunkToFile(Chunk* chunk, const char* name, const char* path) {
	FILE* out = fopen(path, "w");

	if (out == NULL) {
		return false;
	}

	disassembleChunkInternal(out, chunk, name);

	fclose(out);

	return true;
}

int loglevel = 0;

void debugLog(const char* str) {
#ifdef DEBUG_PRINT_CODE
	print(str);
	loglevel++;
#endif
}

void print(const char* str) {
#ifdef DEBUG_PRINT_CODE
	for (int i = 0; i < loglevel; i++) {
		printf("  ");
	}

	fputs(str, stdout);
	fputc('\n', stdout);
#endif
}

void debugUnlog(void) {
#ifdef DEBUG_PRINT_CODE
	loglevel--;
#endif
}