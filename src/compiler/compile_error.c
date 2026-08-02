#include "compiler_internal.h"

// report error at a given token `location`
void errorAt(Token* location, const char* message) {
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
void errorAtCurrent(const char* message) {
	errorAt(&parser.current, message);
}

// report error at the just-scanned token
void error(const char* message) {
	errorAt(&parser.previous, message);
}

// synchronize the compiler to allow compilation after an error
void synchronize() {
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
