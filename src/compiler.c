#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "value.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef void (*ParseFn)();

typedef struct Parser
{
    Token current;
    Token previous;
    bool hadError;
    // indicates if in panic mode
    // when in panic mode errors are not reported
    bool panicMode;
} Parser;

typedef enum Precedence {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . () []
    PREC_PRIMARY
} Precedence;

typedef struct ParseRule {
  ParseFn prefix;
  ParseFn infix;
  ParseFn postfix;
  Precedence precedence;
} ParseRule;

Parser parser;
Chunk* compilingChunk;

static Chunk* currentChunk() {
    return compilingChunk;
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
        if (parser.current.type != TOKEN_ERROR) break;

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

static bool check(TokenType type) {
    return type == parser.current.type;
}

static bool match(TokenType type) {
    if (!check(type))
        return false;
    advance();
    return true;
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

static void emitArray(int length) {
    emitConstant(NUMBER_VAL(length)); // constant storing arr length
    emit(OP_ARRAY);
}

// end compilation
static void endCompiler() {
    emit(OP_RETURN);

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void expression();
static ParseRule* getRule(TokenType type);
static void parse(Precedence precedence);
static void statement();
static void declaration();

// parse a number literal
static void number() {
    debugLog("number()");
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
    debugUnlog();
}

static void string() {
    debugLog("string()");
    char *chars = parser.previous.start + 1;
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

static void array() {
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

// parse a literal
static void literal() {
    debugLog("literal()");
    switch (parser.previous.type) {
        case TOKEN_TRUE:
            emit(OP_TRUE);
            break;
        case TOKEN_FALSE:
            emit(OP_FALSE);
            break;
        case TOKEN_NULL:
            emit(OP_NULL);
            break;
        default:
            break;
    }
    debugUnlog();
}

// parse a parenthesized expression
static void grouping() {
    debugLog("grouping()");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after grouping.");
    debugUnlog();
}

// parse unary expression (-a, !a, a++, a--)
static void unary() {
    debugLog("unary()");
    TokenType operatorType = parser.previous.type;

    // Compile the operand.
    parse(PREC_UNARY);

    print("body()");

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_MINUS: 
            emit(OP_NEGATE); 
            break;
        case TOKEN_BANG: 
            emit(OP_NOT); 
            break;
        default: return; // Unreachable.
    }
    print("bass");
    debugUnlog();
    print("ossa");
}

// parse binary expression (a + b, a - b, etc.)
static void binary() {
    debugLog("binary()");
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parse((Precedence)(rule->precedence + 1));

    print("body()");

    switch (operatorType) {
        case TOKEN_PLUS:          
            emit(OP_ADD); break;
        case TOKEN_MINUS:         
            emit(OP_SUBTRACT); break;
        case TOKEN_STAR:          
            emit(OP_MULTIPLY); break;
        case TOKEN_SLASH:         
            emit(OP_DIVIDE); break;
        case TOKEN_BANG_EQUAL:    
            emit2(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   
            emit(OP_EQUAL); break;
        case TOKEN_GREATER:       
            emit(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: 
            emit2(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          
            emit(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    
            emit2(OP_GREATER, OP_NOT); break;
        default: return; // Unreachable.
    }
    debugUnlog();
}

static void indexAccess() {
    debugLog("indexAccess()");
    expression(); // eval index
    consume(TOKEN_RIGHT_BRACKET, "Expected ']' after expression.");
    emit(OP_INDEX_ACCESS);
    debugUnlog();
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after expression");
    emit(OP_PRINT);
}

// pratt table : BEGIN
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = { grouping, NULL,   NULL,        PREC_NONE       },
    [TOKEN_RIGHT_PAREN]   = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_LEFT_BRACE]    = { array,    NULL,   NULL,        PREC_NONE       },
    [TOKEN_RIGHT_BRACE]   = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_COMMA]         = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_DOT]           = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_MINUS]         = { unary,    binary, NULL,        PREC_TERM       },
    [TOKEN_PLUS]          = { NULL,     binary, NULL,        PREC_TERM       },
    [TOKEN_PLUS_PLUS]     = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_MINUS_MINUS]   = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_SEMICOLON]     = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_SLASH]         = { NULL,     binary, NULL,        PREC_FACTOR     },
    [TOKEN_STAR]          = { NULL,     binary, NULL,        PREC_FACTOR     },
    [TOKEN_BANG]          = { unary,    NULL,   NULL,        PREC_NONE       },
    [TOKEN_BANG_EQUAL]    = { NULL,     binary, NULL,        PREC_EQUALITY   },
    [TOKEN_EQUAL]         = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_EQUAL_EQUAL]   = { NULL,     binary, NULL,        PREC_EQUALITY   },
    [TOKEN_GREATER]       = { NULL,     binary, NULL,        PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL] = { NULL,     binary, NULL,        PREC_COMPARISON },
    [TOKEN_LESS]          = { NULL,     binary, NULL,        PREC_COMPARISON },
    [TOKEN_LESS_EQUAL]    = { NULL,     binary, NULL,        PREC_COMPARISON },
    [TOKEN_PLUS_EQUAL]    = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_MINUS_EQUAL]   = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_STAR_EQUAL]    = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_SLASH_EQUAL]   = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_LEFT_BRACKET]  = { NULL,     NULL,   indexAccess, PREC_CALL       },
    [TOKEN_RIGHT_BRACKET] = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_IDENTIFIER]    = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_STRING]        = { string,   NULL,   NULL,        PREC_NONE       },
    [TOKEN_NUMBER]        = { number,   NULL,   NULL,        PREC_NONE       },
    [TOKEN_AND]           = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_CLASS]         = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_ELSE]          = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_FALSE]         = { literal,  NULL,   NULL,        PREC_NONE       },
    [TOKEN_FOR]           = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_FUNC]          = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_IF]            = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_NULL]          = { literal,  NULL,   NULL,        PREC_NONE       },
    [TOKEN_OR]            = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_PRINT]         = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_RETURN]        = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_SUPER]         = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_THIS]          = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_TRUE]          = { literal,  NULL,   NULL,        PREC_NONE       },
    [TOKEN_VAR]           = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_WHILE]         = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_VOID]          = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_STATIC]        = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_ELIF]          = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_BREAK]         = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_CONTINUE]      = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_USING]         = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_PROPERTY]      = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_ERROR]         = { NULL,     NULL,   NULL,        PREC_NONE       },
    [TOKEN_EOF]           = { NULL,     NULL,   NULL,        PREC_NONE       },
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

    prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();

        ParseRule* rule = getRule(parser.previous.type);

        if (rule->infix != NULL) {
            rule->infix();
        } else if (rule->postfix != NULL) {
            rule->postfix();
        }
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
}

static void declaration() {
    statement();
}

// compile source code into bytecode
bool compile(const char* source, Chunk* chunk) {
    initScanner(source);

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