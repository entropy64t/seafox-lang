const fs = require('fs');
const path = require('path');

const packageJsonPath = path.join(__dirname, '..', 'package.json');
const packageJson = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
const files = packageJson.files || [];

const requiredEntries = ['lsp/scanner_bridge', 'lsp/scanner_bridge.c'];
const missing = requiredEntries.filter((entry) => !files.includes(entry));

if (missing.length > 0) {
    throw new Error(`Packaging manifest missing entries: ${missing.join(', ')}`);
}

console.log('packaging manifest includes scanner bridge assets');
