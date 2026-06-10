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

typedef struct Compiler
{
	Local locals[LOCALS_COUNT];
	HashTable constantNames;
	int localCount;
	int scopeDepth;
} Compiler;

Parser parser;
Compiler* current = NULL; // bad for mulithreading
Chunk* compilingChunk;

// helpers

static Chunk* currentChunk() {
	return compilingChunk;
}

// report error at a given token `location`
static void errorAt(Token* location, const char* message) {
	if (parser.panicMode)
		return;

	parser.panicMode = true;

	fprintf(stderr, "[line %d] Error", location->line);

	if (location->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	}
	else if (location->type == TOKEN_ERROR) {
		// Nothing.
	}
	else {
		fprintf(stderr, " at '%.*s'", location->length, location->start);
	}

	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}

// report error at current token
static void errorAtCurrent(const char* message) {
	errorAt(&parser.current, message);
}

// report error at the just-scanned token
static void error(const char* message) {
	errorAt(&parser.previous, message);
}

// advance until a non-error token
// reports all errors
static void advance() {
	parser.previous = parser.current;

	for (;;) {
		parser.current = scan();
		if (parser.current.type != TOKEN_ERROR)
			break;

		errorAtCurrent(parser.current.start);
	}
}

// consume the next token
// if the next token type does not match `expected` reports an error
static void consume(TokenType expected, const char* message) {
	if (parser.current.type == expected) {
		advance();
		return;
	}

	errorAtCurrent(message);
}

// write byte `b` to the active chunk
static void emitByte(byte b) {
	writeChunk(currentChunk(), b, parser.previous.line);
}

// write bytes `b1` and `b2` to the active chunk
static void emitBytes(byte b1, byte b2) {
	emitByte(b1);
	emitByte(b2);
}

// write `n` bytes to the active chunk
static void emitN(size_t n, ...) {
	va_list args;

	va_start(args, n); // Initialize
	for (size_t i = 0; i < n; i++) {
		int bt = va_arg(args, int); // Get next argument
		emitByte((byte) bt);
	}
	va_end(args); // Cleanup
}

// check if current token is of type `type`
static bool check(TokenType type) {
	return type == parser.current.type;
}

// check if current token is of type `type`, if it is advance
static bool match(TokenType type) {
	if (!check(type))
		return false;
	advance();
	return true;
}

// emit a constant
static int emitConstant(Value value) {
	Opcode opcode = OP_CONSTANT_8;
	int constant = addConstant(currentChunk(), value);
	if (constant > BYTE_MAX) {
		opcode = OP_CONSTANT_24;
	}
	else if (constant > 3 * BYTE_MAX) {
		error("Too many constants in one chunk.");
		return -1;
	}

	emitBytes(opcode, (byte) constant);
	return constant;
}

// emit op to create an array
static void emitArray(int length) {
	emitConstant(NUMBER_VAL(length)); // constant storing arr length
	emitByte(OP_ARRAY);
}

// end compilation
static void endCompiler() {
	emitByte(OP_RETURN);

	freeTable(&current->constantNames);

#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), "code");
	}
#endif
}

// synchronize the compiler to allow compilation after an error
static void synchronize() {
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON)
			return;
		switch (parser.current.type) {
			case TOKEN_CLASS:
			case TOKEN_FUNC:
			case TOKEN_VAR:
			case TOKEN_FOR:
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_PRINT:
			case TOKEN_RETURN:
				return;

			default:; // Do nothing.
		}

		advance();
	}
}

// emit a variable's name, return its location
static byte identifierConstant(Token* name) {
	return emitConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
	if (a->length != b->length)
		return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

// find a local variable with given `name` and return its index. If no local with given name then return `-1` to signal that it's a global
static int resolveLocal(Compiler* compiler, Token* name) {
	// walk backwards to correctly shadow
	for (int i = compiler->localCount - 1; i >= 0; i--) {
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {
				error("Can't read local variable in its own initializer.");
			}

			return i;
		}
	}

	return -1;
}

// add a local variable with specified `name` by setting it in locals table
static void addLocal(Token name) {
	if (current->localCount == LOCALS_COUNT) {
		error("Too many local variables in function.");
		return;
	}

	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
}

// declare local variable from the value at the top of the stack
static void declareVariable() {
	if (current->scopeDepth == 0)
		return;

	Token* name = &parser.previous;
	// check if there already is a variable in this scope with the same name
	for (int i = current->localCount - 1; i >= 0; i--) {
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}

		if (identifiersEqual(name, &local->name)) {
			error("Already a variable with this name in this scope.");
		}
	}

	addLocal(*name);
}

// parse a variable
static byte parseVariable(bool isConst, const char* errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);

	declareVariable();
	// if it is cont then put it in constants names table
	if (isConst) {
		ObjString* key = copyString(parser.previous.start, parser.previous.length);
		tableSet(&current->constantNames, key, NULL_VAL); // set the variable as a const
	}
	if (current->scopeDepth > 0)
		return 0;

	return identifierConstant(&parser.previous);
}

static void initializeLocal() {
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

// define a global variable
// `global` - where in the constants array is the variable's name
static void defineVariable(byte global) {
	if (current->scopeDepth > 0) {
		initializeLocal();
		return;
	}

	emitBytes(OP_DEFINE_GLOBAL, global);
}

// init the compiler
static void initCompiler(Compiler* compiler) {
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	initTable(&compiler->constantNames); // init const names
	current = compiler;
}

static void beginScope() {
	current->scopeDepth++;
}

static void endScope() {
	current->scopeDepth--;

	// remove varibles of the old scope
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		emitByte(OP_POP); // TODO second pass to aggregate pops to popmultis
		current->localCount--;
	}
}

// pratt parers forward defs
static void expression();
static ParseRule* getRule(TokenType type);
static void parse(Precedence precedence);
static void statement();
static void declaration();

// access a variable
static void namedVariable(Token name, bool canAssign) {
	byte getOp, setOp;
	int arg = resolveLocal(current, &name);
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else {
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	bool assignmentToken = match(TOKEN_EQUAL) ||
						   match(TOKEN_PLUS_EQUAL) ||
						   match(TOKEN_MINUS_EQUAL) ||
						   match(TOKEN_STAR_EQUAL) ||
						   match(TOKEN_SLASH_EQUAL) ||
						   match(TOKEN_PLUS_PLUS) ||
						   match(TOKEN_MINUS_MINUS);
	TokenType opType = parser.previous.type;
	// printf("namestr %.*s optype %d\n", name.length, name.start, opType);
	if (canAssign && assignmentToken) {
		Value tmp;
		ObjString* key = copyString(name.start, name.length);
		if (tableGet(&current->constantNames, key, &tmp))
			error("Can't assign to a constant");

		if (opType == TOKEN_EQUAL) { // uh was op_equal  and no errors
			expression();
			emitBytes(setOp, (byte) arg);
			return;
		}

		namedVariable(name, false); // get the variable on the stack

		if (opType != TOKEN_PLUS_PLUS && opType != TOKEN_MINUS_MINUS)
			expression();
		else
			emitConstant(NUMBER_VAL(1)); // for ++ and -- second operand is 1

		switch (opType) {
			case TOKEN_PLUS_EQUAL:
			case TOKEN_PLUS_PLUS:
				emitByte(OP_ADD);
				break;
			case TOKEN_MINUS_EQUAL:
			case TOKEN_MINUS_MINUS:
				emitByte(OP_SUBTRACT);
				break;
			case TOKEN_STAR_EQUAL:
				emitByte(OP_MULTIPLY);
				break;
			case TOKEN_SLASH_EQUAL:
				emitByte(OP_DIVIDE);
				break;
		}
		emitBytes(setOp, (byte) arg);
	}
	else {
		emitBytes(getOp, (byte) arg);
	}
}

// pratt parser core

// parse a number literal
static void number(bool canAssign) {
	debugLog("number()");
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
	debugUnlog();
}
// string literal
static void string(bool canAssign) {
	debugLog("string()");
	char* chars = parser.previous.start + 1;
	int length = parser.previous.length - 2;

	char escaped[length];
	int offs = 0;
	for (int i = 0; i < length; i++) {
		if (i + 1 < length && chars[i] == '\\' && chars[i + 1] == 'n') {
			escaped[i - offs] = '\n';
			//escaped[i + 1] = 'U';
			offs++;
			i++;
		}
		else {
			escaped[i - offs] = chars[i];
		}
	}
	emitConstant(OBJ_VAL(copyString(escaped, length - offs)));
	debugUnlog();
}
// array literal
static void array(bool canAssign) {
	debugLog("array()");
	int length = 0;
	while (parser.current.type != TOKEN_RIGHT_BRACE) {
		expression();
		if (parser.current.type != TOKEN_RIGHT_BRACE) {
			consume(TOKEN_COMMA, "Expected ',' between array values");
		}
		length++;
	}
	consume(TOKEN_RIGHT_BRACE, "Expected '}' after array declaration");

	emitArray(length);
	debugUnlog();
	// emitConstant(OBJ_VAL(copyArray(arr, length - offs)));
}

// parse a literal [true/false/null]
static void literal(bool canAssign) {
	debugLog("literal()");
	switch (parser.previous.type) {
		case TOKEN_TRUE:
			emitByte(OP_TRUE);
			break;
		case TOKEN_FALSE:
			emitByte(OP_FALSE);
			break;
		case TOKEN_NULL:
			emitByte(OP_NULL);
			break;
		default:
			break;
	}
	debugUnlog();
}

// parse a parenthesized expression
static void grouping(bool canAssign) {
	debugLog("grouping()");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after grouping.");
	debugUnlog();
}

// parse (prefix?) unary expression
static void unary(bool canAssign) {
	debugLog("unary()");
	TokenType operatorType = parser.previous.type;

	// Compile the operand.
	parse(PREC_UNARY);

	print("body()");

	// Emit the operator instruction.
	switch (operatorType) {
		case TOKEN_MINUS:
			emitByte(OP_NEGATE);
			break;
		case TOKEN_BANG:
			emitByte(OP_NOT);
			break;
		default:
			return; // Unreachable.
	}
	print("bass");
	debugUnlog();
	print("ossa");
}

// parse binary expression
static void binary(bool canAssign) {
	debugLog("binary()");
	TokenType operatorType = parser.previous.type;
	ParseRule* rule = getRule(operatorType);
	parse((Precedence) (rule->precedence + 1));

	print("body()");

	switch (operatorType) {
		case TOKEN_PLUS:
			emitByte(OP_ADD);
			break;
		case TOKEN_MINUS:
			emitByte(OP_SUBTRACT);
			break;
		case TOKEN_STAR:
			emitByte(OP_MULTIPLY);
			break;
		case TOKEN_SLASH:
			emitByte(OP_DIVIDE);
			break;
		case TOKEN_BANG_EQUAL:
			emitBytes(OP_EQUAL, OP_NOT);
			break;
		case TOKEN_EQUAL_EQUAL:
			emitByte(OP_EQUAL);
			break;
		case TOKEN_GREATER:
			emitByte(OP_GREATER);
			break;
		case TOKEN_GREATER_EQUAL:
			emitBytes(OP_LESS, OP_NOT);
			break;
		case TOKEN_LESS:
			emitByte(OP_LESS);
			break;
		case TOKEN_LESS_EQUAL:
			emitBytes(OP_GREATER, OP_NOT);
			break;
		default:
			return; // Unreachable.
	}
	debugUnlog();
}

// array/string access by index
static void indexAccess(bool canAssign) {
	debugLog("indexAccess()");
	expression(); // eval index
	consume(TOKEN_RIGHT_BRACKET, "Expected ']' after expression.");

	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitByte(OP_INDEX_SET);
	}
	else {
		emitByte(OP_INDEX_GET);
	}
	debugUnlog();
}

// variable get/set
static void variable(bool canAssign) {
	debugLog("variable()");
	namedVariable(parser.previous, canAssign);
	debugUnlog();
}

// print statement
static void printStatement() {
	debugLog("print()");
	expression();
	consume(TOKEN_SEMICOLON, "Expected ';' after expression");
	emitByte(OP_PRINT);
	debugUnlog();
}

// expression statement
static void exprStatement() {
	debugLog("exprStatement()");
	expression();
	consume(TOKEN_SEMICOLON, "Expected ';' after expression");
	emitByte(OP_POP);
	debugUnlog();
}

// variable declaration
static void varDeclaration(bool isConst) {
	const char* chars[20];
	sprintf(chars, "varDeclaration(%d)", isConst);
	debugLog(chars);
	byte global = parseVariable(isConst, "Expected variable name.");

	if (match(TOKEN_EQUAL)) {
		expression();
	}
	else {
		emitByte(OP_NULL);
	}
	consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

	defineVariable(global);
	debugUnlog();
}

// block
static void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expected '}' after block.");
}

// pratt table : BEGIN
ParseRule rules[] = {
	[TOKEN_LEFT_PAREN] = {grouping, NULL, NULL, PREC_NONE},
	[TOKEN_RIGHT_PAREN] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_BRACE] = {array, NULL, NULL, PREC_NONE},
	[TOKEN_RIGHT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_COMMA] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_DOT] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_MINUS] = {unary, binary, NULL, PREC_TERM},
	[TOKEN_PLUS] = {NULL, binary, NULL, PREC_TERM},
	[TOKEN_PLUS_PLUS] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_MINUS_MINUS] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_SEMICOLON] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_SLASH] = {NULL, binary, NULL, PREC_FACTOR},
	[TOKEN_STAR] = {NULL, binary, NULL, PREC_FACTOR},
	[TOKEN_BANG] = {unary, NULL, NULL, PREC_NONE},
	[TOKEN_BANG_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
	[TOKEN_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_EQUAL_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
	[TOKEN_GREATER] = {NULL, binary, NULL, PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
	[TOKEN_LESS] = {NULL, binary, NULL, PREC_COMPARISON},
	[TOKEN_LESS_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
	[TOKEN_PLUS_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_MINUS_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_STAR_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_SLASH_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_BRACKET] = {NULL, indexAccess, NULL, PREC_CALL},
	[TOKEN_RIGHT_BRACKET] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_IDENTIFIER] = {variable, NULL, NULL, PREC_NONE},
	[TOKEN_STRING] = {string, NULL, NULL, PREC_NONE},
	[TOKEN_NUMBER] = {number, NULL, NULL, PREC_NONE},
	[TOKEN_AND] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_CLASS] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_ELSE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_FALSE] = {literal, NULL, NULL, PREC_NONE},
	[TOKEN_FOR] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_FUNC] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_IF] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_NULL] = {literal, NULL, NULL, PREC_NONE},
	[TOKEN_OR] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_PRINT] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_RETURN] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_SUPER] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_THIS] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_TRUE] = {literal, NULL, NULL, PREC_NONE},
	[TOKEN_VAR] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_WHILE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_VOID] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_STATIC] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_ELIF] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_BREAK] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_CONTINUE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_USING] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_PROPERTY] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_ERROR] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_EOF] = {NULL, NULL, NULL, PREC_NONE},
};
// pratt table : END

static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

// parse part of expression with operatos of at least `precedence` precedence
static void parse(Precedence precedence) {
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (prefixRule == NULL) {
		error("Expected expression.");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();

		ParseRule* rule = getRule(parser.previous.type);

		if (rule->infix != NULL) {
			rule->infix(canAssign);
		}
		else if (rule->postfix != NULL) {
			rule->postfix(canAssign);
		}
	}

	if (canAssign && match(TOKEN_EQUAL)) {
		error("Invalid assignment target.");
	}
}

// parse an expression
static void expression() {
	parse(PREC_ASSIGNMENT);
}

static void statement() {
	if (match(TOKEN_PRINT)) {
		printStatement();
	}
	else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	}
	else {
		exprStatement();
	}
}

static void declaration() {
	if (match(TOKEN_VAR) || match(TOKEN_CONST)) {
		varDeclaration(parser.previous.type == TOKEN_CONST);
	}
	else {
		statement();
	}

	if (parser.panicMode)
		synchronize();
}

// compile source code into bytecode
bool compile(const char* source, Chunk* chunk) {
	initScanner(source);
	Compiler compiler;
	initCompiler(&compiler);

	parser.hadError = false;
	parser.panicMode = false;

	compilingChunk = chunk;

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