const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const lspDir = __dirname;
const sourcePath = path.join(lspDir, 'scanner_bridge.c');
const outputPath = path.join(lspDir, 'scanner_bridge');

if (!fs.existsSync(sourcePath)) {
    console.error('scanner_bridge.c not found');
    process.exit(1);
}

if (fs.existsSync(outputPath)) {
    console.log('scanner bridge already built, rebuilding');
    // process.exit(0);
}

try {
    execFileSync('cc', ['-O2', '-Wall', '-Wextra', '-std=c99', '-o', outputPath, sourcePath, '-ldl'], {
        stdio: 'inherit',
    });
    console.log('built scanner bridge');
} catch (error) {
    console.error('failed to build scanner bridge');
    process.exit(error.status || 1);
}
