const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const bridgePath = path.join(__dirname, 'scanner_bridge');

let nextTokenTypeValue = 0;
function nextTokenType() {
    return nextTokenTypeValue++;
}

const TokenType = {
    TOKEN_LEFT_PAREN: nextTokenType(),
    TOKEN_RIGHT_PAREN: nextTokenType(),
    TOKEN_LEFT_BRACE: nextTokenType(),
    TOKEN_RIGHT_BRACE: nextTokenType(),
    TOKEN_COMMA: nextTokenType(),
    TOKEN_DOT: nextTokenType(),
    TOKEN_MINUS: nextTokenType(),
    TOKEN_PLUS: nextTokenType(),
    TOKEN_PLUS_PLUS: nextTokenType(),
    TOKEN_MINUS_MINUS: nextTokenType(),
    TOKEN_SEMICOLON: nextTokenType(),
    TOKEN_SLASH: nextTokenType(),
    TOKEN_STAR: nextTokenType(),
    TOKEN_QMARK: nextTokenType(),
    TOKEN_COLON: nextTokenType(),
    TOKEN_BANG: nextTokenType(),
    TOKEN_BANG_EQUAL: nextTokenType(),
    TOKEN_EQUAL: nextTokenType(),
    TOKEN_EQUAL_EQUAL: nextTokenType(),
    TOKEN_GREATER: nextTokenType(),
    TOKEN_GREATER_EQUAL: nextTokenType(),
    TOKEN_LESS: nextTokenType(),
    TOKEN_LESS_EQUAL: nextTokenType(),
    TOKEN_FORWARD: nextTokenType(),
    TOKEN_PLUS_EQUAL: nextTokenType(),
    TOKEN_MINUS_EQUAL: nextTokenType(),
    TOKEN_STAR_EQUAL: nextTokenType(),
    TOKEN_SLASH_EQUAL: nextTokenType(),
    TOKEN_LEFT_BRACKET: nextTokenType(),
    TOKEN_RIGHT_BRACKET: nextTokenType(),
    TOKEN_IDENTIFIER: nextTokenType(),
    TOKEN_STRING: nextTokenType(),
    TOKEN_NUMBER: nextTokenType(),
    TOKEN_AND: nextTokenType(),
    TOKEN_CLASS: nextTokenType(),
    TOKEN_ELSE: nextTokenType(),
    TOKEN_FALSE: nextTokenType(),
    TOKEN_FOR: nextTokenType(),
    TOKEN_FUNCTION: nextTokenType(),
    TOKEN_IF: nextTokenType(),
    TOKEN_NULL: nextTokenType(),
    TOKEN_OR: nextTokenType(),
    TOKEN_PRINT: nextTokenType(),
    TOKEN_RETURN: nextTokenType(),
    TOKEN_SUPER: nextTokenType(),
    TOKEN_THIS: nextTokenType(),
    TOKEN_TRUE: nextTokenType(),
    TOKEN_VAR: nextTokenType(),
    TOKEN_WHILE: nextTokenType(),
    TOKEN_CONST: nextTokenType(),
    TOKEN_LAMBDA: nextTokenType(),
    TOKEN_IS: nextTokenType(),
    TOKEN_NOT: nextTokenType(),
    TOKEN_STATIC: nextTokenType(),
    TOKEN_ELIF: nextTokenType(),
    TOKEN_BREAK: nextTokenType(),
    TOKEN_CONTINUE: nextTokenType(),
    TOKEN_USING: nextTokenType(),
    TOKEN_PROPERTY: nextTokenType(),
    TOKEN_ERROR: nextTokenType(),
    TOKEN_EOF: nextTokenType(),
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
    TokenType.TOKEN_IS,
    TokenType.TOKEN_NOT,
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
    const natives = ['getTime', 'write', 'writeln', 'readln', 'number', 'array', 'type', 'length', 'isEnd', 'next', 'begin'];
    const types = ['Number', 'Bool', 'Null', 'String', 'Array', 'Function', 'Type', 'Iterator'];
    let symbols = Object.fromEntries(natives.map((name) => [name, 'function']));
    for (const type of types) {
        symbols[type] = 'class';
    }
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
