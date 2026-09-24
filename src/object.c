#include <stdio.h>
#include <string.h>

#include "fox_memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "hashtable.h"

const char* types[VALUE_TYPE_COUNT] = {
	[VAL_BOOL] = "Bool",
	[VAL_NUMBER] = "Number",
	[VAL_NULL] = "Null",
	[VAL_OBJECT] = "Object"};

const char* objtypes[OBJ_TYPE_COUNT] = {
	[OBJ_STRING] = "String",
	[OBJ_ARRAY] = "Array",
	[OBJ_FUNCTION] = "Function",
	[OBJ_NATIVE_FN] = "Native Function",
	[OBJ_CLOSURE] = "Closure",
	[OBJ_UPVALUE] = "Upvalue",
	[OBJ_SEAFOX_TYPE] = "Type",
	[OBJ_ITERATOR] = "Iterator",
};

static Object* allocateObject(size_t size, ObjectType type) {
	Object* object = (Object*) reallocate(NULL, 0, size);
	object->type = type;
	object->marked = false;

	object->next = vm.objects;
	vm.objects = object;

#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void*) object, size, type);
#endif

	return object;
}

#define ALLOCATE_OBJ(type, objectType) (type*) allocateObject(sizeof(type), objectType)

static uint32_t hashString(const char* key, int length) {
	uint32_t hash = 2166136261u; // 1st magic prime
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t) key[i];
		hash *= 16777619; // second magic prime
	}
	return hash;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
	ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;

	// The intern table is weak, and inserting into it can trigger a GC.
	push(OBJ_VAL(string));
	tableSet(&vm.strings, string, NULL_VAL); // treat the hash table like a hash set
	pop();

	return string;
}

static ObjArray* allocateArray(Value* items, int length) {
	ObjArray* array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY);
	array->length = length;
	array->items = items;
	return array;
}

ObjString* copyString(char* chars, int length) {
	uint32_t hash = hashString(chars, length);

	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);

	if (interned != NULL)
		return interned;

	char* heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';
	return allocateString(heapChars, length, hash);
}

ObjString* takeString(char* chars, int length) {
	uint32_t hash = hashString(chars, length);

	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}

	return allocateString(chars, length, hash);
}

ObjArray* copyArray(Value* items, int length) {
	Value* heapItems = ALLOCATE(Value, length);
	memcpy(heapItems, items, sizeof(Value) * length);

	return allocateArray(heapItems, length);
}

ObjArray* takeArray(Value* items, int length) {
	return allocateArray(items, length);
}

ObjArray* newArray(int length) {
	Value* nulls = ALLOCATE(Value, length);
	for (int i = 0; i < length; i++)
		nulls[i] = NULL_VAL;
	return allocateArray(nulls, length);
}

ObjFunction* newFunction() {
	ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
	function->arity = 0;
	function->defaultsCount = 0;
	function->upvalueCount = 0;
	function->name = NULL;
	initChunk(&function->chunk);
	return function;
}

ObjUpvalue* newUpvalue(Value* slot) {
	ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
	upvalue->closed = NULL_VAL;
	upvalue->location = slot;
	upvalue->next = NULL;
	return upvalue;
}

ObjClosure* newClosure(ObjFunction* function) {
	ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
	for (int i = 0; i < function->upvalueCount; i++) {
		upvalues[i] = NULL;
	}

	ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;

	return closure;
}

ObjSeafoxType* newType(char* name, ValueType value, ObjectType object) {
	ObjSeafoxType* type = ALLOCATE_OBJ(ObjSeafoxType, OBJ_SEAFOX_TYPE);
	type->value = value;
	type->object = object;
	type->name = copyString(name, (int) strlen(name));
	return type;
}

Value seafoxType(Value value) {
	ValueType valueType = value.type;
	ObjectType objType = OBJ_TYPE_COUNT;
	if (IS_OBJECT(value)) {
		objType = OBJ_TYPE(value);
	}
	if (objType == OBJ_CLOSURE || objType == OBJ_NATIVE_FN)
		objType = OBJ_FUNCTION;
	if (objType == OBJ_UPVALUE) {
		return seafoxType(*AS_UPVALUE(value)->location);
	}
	char* name = types[valueType];
	if (objType != OBJ_TYPE_COUNT)
		name = objtypes[objType];

	return OBJ_VAL(newType(name, valueType, objType));
	// TODO this is most of the time unnecesarily slow
	// because for 'is' the type name does not matter
}

bool typesEqual(Value a, Value b) {
	ObjSeafoxType* t1 = AS_SEAFOX_TYPE(a);
	if (!IS_SEAFOX_TYPE(b) && IS_NULL(b)) {
		return t1->value != VAL_NULL;
	}
	ObjSeafoxType* t2 = AS_SEAFOX_TYPE(b);
	return t1->value == t2->value && t1->object == t2->object;
	// TODO should types check their names?
}

static void fprintArray(FILE* file, ObjArray* array) {
	fprintf(file, "[");

	for (int i = 0; i < array->length - 1; i++) {
		fprintValue(file, array->items[i], ", ");
	}
	fprintValue(file, array->items[array->length - 1], "");
	fprintf(file, "]");
}

static void fprintFunction(FILE* file, ObjFunction* function) {
	if (function->name == NULL) {
		fprintf(file, "<script>");
		return;
	}
	fprintf(file, "<function %s>", function->name->chars);
}

void fprintObject(FILE* file, Value value) {
	switch (OBJ_TYPE(value)) {
		case OBJ_STRING:
			fprintf(file, "%s", AS_CSTRING(value));
			break;
		case OBJ_ARRAY:
			fprintArray(file, AS_ARRAY(value));
			break;
		case OBJ_FUNCTION:
			fprintFunction(file, AS_FUNCTION(value));
			break;
		case OBJ_UPVALUE:
			fprintf(file, "upvalue");
			break;
		case OBJ_CLOSURE:
			fprintFunction(file, AS_CLOSURE(value)->function);
			break;
		case OBJ_NATIVE_FN:
			fprintf(file, "<native function %s>", AS_NATIVE(value)->name);
			break;
		case OBJ_SEAFOX_TYPE:
			ObjString* name = AS_SEAFOX_TYPE(value)->name;
			fprintf(file, "<type %.*s>", name->length, name->chars);
			break;
		case OBJ_ITERATOR:
			fprintf(file, "<iterator>");
			break;
	}
}

void printObject(Value value) {
	fprintObject(stdout, value);
}

ObjNativeFn* newNativeFn(NativeFn function, const char* name, int arity) {
	ObjNativeFn* native = ALLOCATE_OBJ(ObjNativeFn, OBJ_NATIVE_FN);
	native->function = function;
	native->name = name;
	native->arity = arity;
	return native;
}

ObjIterator* makeIterator(ObjArray* container) {
	ObjIterator* iter = ALLOCATE_OBJ(ObjIterator, OBJ_ITERATOR);
	iter->pointer = container->items;
	iter->container = container;
	return iter;
}