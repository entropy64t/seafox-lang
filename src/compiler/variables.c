#include "compiler_internal.h"

// emit a variable's name, return its location
static byte identifierConstant(Token* name) {
	return emitConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
	if (a->length != b->length)
		return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

// find a local variable with given `name` and return its index.
// If no local with given name then return `-1` to signal that it's a global
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
byte parseVariable(bool isConst, const char* errorMessage) {
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
void defineVariable(byte global) {
	if (current->scopeDepth > 0) {
		initializeLocal();
		return;
	}

	emitBytes(OP_DEFINE_GLOBAL, global);
}

// access a variable
void namedVariable(Token name, bool canAssign) {
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
