#pragma once

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

typedef void (*ParseFn)(bool canAssign);

typedef struct Parser
{
	Token current;
	Token previous;
	bool hadError;
	// indicates if in panic mode
	// when in panic mode errors are not reported
	bool panicMode;
} Parser;

typedef enum Precedence
{
	PREC_NONE,
	PREC_ASSIGNMENT, // =
	PREC_OR,		 // or
	PREC_AND,		 // and
	PREC_EQUALITY,	 // == !=
	PREC_COMPARISON, // < > <= >=
	PREC_TERM,		 // + -
	PREC_FACTOR,	 // * /
	PREC_UNARY,		 // ! -
	PREC_CALL,		 // . () []
	PREC_PRIMARY
} Precedence;

typedef struct ParseRule
{
	ParseFn prefix;
	ParseFn infix;
	ParseFn postfix;
	Precedence precedence;
} ParseRule;

typedef struct Local
{
	Token name;
	int depth;
} Local;

typedef enum FunctionType
{
	TYPE_SCRIPT,
	TYPE_FUNCTION,
} FunctionType;

typedef struct Compiler
{
	ObjFunction* function;
	FunctionType type;
	Local locals[LOCALS_COUNT];
	HashTable constantNames;
	int localCount;
	int scopeDepth;
} Compiler;

extern Parser parser;
extern Compiler* current; // bad for mulithreading

// compiler.c //

Chunk* currentChunk();

void setChunk(Chunk* chunk);

void beginScope();

void endScope();

// compile_error.c //

// report error at a given token `location`
void errorAt(Token* location, const char* message);

// report error at current token
void errorAtCurrent(const char* message);

// report error at the just-scanned token
void error(const char* message);

// synchronize the compiler to allow compilation after an error
void synchronize();

// tokens.c //

// check if current token is of type `type`
bool check(TokenType type);

// check if current token is of type `type`, if it is advance
bool match(TokenType type);

// advance until a non-error token
// reports all errors
void advance();

// consume the next token
// if the next token type does not match `expected` reports an error
void consume(TokenType expected, const char* message);

// emit.c //

// write byte `b` to the active chunk
void emitByte(byte b);

// write bytes `b1` and `b2` to the active chunk
void emitBytes(byte b1, byte b2);

// write `n` bytes to the active chunk
void emitN(size_t n, ...);

// emit a constant
int emitConstant(Value value);

// emit op to create an array
void emitArray(int length);

// emit instruction `jumpType` with 2 bytes for operands and return its location
int emitJump(byte jumpType);

// patch a jump's oparand
void patchJump(int location);

void emitLoop(int offset);

// variable.c //

// parse a variable
byte parseVariable(bool isConst, const char* errorMessage);

// define a global variable
// `global` - where in the constants array is the variable's name
void defineVariable(byte global);

// access a variable
void namedVariable(Token name, bool canAssign);

// expressions.c //

// parse an expression
void expression();

ParseRule* getRule(TokenType type);

// parse part of expression with operatos of at least `precedence` precedence
void parse(Precedence precedence);

// statements.c //
void declaration();