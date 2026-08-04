const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const bridgePath = path.join(__dirname, 'scanner_bridge');

const TokenType = {
    TOKEN_LEFT_PAREN: 0,
    TOKEN_RIGHT_PAREN: 1,
    TOKEN_LEFT_BRACE: 2,
    TOKEN_RIGHT_BRACE: 3,
    TOKEN_COMMA: 4,
    TOKEN_DOT: 5,
    TOKEN_MINUS: 6,
    TOKEN_PLUS: 7,
    TOKEN_PLUS_PLUS: 8,
    TOKEN_MINUS_MINUS: 9,
    TOKEN_SEMICOLON: 10,
    TOKEN_SLASH: 11,
    TOKEN_STAR: 12,
    TOKEN_BANG: 13,
    TOKEN_BANG_EQUAL: 14,
    TOKEN_EQUAL: 15,
    TOKEN_EQUAL_EQUAL: 16,
    TOKEN_GREATER: 17,
    TOKEN_GREATER_EQUAL: 18,
    TOKEN_LESS: 19,
    TOKEN_LESS_EQUAL: 20,
    TOKEN_FORWARD: 21,
    TOKEN_PLUS_EQUAL: 22,
    TOKEN_MINUS_EQUAL: 23,
    TOKEN_STAR_EQUAL: 24,
    TOKEN_SLASH_EQUAL: 25,
    TOKEN_LEFT_BRACKET: 26,
    TOKEN_RIGHT_BRACKET: 27,
    TOKEN_IDENTIFIER: 28,
    TOKEN_STRING: 29,
    TOKEN_NUMBER: 30,
    TOKEN_AND: 31,
    TOKEN_CLASS: 32,
    TOKEN_ELSE: 33,
    TOKEN_FALSE: 34,
    TOKEN_FOR: 35,
    TOKEN_FUNCTION: 36,
    TOKEN_IF: 37,
    TOKEN_NULL: 38,
    TOKEN_OR: 39,
    TOKEN_PRINT: 40,
    TOKEN_RETURN: 41,
    TOKEN_SUPER: 42,
    TOKEN_THIS: 43,
    TOKEN_TRUE: 44,
    TOKEN_VAR: 45,
    TOKEN_WHILE: 46,
    TOKEN_CONST: 47,
    TOKEN_LAMBDA: 48,
    TOKEN_STATIC: 49,
    TOKEN_ELIF: 50,
    TOKEN_BREAK: 51,
    TOKEN_CONTINUE: 52,
    TOKEN_USING: 53,
    TOKEN_PROPERTY: 54,
    TOKEN_ERROR: 55,
    TOKEN_EOF: 56,
};

TokenType.boolean = () => [TokenType.TOKEN_TRUE, TokenType.TOKEN_FALSE];
TokenType.keyword = () => [
    TokenType.TOKEN_AND,
    TokenType.TOKEN_CLASS,
    TokenType.TOKEN_ELSE,
    TokenType.TOKEN_FOR,
    TokenType.TOKEN_FUNCTION,
    TokenType.TOKEN_IF,
    TokenType.TOKEN_OR,
    TokenType.TOKEN_PRINT,
    TokenType.TOKEN_RETURN,
    TokenType.TOKEN_SUPER,
    TokenType.TOKEN_THIS,
    TokenType.TOKEN_VAR,
    TokenType.TOKEN_WHILE,
    TokenType.TOKEN_CONST,
    TokenType.TOKEN_LAMBDA,
    TokenType.TOKEN_STATIC,
    TokenType.TOKEN_ELIF,
    TokenType.TOKEN_BREAK,
    TokenType.TOKEN_CONTINUE,
    TokenType.TOKEN_USING,
    TokenType.TOKEN_PROPERTY,
];

Object.freeze(TokenType);

function ensureBridgeAvailable() {
    if (!fs.existsSync(bridgePath)) {
        throw new Error('Scanner bridge not found. Run node lsp/build-scanner.js first.');
    }
}

class Scanner {
    constructor(source) {
        this.source = source;
    }

    *tokens() {
        ensureBridgeAvailable();
        const result = spawnSync(bridgePath, {
            input: this.source,
            encoding: 'utf8',
            maxBuffer: 1024 * 1024 * 16,
        });

        if (result.status !== 0) {
            const details = (result.stderr || '').trim() || 'scanner bridge failed';
            throw new Error(`scanner bridge error: ${details}`);
        }

        for (const line of (result.stdout || '').split(/\r?\n/)) {
            if (!line.trim()) {
                continue;
            }
            const token = JSON.parse(line);
            yield {
                type: token.type,
                text: token.text,
                line: token.line,
                position: token.position,
                length: token.length,
            };
        }
    }
}

function parse(source) {
    const data = [];
    let previousLine = 0;
    let previousStart = 0;
    let previousLineStart = 0;
    const natives = ['getTime', 'write', 'writeln', 'readln', 'number'];
    const symbols = Object.fromEntries(natives.map((name) => [name, 'function']));
    let previous = null;
    let inArgList = false;

    for (const token of new Scanner(source).tokens()) {
        let tokenType = undefined;
        let isReadonly = false;

        if (token.type == TokenType.TOKEN_LAMBDA) {
            inArgList = true;
        }

        if (inArgList && (token.type === TokenType.TOKEN_RIGHT_PAREN || token.type === TokenType.TOKEN_FORWARD)) {
            inArgList = false;
        }

        if (token.type === TokenType.TOKEN_IDENTIFIER) {
            const name = token.text;
            if (!(name in symbols)) {
                if (!inArgList) {
                    symbols[name] = previous;
                } else {
                    symbols[name] = 'param';
                    isReadonly = previous === 'const';
                }
            }

            const kind = symbols[name];
            if (kind === 'var') {
                tokenType = 0;
            } else if (kind === 'function') {
                tokenType = 1;
            } else if (kind === 'class') {
                tokenType = 2;
            } else if (kind === 'const') {
                tokenType = 0;
            } else if (kind === 'param') {
                tokenType = 3;
            } else if (kind === null) {
                tokenType = undefined;
            }

            if (previous === 'function') {
                inArgList = true;
            }
        }

        if (tokenType === undefined) {
            if (token.type === TokenType.TOKEN_STRING) {
                tokenType = 4;
            } else if (token.type === TokenType.TOKEN_NUMBER) {
                tokenType = 5;
            } else if (token.type === TokenType.TOKEN_NULL) {
                tokenType = 7;
            } else if (TokenType.boolean().includes(token.type)) {
                tokenType = 6;
            } else if (TokenType.keyword().includes(token.type)) {
                // skip keywords so they can be colored by text mate
                tokenType = -1;
            }
        }

        if (tokenType === undefined) {
            previous = previousTokenKind(token.type);
            continue;
        }

        const line = token.line - 1;
        const absoluteStart = token.position;
        const lineStart = line === previousLine ? previousLineStart : getLineStart(source, line);
        const start = absoluteStart - lineStart;
        const deltaStart = line === previousLine ? start - previousStart : start;

        data.push(line - previousLine, deltaStart, token.length, tokenType, isReadonly ? 1 : 0);

        previousLine = line;
        previousStart = start;
        previousLineStart = lineStart;
        previous = previousTokenKind(token.type);
    }

    return data;
}

function getLineStart(source, line) {
    let offset = 0;
    let currentLine = 0;
    const newline = /\r?\n/;

    for (let index = 0; index < source.length; index += 1) {
        if (currentLine === line) {
            return offset;
        }

        const char = source[index];
        if (char === '\n') {
            currentLine += 1;
            offset = index + 1;
        } else if (char === '\r') {
            if (source[index + 1] === '\n') {
                index += 1;
            }
            currentLine += 1;
            offset = index + 1;
        }
    }

    return offset;
}

function previousTokenKind(type) {
    switch (type) {
        case TokenType.TOKEN_VAR:
            return 'var';
        case TokenType.TOKEN_LAMBDA:
        case TokenType.TOKEN_FUNCTION:
            return 'function';
        case TokenType.TOKEN_CONST:
            return 'const';
        case TokenType.TOKEN_CLASS:
            return 'class';
        default:
            return null;
    }
}

module.exports = {
    TokenType,
    Scanner,
    parse,
};
