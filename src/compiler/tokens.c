#include "compiler_internal.h"

// check if current token is of type `type`
bool check(TokenType type) {
	return type == parser.current.type;
}

// check if current token is of type `type`, if it is advance
bool match(TokenType type) {
	if (!check(type))
		return false;
	advance();
	return true;
}

// advance until a non-error token
// reports all errors
void advance() {
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
void consume(TokenType expected, const char* message) {
	if (parser.current.type == expected) {
		advance();
		return;
	}

	errorAtCurrent(message);
}