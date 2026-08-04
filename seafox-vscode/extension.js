const vscode = require("vscode");
const {
    LanguageClient
} = require("vscode-languageclient/node");
const path = require("path");
const { execFileSync } = require("child_process");

let client;

function activate(context) {
    const output = vscode.window.createOutputChannel("seafox LSP debug");
    output.show();

    console.log("seafox LSP STARTING....");

    output.appendLine("Starting seafox language server...");

    const serverPath = path.join(
        context.extensionPath,
        "lsp",
        "server.js"
    );

    const buildScript = path.join(context.extensionPath, "lsp", "build-scanner.js");

    try {
        execFileSync(process.execPath, [buildScript], {
            stdio: "ignore"
        });
    } catch (error) {
        console.warn("seafox scanner build skipped:", error.message);
    }

    client =
        new LanguageClient(
            "seafox",
            "seafox Server",
            {
                command: process.execPath,
                args: [
                    serverPath
                ]
            },
            {
                documentSelector: [
                    {
                        language: "seafox"
                    }
                ],
                outputChannelName: "seafox LSP debug"
            }
        );

    client.onDidChangeState((event) => {
        console.log("LSP state:", event);
        output.appendLine(`LSP State: ${event}`);
    });
    console.log("server:", serverPath);
    console.log("runtime:", process.execPath);
    output.appendLine(`server: ${serverPath}`);
    output.appendLine(`runtime: ${process.execPath}`);

    context.subscriptions.push(output);
    context.subscriptions.push(client);
    client.start();
}


function deactivate() { }

module.exports = {
    activate,
    deactivate
};