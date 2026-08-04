const { createConnection, TextDocuments, SemanticTokensRequest, ProposedFeatures } = require('vscode-languageserver/node');
const { TextDocument } = require('vscode-languageserver-textdocument');
const { parse } = require('./scanner');

let connection;
let documents;

const TOKEN_TYPES = [
    'variable',
    'function',
    'class',
    'parameter',
    'string',
    'number',
    'boolean',
    'null',
    'keyword',
];

const TOKEN_MODIFIERS = ['readonly'];

function parseSemanticTokens(source) {
    return parse(source);
}

function startServer() {
    connection = createConnection(ProposedFeatures.all, process.stdin, process.stdout);
    documents = new TextDocuments(TextDocument);

    connection.onInitialize(() => ({
        capabilities: {
            semanticTokensProvider: {
                legend: {
                    tokenTypes: TOKEN_TYPES,
                    tokenModifiers: TOKEN_MODIFIERS,
                },
                range: false,
                full: {
                    delta: false,
                },
            },
        },
    }));

    connection.onRequest(SemanticTokensRequest.type, (params) => {
        const document = documents.get(params.textDocument.uri);
        if (!document) {
            return { data: [] };
        }

        return {
            data: parseSemanticTokens(document.getText()),
        };
    });

    documents.listen(connection);
    connection.listen();
}

if (require.main === module) {
    startServer();
}

module.exports = {
    parse: parseSemanticTokens,
};
