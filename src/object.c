#include <stdio.h>
#include <string.h>

#include "fox_memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "hashtable.h"

static Object* allocateObject(size_t size, ObjectType type) {
	Object* object = (Object*) reallocate(NULL, 0, size);
	object->type = type;

	object->next = vm.objects;
	vm.objects = object;

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

	tableSet(&vm.strings, string, NULL_VAL); // treat the hash table like a hash set

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
	function->name = NULL;
	initChunk(&function->chunk);
	return function;
}

static void fprintArray(FILE* file, ObjArray* array) {
	fprintf(file, "{ ");

	for (int i = 0; i < array->length - 1; i++) {
		fprintValue(file, array->items[i], ", ");
	}
	fprintValue(file, array->items[array->length - 1], " ");
	fprintf(file, "}");
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
		case OBJ_NATIVE_FN:
			fprintf(file, "<native function %s>", AS_NATIVE(value)->name);
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