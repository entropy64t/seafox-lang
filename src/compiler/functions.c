#include "compiler_internal.h"

void function(FunctionType type) {
	Compiler compiler;
	initCompiler(&compiler, type);
	beginScope();

	if (type != TYPE_LAMBDA)
		consume(TOKEN_LEFT_PAREN, "Expected '(' after function name");
	TokenType argsEnd = (type == TYPE_LAMBDA ? TOKEN_FORWARD : TOKEN_RIGHT_PAREN);
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
	emitConstant(OBJ_VAL(function));
}