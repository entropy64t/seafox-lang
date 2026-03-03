#include <stdio.h>

#include "debug.h"
#include "value.h"

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

// operand size in bytes
static int constantInstruction(const char* name, Chunk* chunk, int offset, int operandSize) {
    byte constant[operandSize];
    int index = 0;
    int p = 0;
    for (int i = operandSize - 1; i >= 0; i--) {
        constant[i] = chunk->code[offset + i + 1];
        index += constant[i] << p;
        p += 8; // shift by 1 byte
    }

    printf("%-16s %4d '", name, index);
    printValue(chunk->constants.values[index], "");
    printf("'\n");
    return offset + 1 + operandSize;
}

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("=== %s ===\n", name);
    for (int offset = 0; offset < chunk->count; ) {
        offset = disassembleInstruction(chunk, offset);
    }
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);

    int line = getLine(chunk, offset);
    if (offset > 0 && line == getLine(chunk, offset - 1)) {
        printf("   | ");
    }
    else {
        printf("%4d ", line);
    }

    byte instruction = chunk->code[offset];
    switch (instruction) {
        case OP_RETURN:
            return simpleInstruction("RETURN", offset);
        case OP_CONSTANT_8:
            return constantInstruction("CONSTANT_8", chunk, offset, 1);
        case OP_CONSTANT_24:
            return constantInstruction("CONSTANT_24", chunk, offset, 3);
        case OP_TRUE:
            return simpleInstruction("TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("FALSE", offset);
        case OP_NULL:
            return simpleInstruction("NULL", offset);
        case OP_NEGATE:
            return simpleInstruction("NEGATE", offset);
        case OP_NOT:
            return simpleInstruction("NOT", offset);
        case OP_ADD:
            return simpleInstruction("ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("DIVIDE", offset);
            case OP_EQUAL:
            return simpleInstruction("EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("GREATER", offset);
        case OP_LESS:
            return simpleInstruction("LESS", offset);
        case OP_ARRAY:
            return simpleInstruction("ARRAY", offset);
        case OP_INDEX_GET:
            return simpleInstruction("INDEX_ACCESS", offset);
        case OP_PRINT:
            return simpleInstruction("PRINT", offset);
        case OP_POP:
            return simpleInstruction("POP", offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("GLOBAL_DEFINE", chunk, offset, 1);
        case OP_GET_GLOBAL:
            return constantInstruction("GLOBAL_GET", chunk, offset, 1);
        case OP_SET_GLOBAL:
            return constantInstruction("GLOBAL_SET", chunk, offset, 1);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

int loglevel = 0;
void debugLog(const char *str) {
#ifdef DEBUG_PRINT_CODE
    print(str);
    loglevel++;
#endif
}

void print(const char* str) {
#ifdef DEBUG_PRINT_CODE
    for (int i = 0; i < loglevel; i++)
        printf("  ");
    printf(str);
    printf("\n");
#endif
}

void debugUnlog() {
#ifdef DEBUG_PRINT_CODE
    loglevel--;
#endif
}