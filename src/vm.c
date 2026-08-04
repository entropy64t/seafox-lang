#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "value.h"
#include "debug.h"
#include "compiler.h"
#include "runtime_error.h"
#include "object.h"
#include "fox_memory.h"
#include "hashtable.h"

#include "seafox_natives.h"

VM vm;

static void resetStack() {
	vm.stackTop = vm.stack;
	vm.frameCount = 0;
}

static void printStack() {
	fprintf(stderr, "Stack: ");
	if (vm.stackTop == vm.stack) {
		fprintf(stderr, "-- empty --");
	}

	for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
		fprintf(stderr, "[ ");
		fprintValue(stderr, *slot, "");
		fprintf(stderr, " ]");
	}
	fprintf(stderr, "\n");
}

static Value peek(int distance) {
	return vm.stackTop[-1 - distance];
}

static void defineNative(const char* name, NativeFn function, int arity) {
	push(OBJ_VAL(copyString(name, (int) strlen(name))));
	push(OBJ_VAL(newNativeFn(function, name, arity)));
	tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
	pop();
	pop();
}

static void concatenate() {
	ObjString* b = AS_STRING(pop());
	ObjString* a = AS_STRING(pop());

	int length = a->length + b->length;
	char* chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjString* result = takeString(chars, length);
	push(OBJ_VAL(result));
}

static void concatArray() {
	ObjArray* b = AS_ARRAY(pop());
	ObjArray* a = AS_ARRAY(pop());

	int length = a->length + b->length;
	Value* items = ALLOCATE(Value, length);

	memcpy(items, a->items, a->length * sizeof(Value));

	memcpy(items + a->length, b->items, b->length * sizeof(Value));

	ObjArray* result = takeArray(items, length);
	push(OBJ_VAL(result));
}

static bool add() {
	Value av = peek(1);
	Value bv = peek(0);

	if (IS_STRING(av) && IS_STRING(bv)) {
		concatenate();
		return true; // success
	}
	else if (IS_NUMBER(av) && IS_NUMBER(bv)) {
		double b = AS_NUMBER(pop());
		double a = AS_NUMBER(pop());
		push(NUMBER_VAL(a + b));
		return true; // success
	}
	else if (IS_ARRAY(av) && IS_ARRAY(bv)) {
		concatArray();
		return true;
	}
	else if (IS_STRING(av)) {
		runtimeError("Expected string as right operand, got %T.", &bv); // peek(1) is not string
	}
	else if (IS_NUMBER(av)) {
		runtimeError("Expected number as right operand, got %T.", &bv);
	}
	else if (IS_ARRAY(av)) {
		runtimeError("Expected array as right operand, got %T", &bv);
	}
	else {
		runtimeError("Expected string or number as left operand, got %T.", &av);
	}
	return false;
}

static bool array() {
	int length = AS_NUMBER(pop());
	Value* items = ALLOCATE(Value, length);

	for (int i = length - 1; i >= 0; i--) {
		items[i] = pop();
	}

	ObjArray* result = copyArray(items, length);
	push(OBJ_VAL(result));
	return true;
}

static bool indexString(ObjString* string, int index) {
	if (index < 0 || index >= string->length) {
		runtimeError("Index must be in range [0, %d), is %d", string->length, index);
		return false;
	}
	char* single = &string->chars[index];
	push(OBJ_VAL(copyString(single, 1)));
	return true;
}

static bool indexArray(ObjArray* arr, int index) {
	if (index < 0 || index >= arr->length) {
		runtimeError("Index must be in range [0, %d), is %d", arr->length, index);
		return false;
	}
	Value single = arr->items[index];
	push(single);
	return true;
}

static bool indexGet() {
	Value index = pop();
	if (!IS_NUMBER(index)) {
		runtimeError("Expresion must be a number, not %T", &index);
		return false;
	}

	int intValue = AS_NUMBER(index);
	if (intValue != AS_NUMBER(index)) {
		runtimeError("Array indices must be integers.");
		return false;
	}

	Value array = pop();
	if (!IS_OBJECT(array)) {
		runtimeError("Can only index arrays or strings, not %T.", &array);
		return false;
	}

	switch (AS_OBJECT(array)->type) {
		case OBJ_STRING:
			return indexString(AS_STRING(array), intValue);
		case OBJ_ARRAY:
			return indexArray(AS_ARRAY(array), intValue);
		default:
			runtimeError("Can only index arrays or strings, not %T", &array);
			return false;
	}
	return true;
}

static bool indexSet() {
	Value target = pop();
	if (!IS_NUMBER(peek(0))) {
		runtimeError("Array indices must be numbers");
		return false;
	}
	if (!IS_ARRAY(peek(1))) {
		runtimeError("Can only assign to array indices");
		return false;
	}
	double d = AS_NUMBER(pop());
	int index = d;
	if (d != index) {
		runtimeError("Array indices must be integers");
		return false;
	}
	ObjArray* array = AS_ARRAY(peek(0));
	if (index >= array->length) {
		runtimeError("Out of array range. Index: %d, length: %d", index, array->length);
		return false;
	}
	array->items[index] = target;
	return true;
}

static bool call(ObjFunction* function, byte argCount) {
	if (argCount != function->arity) {
		runtimeError("Expected %d arguments, got %d", function->arity, argCount);
		return false;
	}

	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow");
		return false;
	}

	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->function = function;
	frame->ip = function->chunk.code;
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

static bool callValue(Value callee, byte argCount) {
	if (IS_OBJECT(callee)) {
		switch (OBJ_TYPE(callee)) {
			case OBJ_FUNCTION:
				return call(AS_FUNCTION(callee), argCount);
				break;
			case OBJ_NATIVE_FN:
				ObjNativeFn* native = AS_NATIVE(callee);
				if (native->arity != -1 && argCount != native->arity) {
					runtimeError("Native function %s expected %d arguments, got %d", native->name, native->arity, argCount);
					return false;
				}
				NativeFn function = native->function;
				Value result;
				bool ok = function(argCount, vm.stackTop - argCount, &result);
				if (!ok)
					return false;
				vm.stackTop -= argCount + 1;
				push(result);
				return true;

			default:
				break;
		}
	}
	runtimeError("Can only call functions");
	return false;
}

void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	rte(format, args);
	va_end(args);
	fputs("\n", stderr);

	for (int i = vm.frameCount - 1; i >= 0; --i) {
		CallFrame* frame = &vm.frames[i];
		ObjFunction* function = frame->function;
		size_t instruction = frame->ip - function->chunk.code - 1;

		int line = getLine(&frame->function->chunk, instruction);
		fprintf(stderr, "[line %d] in ", line);
		if (function->name == NULL)
			fprintf(stderr, "script\n");
		else
			fprintf(stderr, "%s()\n", function->name->chars);
	}

	resetStack();
}

static InterpretResult run() {
	CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_UINT16() ((uint16_t) (((uint16_t) READ_BYTE() << 8) | (uint16_t) READ_BYTE()))
#define READ_UINT24() ((uint32_t) (((uint32_t) READ_UINT16() << 8) | (uint32_t) READ_BYTE()))
#define READ_UINT32() ((uint32_t) (((uint32_t) READ_UINT24() << 8) | (uint32_t) READ_BYTE()))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (frame->function->chunk.constants.values[READ_UINT24()])
#define BINARY_OP(valueType, op)                                                     \
	do {                                                                             \
		if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                            \
			Value v1 = peek(1);                                                      \
			Value v2 = peek(0);                                                      \
			runtimeError("Both operands must be numbers, not %T and %T.", &v1, &v2); \
			return INTERPRET_RUNTIME_ERROR;                                          \
		}                                                                            \
		double b = AS_NUMBER(pop());                                                 \
		double a = AS_NUMBER(pop());                                                 \
		push(valueType(a op b));                                                     \
	} while (false)
#define READ_STRING() AS_STRING(READ_CONSTANT())

	for (;;) {
		byte instruction;
		Value constant;

#ifdef DEBUG_TRACE_EXECUTION
		fprintf(stderr, "Stack before:\n");
		printStack();
		disassembleInstruction(stderr, &frame->function->chunk, (int) (frame->ip - frame->function->chunk.code));
#endif

		switch (instruction = READ_BYTE()) {
			case OP_CONSTANT_8:
				constant = READ_CONSTANT();
				push(constant);
				break;
			case OP_CONSTANT_24:
				constant = READ_CONSTANT_LONG();
				push(constant);
				break;
			case OP_TRUE:
				push(BOOL_VAL(true));
				break;
			case OP_FALSE:
				push(BOOL_VAL(false));
				break;
			case OP_NULL:
				push(NULL_VAL);
				break;
			case OP_NEGATE:
				if (!IS_NUMBER(peek(0))) {
					Value p = peek(0);
					runtimeError("Value to negate must be a number, not %T.", &p);
					return INTERPRET_RUNTIME_ERROR;
				}
				push(NUMBER_VAL(-AS_NUMBER(pop())));
				break;
			case OP_NOT:
				push(BOOL_VAL(isFalsey(pop())));
				break;
			case OP_ADD:
				if (!add())
					return INTERPRET_RUNTIME_ERROR;
				break;
			case OP_SUBTRACT:
				BINARY_OP(NUMBER_VAL, -);
				break;
			case OP_MULTIPLY:
				BINARY_OP(NUMBER_VAL, *);
				break;
			case OP_DIVIDE:
				BINARY_OP(NUMBER_VAL, /);
				break;
			case OP_EQUAL:
				{
					Value b = pop();
					Value a = pop();
					push(BOOL_VAL(valuesEqual(a, b)));
				}
				break;
			case OP_GREATER:
				BINARY_OP(BOOL_VAL, >);
				break;
			case OP_LESS:
				BINARY_OP(BOOL_VAL, <);
				break;
			case OP_ARRAY:
				if (!array())
					return INTERPRET_RUNTIME_ERROR;
				break;
			case OP_INDEX_GET:
				if (!indexGet())
					return INTERPRET_RUNTIME_ERROR;
				break;
			case OP_INDEX_SET:
				if (!indexSet())
					return INTERPRET_RUNTIME_ERROR;
				break;
			case OP_PRINT:
				printValue(pop(), "\n");
				break;
			case OP_POP:
				pop();
				break;
			case OP_DEFINE_GLOBAL:
				{
					ObjString* name = READ_STRING();
					tableSet(&vm.globals, name, peek(0));
					pop();
					pop();
					break;
				}
			case OP_GET_GLOBAL:
				{
					ObjString* name = READ_STRING();
					Value var;
					if (!tableGet(&vm.globals, name, &var)) {
						runtimeError("Undefined global variable: '%s'.", name->chars);
						return INTERPRET_RUNTIME_ERROR;
					}
					pop();
					push(var);
					break;
				}
			case OP_SET_GLOBAL:
				{
					ObjString* name = READ_STRING();
					Value top = peek(0);
					if (tableSet(&vm.globals, name, top)) {
						tableDelete(&vm.globals, name);
						runtimeError("Undefined global variable: '%s'.", name->chars);
						return INTERPRET_RUNTIME_ERROR;
					}
					pop();
					pop();
					push(top);
					break;
				}
			case OP_GET_LOCAL:
				{
					byte slot = READ_BYTE();
					push(frame->slots[slot]);
					break;
				}
			case OP_SET_LOCAL:
				{
					byte slot = READ_BYTE();
					frame->slots[slot] = peek(0);
					break;
				}
			case OP_JUMP_IF_TRUE:
				{
					uint16_t jump = READ_UINT16();
					if (!isFalsey(peek(0)))
						frame->ip += jump;
					break;
				}
			case OP_JUMP_IF_FALSE:
				{
					uint16_t jump = READ_UINT16();
					if (isFalsey(peek(0)))
						frame->ip += jump;
					break;
				}
			case OP_JUMP:
				{
					uint16_t jump = READ_UINT16();
					frame->ip += jump;
					break;
				}
			case OP_LOOP:
				{
					uint16_t jump = READ_UINT16();
					frame->ip -= jump;
					break;
				}
			case OP_CALL:
				{
					int argCount = READ_BYTE();
					if (!callValue(peek(argCount), argCount)) {
						return INTERPRET_RUNTIME_ERROR;
					}
					frame = &vm.frames[vm.frameCount - 1];
					break;
				}
			case OP_RETURN:
				{
					Value result = pop();
					vm.frameCount--;
					if (vm.frameCount == 0) {
						// end of top-level code, stop execution
						pop(); // pop the implicit <script> function CallFrame
						return INTERPRET_OK;
					}

					vm.stackTop = frame->slots;
					push(result);
					frame = &vm.frames[vm.frameCount - 1];
					break;
				}

			default:
				return INTERPRET_RUNTIME_ERROR;
		}
	}

#undef READ_BYTE
#undef READ_UINT16
#undef READ_UINT24
#undef READ_UINT32
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef BINARY_OP
#undef READ_STRING
}

static void dumpChunk(Chunk* chunk, char* bytecodePath, char* tracePath) {
	writeChunkToFile(chunk, bytecodePath);
	disassembleChunkToFile(chunk, "code", tracePath);
}

void initVM() {
	resetStack();
	vm.objects = NULL;
	initTable(&vm.strings);
	initTable(&vm.globals);

	defineNative("getTime", getTimeNative, 0);
	defineNative("write", writeNative, -1);
	defineNative("writeln", writelnNative, -1);
	defineNative("readln", readlnNative, 0);
	defineNative("array", arrayNative, 1);
	defineNative("number", numberNative, 1);
}

void freeVM() {
	freeTable(&vm.strings);
	freeTable(&vm.globals);
	freeObjects();
}

InterpretResult interpret(const char* source, char* bytecodePath, const char* tracePath) {
	ObjFunction* function = compile(source);
	if (function == NULL)
		return INTERPRET_COMPILE_ERROR;

	dumpChunk(&function->chunk, bytecodePath, tracePath);

	push(OBJ_VAL(function));
	call(function, 0); // call the top-level function

	return run();
}

void push(Value v) {
	*vm.stackTop = v;
	vm.stackTop++;
}

Value pop() {
	vm.stackTop--;
	return *vm.stackTop;
}