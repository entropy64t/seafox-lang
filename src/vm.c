#include <stdio.h>

#include "common.h"
#include "vm.h"
#include "value.h"
#include "debug.h"
#include "compiler.h"

VMachine vm;

static void resetStack() {
    vm.stackTop = vm.stack;
}

static void printStack() {
    printf("Stack: ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
    }
    printf("\n");
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (vm.chunk->constants.values[((int)READ_BYTE() << 16) + ((int)READ_BYTE() << 8) + (int)READ_BYTE()])
#define BINARY_OP(op) do { \
        double r = pop(); \
        double l = pop(); \
        push(l op r); \
    } while (false)

    for (;;) {
        byte instruction;
        Value constant;

#ifdef DEBUG_TRACE_EXECUTION
        printStack();
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        switch (instruction = READ_BYTE()) {
            case OP_RETURN:
                printValue(pop());
                return INTERPRET_OK;
            case OP_CONSTANT_8:
                constant = READ_CONSTANT();
                push(constant);
                break;
            case OP_CONSTANT_24:
                constant = READ_CONSTANT_LONG();
                push(constant);
                break;
            case OP_NEGATE:
                push(-pop());
                break;
            case OP_ADD:
                BINARY_OP(+);
                break;
            case OP_SUBTRACT:
                BINARY_OP(-);
                break;
            case OP_MULTIPLY:
                BINARY_OP(*);
                break;
            case OP_DIVIDE:
                BINARY_OP(/);
                break;
            default:
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef BINARY_OP
}

void initVM() {
    resetStack();
}

void freeVM() {
    free(vm.stack);
}

InterpretResult interpret(const char* source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);

    return result;
}

void push(Value v) {
    *vm.stackTop = v;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}