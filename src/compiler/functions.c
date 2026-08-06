#include "compiler_internal.h"

void function(FunctionType type) {
	Compiler compiler;
	initCompiler(&compiler, type);
	beginScope();

	if (type != TYPE_LAMBDA)
		consume(TOKEN_LEFT_PAREN, "Expected '(' after function name");

	TokenType argsEnd = (type == TYPE_LAMBDA ? TOKEN_FORWARD : TOKEN_RIGHT_PAREN);

	bool defargsStarted = false;
	if (!check(argsEnd)) {
		do {
			current->function->arity++;
			if (current->function->arity > 255) {
				errorAtCurrent("A function can have at most 255 parameters");
			}

			bool isConst = false;
			if (match(TOKEN_CONST))
				isConst = true;

			byte constant = parseVariable(isConst, "Expected parameter name");

			if (defargsStarted && !check(TOKEN_EQUAL))
				error("Non-default function argument after a default argument");

			if (match(TOKEN_EQUAL)) {
				expression();
				defargsStarted = true;
				current->function->defaultsCount++;
			}
			defineVariable(constant);
		} while (match(TOKEN_COMMA));
	}

	if (argsEnd == TOKEN_RIGHT_PAREN)
		consume(argsEnd, "Expected '(' after function parameters");
	// for lambdas, the => is consumed in the match

	if (match(TOKEN_LEFT_BRACE)) {
		block();
	}
	else if (match(TOKEN_FORWARD)) {
		// support for expression bodies
		if (type == TYPE_LAMBDA && match(TOKEN_RIGHT_BRACE)) {
			block();
		}
		else {
			expression();
			if (type != TYPE_LAMBDA)
				consume(TOKEN_SEMICOLON, "Expected ';' after expression body");
			emitByte(OP_RETURN);
		}
	}
	else {
		error("Expected a block or expression function body");
		return; // dont try to compile broken function
	}
	ObjFunction* function = endCompiler();
	emitByte(OP_CLOSURE);
	emitByte(CONSTANT(makeConstant(OBJ_VAL(function))));

	for (int i = 0; i < function->upvalueCount; i++) {
		emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
		emitByte(compiler.upvalues[i].index);
	}
}