const { parse } = require('./server');

const source = `
function f(x) {
    if (x < 1) return 1;
    return f(x / 2) * x;
}

const v = f(5);
writeln(v);
`;

const result = parse(source);
console.log(JSON.stringify(result.slice(0, 24)));
console.log(`token count: ${result.length / 5}`);

const indented = parse('first\n   write("")\n');
if (indented[0] !== 1 || indented[1] !== 3) {
    throw new Error(`Expected indented token to start at column 3, got ${indented.slice(0, 8)}`);
}
