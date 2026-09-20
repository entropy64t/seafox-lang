#include "compiler_internal.h"

// parse part of expression with operatos of at least `precedence` precedence
void parse(Precedence precedence) {
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

void makeLambda();

// parse an expression
void expression() {
	if (match(TOKEN_LAMBDA)) {
		makeLambda();
		return;
	}

	parse(PREC_ASSIGNMENT);
}

void makeLambda() {
	function(TYPE_LAMBDA);
}

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
	while (parser.current.type != TOKEN_RIGHT_BRACKET) {
		expression();
		if (parser.current.type != TOKEN_RIGHT_BRACKET) {
			consume(TOKEN_COMMA, "Expected ',' between array values");
		}
		length++;
	}
	consume(TOKEN_RIGHT_BRACKET, "Expected ']' after array declaration");

	emitArray(length);
	debugUnlog();
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
		case TOKEN_STAR:
			emitByte(OP_ITERATOR_GET);
			break;
		default:
			return; // Unreachable.
	}
	debugUnlog();
}

// parse binary expression
static void binary(bool canAssign) {
	debugLog("binary()");
	TokenType operatorType = parser.previous.type;
	bool isNot = false;
	if (operatorType == TOKEN_IS) {
		if (match(TOKEN_NOT))
			isNot = true;
	}
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
		case TOKEN_IS:
			emitByte(OP_IS);
			if (isNot)
				emitByte(OP_NOT);
			break;
		case TOKEN_MODULO:
			emitByte(OP_MODULO);
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

static void logicAnd(bool canAssign) {
	debugLog("and()");

	int jumpPastEnd = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP); // pop the result

	parse(PREC_AND);

	patchJump(jumpPastEnd);

	debugUnlog();
}

static void logicOr(bool canAssign) {
	debugLog("or()");

	int jumpPastEnd = emitJump(OP_JUMP_IF_TRUE);
	emitByte(OP_POP); // pop the result

	parse(PREC_AND);

	patchJump(jumpPastEnd);

	debugUnlog();
}

static byte argumentList() {
	byte count = 0;
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			if (count >= 255) {
				error("A function can have at most 255 parameters");
			}
			count++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
	return count;
}

static void call(bool canAssign) {
	byte argCount = argumentList();
	emitBytes(OP_CALL, argCount);
}

static void ternary(bool canAssign) {
	debugLog("ternary()");
	TokenType operatorType = parser.previous.type;
	ParseRule* rule = getRule(operatorType);
	parse((Precedence) (rule->precedence + 1));
	consume(TOKEN_COLON, "Expected ':' between ternary operator expressions.");
	parse((Precedence) (rule->precedence + 1));

	if (operatorType != TOKEN_QMARK) {
		error("Unsupported ternary operator");
		return;
	}

	emitByte(OP_CONDITIONAL);
}

// pratt table : BEGIN
ParseRule rules[] = {
	[TOKEN_LEFT_PAREN] = {grouping, call, NULL, PREC_CALL},
	[TOKEN_RIGHT_PAREN] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_RIGHT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_COMMA] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_DOT] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_MINUS] = {unary, binary, NULL, PREC_TERM},
	[TOKEN_PLUS] = {NULL, binary, NULL, PREC_TERM},
	[TOKEN_PLUS_PLUS] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_MINUS_MINUS] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_SEMICOLON] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_SLASH] = {NULL, binary, NULL, PREC_FACTOR},
	[TOKEN_STAR] = {unary, binary, NULL, PREC_FACTOR},
	[TOKEN_QMARK] = {NULL, ternary, NULL, PREC_TERANRY},
	[TOKEN_MODULO] = {NULL, binary, NULL, PREC_TERM},
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
	[TOKEN_LEFT_BRACKET] = {array, indexAccess, NULL, PREC_CALL},
	[TOKEN_RIGHT_BRACKET] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_IDENTIFIER] = {variable, NULL, NULL, PREC_NONE},
	[TOKEN_STRING] = {string, NULL, NULL, PREC_NONE},
	[TOKEN_NUMBER] = {number, NULL, NULL, PREC_NONE},
	[TOKEN_AND] = {NULL, logicAnd, NULL, PREC_AND},
	[TOKEN_CLASS] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_ELSE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_FALSE] = {literal, NULL, NULL, PREC_NONE},
	[TOKEN_FOR] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_FUNCTION] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_LAMBDA] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_IF] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_NULL] = {literal, NULL, NULL, PREC_NONE},
	[TOKEN_OR] = {NULL, logicOr, NULL, PREC_OR},
	[TOKEN_PRINT] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_RETURN] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_SUPER] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_THIS] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_TRUE] = {literal, NULL, NULL, PREC_NONE},
	[TOKEN_VAR] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_IS] = {NULL, binary, NULL, PREC_EQUALITY},
	[TOKEN_WHILE] = {NULL, NULL, NULL, PREC_NONE},
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

ParseRule* getRule(TokenType type) {
	return &rules[type];
}