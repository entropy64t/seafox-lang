#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

#define USAGE "Usage:\n"                                                       \
			  "    seafox\n"                                                   \
			  "        Start the REPL.\n"                                      \
			  "\n"                                                             \
			  "    seafox <file.fox>\n"                                        \
			  "        Compile and run a Seafox source file.\n"                \
			  "\n"                                                             \
			  "    seafox <file.fox> [-o <bytecode-path>] [-c <trace-path>]\n" \
			  "        Compile and run the source file.\n"                     \
			  "        Optionally save bytecode and/or disassembly."

static void usage() {
	fprintf(stderr, USAGE);
	exit(64);
}

static void repl() {
	char line[1024];
	for (;;) {
		printf("> ");

		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}

		interpret(line, NULL, NULL);
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

	char* buffer = (char*) malloc(fileSize + 1);
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

static void runFile(const char* path, const char* bytecodeDump, const char* traceDump) {
	char* source = readFile(path);
	InterpretResult result = interpret(source, bytecodeDump, traceDump);
	free(source);

	if (result == INTERPRET_COMPILE_ERROR)
		exit(65);
	if (result == INTERPRET_RUNTIME_ERROR)
		exit(70);
}

int parseArgs(int argc, const char** argv) {
	if (argc == 1) {
		repl();
		return 0;
	}

	const char* source = NULL;
	const char* outBytecode = NULL;
	const char* outTrace = NULL;

	source = argv[1];

	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0) {
			if (++i >= argc)
				usage();
			outBytecode = argv[i];
		}
		else if (strcmp(argv[i], "-c") == 0) {
			if (++i >= argc)
				usage();
			outTrace = argv[i];
		}
		else {
			usage();
		}
	}

	runFile(source, outBytecode, outTrace);
}

int main(int argc, const char** argv) {
	initVM();

	parseArgs(argc, argv);

	freeVM();

	return 0;
}