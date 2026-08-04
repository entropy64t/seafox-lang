#include "compiler_internal.h"

// report error at a given token `location`
void errorAt(Token* location, const char* format, ...) {
	if (parser.panicMode)
		return;

	parser.panicMode = true;

	fprintf(stderr, "[line %d] Error", location->line);

	if (location->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	}
	else if (location->type != TOKEN_ERROR) {
		fprintf(stderr, " at '%.*s'", location->length, location->start);
	}

	fprintf(stderr, ": ");

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	fprintf(stderr, "\n");

	parser.hadError = true;
}

// report error at current token
void errorAtCurrent(const char* format, ...) {
	va_list args;
	va_start(args, format);
	errorAt(&parser.current, format, args);
	va_end(args);
}

// report error at the just-scanned token
void error(const char* format, ...) {
	va_list args;
	va_start(args, format);
	errorAt(&parser.previous, format, args);
	va_end(args);
}

// synchronize the compiler to allow compilation after an error
void synchronize() {
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON)
			return;
		switch (parser.current.type) {
			case TOKEN_CLASS:
			case TOKEN_FUNCTION:
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
