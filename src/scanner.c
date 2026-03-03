#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct
{
    const char* start;
    const char* current;
    int line;
} Scanner;

Scanner scanner;

static bool isAtEnd() {
    return *scanner.current == '\0';
}

static Token makeToken(TokenType type) {
    Token token = {
        .type = type,
        .start = scanner.start,
        .length = (int)(scanner.current - scanner.start),
        .line = scanner.line
    };
    return token;
}

static Token errorToken(const char* message) {
    Token token = {
        .type = TOKEN_ERROR,
        .start = message,
        .length = (int)strlen(message),
        .line = scanner.line
    };
    return token;
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

static bool isDigit(char c) {
    return '0' <= c && c <= '9';
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_';
}

static bool isAlnum(char c) {
    return isAlpha(c) || isDigit(c);
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

/// @brief 
/// @return current character 
static char peek() {
    return *scanner.current;
}

static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static bool comment() {
    if (peekNext() == '/') {
        while (peek() != '\n' && !isAtEnd()) {
            advance();
        }
        return true;
    }
    else if (peekNext() == '*') {
        advance(); advance(); // advance past the initial '/*'

        while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
            if (peek() == '\n') scanner.line++;
            advance();
        }

        advance(); advance(); // skip past the trailing '*/'
        return true;
    }
    return false;
}

static void skipWhitespace() {
    for (;;) {
        char c = peek();

        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (comment()) break;
                else return;
            default:
                return;
        }
    }
}

static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string literal.");

    advance(); // advance past the closing quote

    return makeToken(TOKEN_STRING);
}

static Token number() {
    while (isDigit(peek())) advance();

    if (peek() == '.' && isDigit(peekNext())) {
        advance(); // past the dot
        while (isDigit(peek())) {
            advance();
        }
    }

    return makeToken(TOKEN_NUMBER);
}

static TokenType identifierType() {
// identifier lookup : BEGIN
    int length = scanner.current - scanner.start;
    switch (length) {
        case 2:
            if (memcmp(scanner.start, "if", 2) == 0) return TOKEN_IF;
            if (memcmp(scanner.start, "or", 2) == 0) return TOKEN_OR;
            break;

        case 3:
            if (memcmp(scanner.start, "and", 3) == 0) return TOKEN_AND;
            if (memcmp(scanner.start, "for", 3) == 0) return TOKEN_FOR;
            if (memcmp(scanner.start, "var", 3) == 0) return TOKEN_VAR;
            break;

        case 4:
            if (memcmp(scanner.start, "else", 4) == 0) return TOKEN_ELSE;
            if (memcmp(scanner.start, "func", 4) == 0) return TOKEN_FUNC;
            if (memcmp(scanner.start, "null", 4) == 0) return TOKEN_NULL;
            if (memcmp(scanner.start, "this", 4) == 0) return TOKEN_THIS;
            if (memcmp(scanner.start, "true", 4) == 0) return TOKEN_TRUE;
            if (memcmp(scanner.start, "void", 4) == 0) return TOKEN_VOID;
            if (memcmp(scanner.start, "elif", 4) == 0) return TOKEN_ELIF;
            break;

        case 5:
            if (memcmp(scanner.start, "class", 5) == 0) return TOKEN_CLASS;
            if (memcmp(scanner.start, "false", 5) == 0) return TOKEN_FALSE;
            if (memcmp(scanner.start, "print", 5) == 0) return TOKEN_PRINT;
            if (memcmp(scanner.start, "super", 5) == 0) return TOKEN_SUPER;
            if (memcmp(scanner.start, "while", 5) == 0) return TOKEN_WHILE;
            if (memcmp(scanner.start, "break", 5) == 0) return TOKEN_BREAK;
            if (memcmp(scanner.start, "using", 5) == 0) return TOKEN_USING;
            break;

        case 6:
            if (memcmp(scanner.start, "return", 6) == 0) return TOKEN_RETURN;
            if (memcmp(scanner.start, "static", 6) == 0) return TOKEN_STATIC;
            break;

        case 8:
            if (memcmp(scanner.start, "continue", 8) == 0) return TOKEN_CONTINUE;
            if (memcmp(scanner.start, "property", 8) == 0) return TOKEN_PROPERTY;
            break;
    }

    return TOKEN_IDENTIFIER;
// identifier lookup : END
}

static Token identifier() {
    while (isAlpha(peek()) || isDigit(peek())) advance();

    return makeToken(identifierType());
}

void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

/// Get the next token.
///
/// (performance matters)
/// @return 
Token scan() {
#define MAKE_MATCHING(expected, matches, other) makeToken(match(expected) ? (matches) : (other))
#define MAKE_MATCHING_3(e1, e2, m1, m2, other) makeToken(match(e1) ? (m1) : (match(e2) ? (m2) : (other)))
    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();

    if (isDigit(c)) return number();
    if (isAlpha(c)) return identifier();

    switch (c) {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case '[': return makeToken(TOKEN_LEFT_BRACKET);
        case ']': return makeToken(TOKEN_RIGHT_BRACKET);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);

        case '+': return MAKE_MATCHING_3('+', '=', TOKEN_PLUS_PLUS, TOKEN_PLUS_EQUAL, TOKEN_PLUS);
        case '-': return MAKE_MATCHING_3('-', '=', TOKEN_MINUS_MINUS, TOKEN_MINUS_EQUAL, TOKEN_MINUS);
        case '/': return MAKE_MATCHING('=', TOKEN_SLASH_EQUAL, TOKEN_SLASH);
        case '*': return MAKE_MATCHING('=', TOKEN_STAR_EQUAL, TOKEN_STAR);

        case '!': return MAKE_MATCHING('=', TOKEN_BANG_EQUAL, TOKEN_BANG);
        case '=': return MAKE_MATCHING('=', TOKEN_EQUAL_EQUAL, TOKEN_EQUAL);
        case '<': return MAKE_MATCHING('=', TOKEN_LESS_EQUAL, TOKEN_LESS);
        case '>': return MAKE_MATCHING('=', TOKEN_GREATER_EQUAL, TOKEN_GREATER);

        case '"': return string();
    }

    return errorToken("Unexpected character.");
#undef MAKE_MATCHING
}