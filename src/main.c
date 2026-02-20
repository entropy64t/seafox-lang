#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source); 

    if (result == INTERPRET_COMPILE_ERROR)
        exit(65);
    if (result == INTERPRET_RUNTIME_ERROR)
        exit(70);
}

int main(int argc, const char* argv[]) {
    initVM();

    if (argc == 1) {
        repl();
    }
    else if (argc == 2) {
        runFile(argv[1]);
    }
    else if (argc == 3) {
        // compile into bytecode and print to a file, then run. TODO
    }
    else {
        fprintf(stderr, "Usage: seafox [path]");
        exit(64);
    }

    freeVM();
    /*
    Chunk chunk;
    initChunk(&chunk);

    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 1);
    writeChunk(&chunk, constant, 1);

    // writeChunk(&chunk, OP_RETURN, 1);

   // for (int i = 0; i < 1000; i++) {
   //     addConstant(&chunk, 1);
   // }
//disassembleChunk(&chunk, "a");
    for (int line = 2; line < 10; line++) {
        writeConstant(&chunk, line * 2, line);
    }
    //disassembleChunk(&chunk, "a");
    writeChunk(&chunk, OP_NEGATE, 10);
    writeChunk(&chunk, OP_ADD, 11);
    writeChunk(&chunk, OP_RETURN, 12);

    // disassembleChunk(&chunk, "test chunk");
    writeConstant(&chunk, 1.2, 1);
    writeConstant(&chunk, 3.4, 2);

    writeChunk(&chunk, OP_ADD, 3);

    writeConstant(&chunk, 5.6, 4);

    writeChunk(&chunk, OP_DIVIDE, 5);

    interpret(&chunk);

    freeChunk(&chunk);

    freeVM();*/

    return 0;
}