#include "compiler_internal.h"

// pratt parers forward defs
static void statement();
void declaration();

// print statement
static void printStatement() {
	debugLog("print()");
	expression();
	consume(TOKEN_SEMICOLON, "Expected ';' after expression");
	emitByte(OP_PRINT);
	debugUnlog();
}

// expression statement
static void expressionStatement() {
	debugLog("expressionStatement()");
	expression();
	consume(TOKEN_SEMICOLON, "Expected ';' after expression");
	emitByte(OP_POP);
	debugUnlog();
}

// variable declaration
static void varDeclaration(bool isConst) {
	char chars[20];
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

static void ifStatement() {
	consume(TOKEN_LEFT_PAREN, "Expected '(' after if statement");
	expression(); // condition
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition");

	int ifBranchSkip = emitJump(OP_JUMP_IF_FALSE); // jump to skip if body
	emitByte(OP_POP);							   // pop condition
	statement();								   // body

	int elseBranchSkip = emitJump(OP_JUMP);

	patchJump(ifBranchSkip);
	emitByte(OP_POP); // pop condition

	if (match(TOKEN_ELSE))
		statement();

	patchJump(elseBranchSkip);
}

static void whileStatement() {
	int loopStart = currentChunk()->count;
	consume(TOKEN_LEFT_PAREN, "Expected '(' after 'while'");
	expression(); // condition
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition");

	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();

	emitLoop(loopStart);

	patchJump(exitJump);
	emitByte(OP_POP);
}

static void forInitializer() {
	if (match(TOKEN_SEMICOLON)) {
		// No initializer.
	}
	else if (match(TOKEN_VAR)) {
		varDeclaration(false);
	}
	else {
		expressionStatement();
	}
}

static int forCondition() {
	if (match(TOKEN_SEMICOLON))
		return -1;

	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

	// Jump out of the loop if the condition is false.
	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP); // Condition.
	return exitJump;
}

static int forIncrement(int loopStart) {
	if (match(TOKEN_RIGHT_PAREN))
		return loopStart;

	expression();
	emitByte(OP_POP);
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

	return loopStart;
}

static void forStatement() {
	beginScope();
	consume(TOKEN_LEFT_PAREN, "Expected '(' after 'for'");

	forInitializer();

	int loopStart = currentChunk()->count;

	int exitJump = forCondition();

	Chunk incrementChunk;
	Chunk* enclosing = currentChunk();
	initChunk(&incrementChunk);
	setChunk(&incrementChunk);
	loopStart = forIncrement(loopStart);
	setChunk(enclosing);

	statement();

	disassembleChunk(&incrementChunk, "FOR INCREMENT");

	appendChunk(enclosing, &incrementChunk);

	emitLoop(loopStart);

	if (exitJump != -1) {
		patchJump(exitJump);
		emitByte(OP_POP);
	}

	endScope();
}

// block
static void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expected '}' after block.");
}

static void statement() {
	if (match(TOKEN_PRINT)) {
		printStatement();
	}
	else if (match(TOKEN_IF)) {
		ifStatement();
	}
	else if (match(TOKEN_WHILE)) {
		whileStatement();
	}
	else if (match(TOKEN_FOR)) {
		forStatement();
	}
	else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	}
	else {
		expressionStatement();
	}
}

void declaration() {
	if (match(TOKEN_VAR) || match(TOKEN_CONST)) {
		varDeclaration(parser.previous.type == TOKEN_CONST);
	}
	else {
		statement();
	}

	if (parser.panicMode)
		synchronize();
}
