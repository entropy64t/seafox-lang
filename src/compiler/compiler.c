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
#include "fox_memory.h"
#include "vm.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include "compiler_internal.h"

Parser parser;
Compiler* current = NULL;

Chunk* currentChunk() {
	return &current->function->chunk;
}

ObjFunction* endCompiler() {
	emitReturn();

	freeTable(&current->constantNames);

	ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
	}
#endif

	current = current->enclosing;

	return function;
}

void initCompiler(Compiler* compiler, FunctionType type) {
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	initTable(&compiler->constantNames); // init const names
	compiler->function = newFunction();
	current = compiler;

	if (type != TYPE_SCRIPT) {
		current->function->name = copyString(parser.previous.start, parser.previous.length);
	}

	Local* local = &current->locals[current->localCount++];
	local->depth = 0;
	local->name.start = "";
	local->name.length = 0;
	local->isCaptured = false;
}

void beginScope() {
	current->scopeDepth++;
}

void endScope() {
	current->scopeDepth--;

	// remove varibles of the old scope
	while (current->localCount > 0 &&
		   current->locals[current->localCount - 1].depth > current->scopeDepth) {
		if (current->locals[current->localCount - 1].isCaptured) {
			emitByte(OP_CLOSE_UPVALUE);
		}
		else {
			emitByte(OP_POP);
		}
		current->localCount--;
	}
}

// compile source code into bytecode
ObjFunction* compile(const char* source) {
	initScanner(source);
	Compiler compiler;
	initCompiler(&compiler, TYPE_SCRIPT);
	push(OBJ_VAL(compiler.function));

	parser.hadError = false;
	parser.panicMode = false;

	advance();

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_EOF, "Expected end of program");

	if (parser.hadError) {
		pop();
		return NULL;
	}
	ObjFunction* function = endCompiler();
	return function;
}

void markCompilerRoots() {
	Compiler* compiler = current;
	while (compiler != NULL) {
		markObject((Object*) compiler->function);
		markTable(&compiler->constantNames);
		compiler = compiler->enclosing;
	}

	if (current != NULL) {
		for (Object* object = vm.objects; object != NULL; object = object->next) {
			markObject(object);
		}
	}
}