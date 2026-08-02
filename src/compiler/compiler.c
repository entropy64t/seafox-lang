#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "value.h"
#include "object.h"
#include "hashtable.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include "compiler_internal.h"

Parser parser;
Compiler* current = NULL;

Chunk* currentChunk() {
	return &current->function->chunk;
}

void setChunk(Chunk* chunk) {
	compilingChunk = chunk;
}

static ObjFunction* endCompiler() {
	emitByte(OP_RETURN);

	freeTable(&current->constantNames);

	ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
	}
#endif

	return function;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
	compiler->function = NULL;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	initTable(&compiler->constantNames); // init const names
	compiler->function = newFunction();
	current = compiler;

	Local* local = &current->locals[current->localCount++];
	local->depth = 0;
	local->name.start = "";
	local->name.length = 0;
}

void beginScope() {
	current->scopeDepth++;
}

void endScope() {
	current->scopeDepth--;

	// remove varibles of the old scope
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		emitByte(OP_POP); // TODO second pass to aggregate pops to popmultis
		current->localCount--;
	}
}

// compile source code into bytecode
bool compile(const char* source, Chunk* chunk) {
	initScanner(source);
	Compiler compiler;
	initCompiler(&compiler, TYPE_SCRIPT);

	parser.hadError = false;
	parser.panicMode = false;

	advance();

	while (!match(TOKEN_EOF)) {
		declaration();
	}
	//while (parser.current.type != TOKEN_EOF) {
	//expression();
	//}

	consume(TOKEN_EOF, "Expected end of program");

	endCompiler();

	return !parser.hadError;
}