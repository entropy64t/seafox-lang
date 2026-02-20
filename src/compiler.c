#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

typedef struct Parser
{
    Token current;
    Token previous;
    bool hadError;
    // indicates if in panic mode
    // when in panic mode errors are not reported
    bool panicMode;
} Parser;

typedef enum Precedence{
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;


Parser parser;
Chunk* compilingChunk;

static Chunk* currentChunk() {
    return compilingChunk;
}

// report error at current token
static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

// report error at the just-scanned token
static void error(const char* message) {
    errorAt(&parser.previous, message);
}

// report error at a given token `location`
static void errorAt(Token* location, const char* message) {
    if (parser.panicMode) return;

    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", location->line);

    if (location->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (location->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", location->length, location->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
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

// advance until a non-error token
// reports all errors
static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scan();
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

// write byte `b` to the active chunk
static void emit(byte b) {
    writeChunk(currentChunk(), b, parser.previous.line);
}

// write bytes `b1` and `b2` to the active chunk
static void emit2(byte b1, byte b2) {
    emit(b1);
    emit(b2);
}

// write `n` bytes to the active chunk
static void emitN(size_t n, ...) {
    va_list args;

    va_start(args, n);  // Initialize
    for (size_t i = 0; i < n; i++) {
        int bt = va_arg(args, int);  // Get next argument
        emit((byte)bt);
    }
    va_end(args);  // Cleanup
}

// emit a constant
static void emitConstant(Value value) {
    Opcode opcode = OP_CONSTANT_8;
    int constant = addConstant(currentChunk(), value);
    if (constant > BYTE_MAX) {
        opcode = OP_CONSTANT_24;
    }
    else if (constant > 3 * BYTE_MAX) {
        error("Too many constants in one chunk.");
        return;
    }

    emit2(opcode, (byte)constant);
}

// end compilation
static void endCompiler() {
    emit(OP_RETURN);
}

// parse part of expression with operatos of at least `precedence` precedence
static void parse(Precedence precedence) {
    // ???
}

// parse a number literal
static void number() {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(value);
}

// parse a parenthesized expression
static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after grouping.");
}

// parse unary expression (-a, !a, a++, a--)
static void unary() {
    TokenType operatorType = parser.previous.type;

    // Compile the operand.
    parse(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return; // Unreachable.
    }
}

// parse an expression
static void expression() {
    parse(PREC_ASSIGNMENT);
}

// compile source code into bytecode
bool compile(const char* source, Chunk* chunk) {
    initScanner(source);

    parser.hadError = false;
    parser.panicMode = false;

    compilingChunk = chunk;

    advance();
    expression();

    consume(TOKEN_EOF, "Expected end of expression");

    endCompiler();

    return !parser.hadError;
}