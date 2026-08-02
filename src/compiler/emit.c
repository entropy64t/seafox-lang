#include "compiler_internal.h"

// write byte `b` to the active chunk
void emitByte(byte b) {
	writeChunk(currentChunk(), b, parser.previous.line);
}

// write bytes `b1` and `b2` to the active chunk
void emitBytes(byte b1, byte b2) {
	emitByte(b1);
	emitByte(b2);
}

// write `n` bytes to the active chunk
void emitN(size_t n, ...) {
	va_list args;

	va_start(args, n); // Initialize
	for (size_t i = 0; i < n; i++) {
		int bt = va_arg(args, int); // Get next argument
		emitByte((byte) bt);
	}
	va_end(args); // Cleanup
}

// emit a constant
int emitConstant(Value value) {
	Opcode opcode = OP_CONSTANT_8;
	int constant = addConstant(currentChunk(), value);
	if (constant > BYTE_MAX) {
		opcode = OP_CONSTANT_24;
	}
	else if (constant > 3 * BYTE_MAX) {
		error("Too many constants in one chunk.");
		return -1;
	}

	emitBytes(opcode, (byte) constant);
	return constant;
}

// emit op to create an array
void emitArray(int length) {
	emitConstant(NUMBER_VAL(length)); // constant storing arr length
	emitByte(OP_ARRAY);
}

// emit instruction `jumpType` with 2 bytes for operands and return its location
int emitJump(byte jumpType) {
	emitByte(jumpType);
	int location = currentChunk()->count;
	emitBytes(0xff, 0xff);
	return location;
}

// patch a jump's oparand
void patchJump(int location) {
	// -2 to adjust for the jump's operand
	int jump = currentChunk()->count - location - 2;

	if (jump > UINT16_MAX) {
		error("Too much code to jump over.");
	}

	currentChunk()->code[location] = (jump >> 8) & 0xff;
	currentChunk()->code[location + 1] = jump & 0xff;
}

void emitLoop(int loopStart) {
	emitByte(OP_LOOP);

	int offset = currentChunk()->count - loopStart + 2;
	if (offset > UINT16_MAX)
		error("Loop body too large.");

	emitByte((offset >> 8) & 0xff);
	emitByte(offset & 0xff);
}
