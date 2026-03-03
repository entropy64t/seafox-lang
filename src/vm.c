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

VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
}

static void printStack() {
    printf("Stack: ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        printf("[ ");
        printValue(*slot, "");
        printf(" ]");
    }
    printf("\n");
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    rte(format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = getLine(vm.chunk, instruction);
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
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

#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (vm.chunk->constants.values[((int)READ_BYTE() << 16) + ((int)READ_BYTE() << 8) + (int)READ_BYTE()])
#define BINARY_OP(valueType, op)                        \
    do                                                  \
    {                                                   \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
        {                                               \
            Value v1 = peek(1);                         \
            Value v2 = peek(0); \
            runtimeError("Both operands must be numbers, not %T and %T.", &v1, &v2); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (false)
#define READ_STRING() AS_STRING(READ_CONSTANT())

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
    else if (IS_STRING(av))
    {
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
        runtimeError("Index must be in range [0, %i), is %i", string->length, index);
        return false;
    }
    char* single = &string->chars[index];
    push(OBJ_VAL(copyString(single, 1)));
    return true;
}

static bool indexArray(ObjArray* arr, int index) {
    if (index < 0 || index >= arr->length) {
        runtimeError("Index must be in range [0, %i), is %i", arr->length, index);
        return false;
    }
    Value single = arr->items[index];
    push(single);
    return true;
}

static bool indexAccess() {
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
    
    switch (AS_OBJECT(array)->type)
    {
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

static InterpretResult run() {
    for (;;) {
        byte instruction;
        Value constant;

#ifdef DEBUG_TRACE_EXECUTION
        printStack();
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        switch (instruction = READ_BYTE()) {
            case OP_RETURN:
                //printValue(pop(), "\n");
                return INTERPRET_OK;
            case OP_CONSTANT_8:
                constant = READ_CONSTANT();
                push(constant); break;
            case OP_CONSTANT_24:
                constant = READ_CONSTANT_LONG();
                push(constant); break;
            case OP_TRUE:
                push(BOOL_VAL(true)); break;
            case OP_FALSE:
                push(BOOL_VAL(false)); break;
            case OP_NULL:
                push(NULL_VAL); break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    Value p = peek(0);
                    runtimeError("Value to negate must be a number, not %T.", &p);
                    return INTERPRET_RUNTIME_ERROR; 
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop()))); break;
            case OP_ADD:
                if (!add())
                    return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /); break;
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b))); }
                break;
            case OP_GREATER:  
                BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     
                BINARY_OP(BOOL_VAL, <); break;
            case OP_ARRAY:
                if (!array())
                    return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_INDEX_ACCESS:
                if (!indexAccess())
                    return INTERPRET_RUNTIME_ERROR;
                break;
            case OP_PRINT:
                printValue(pop(), "\n");
                break;
            case OP_POP:
                pop();
                break;
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value var;
                if (!tableGet(&vm.globals, name, &var)) {
                    runtimeError("Undefined variable: '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(var);
                break;
            }
            default:
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef BINARY_OP
#undef READ_STRING
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globals);
}

void freeVM() {
    freeTable(&vm.strings);
    freeTable(&vm.globals);
    freeObjects();
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