#include "seafox_natives.h"

#include "object.h"
#include "vm.h"
#include <stdlib.h>
#include <time.h>

#define TYPE_ERROR(fn, expected, at) runtimeError(fn "(): argument " #at ": expected " expected ", got %T", &args[at - 1]);

bool getTimeNative(int argCount, Value* args, Value* out) {
	*out = NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
	return true;
}

bool writeNative(int argCount, Value* args, Value* out) {
	for (int i = 0; i < argCount - 1; i++) {
		printValue(args[i], " ");
	}
	printValue(args[argCount - 1], "");
	*out = NULL_VAL;
	return true;
}

bool writelnNative(int argCount, Value* args, Value* out) {
	bool result = writeNative(argCount, args, out);
	printf("\n");
	return result;
}

bool readlnNative(int argCount, Value* args, Value* out) {
	char buffer[1024];
	if (!fgets(buffer, sizeof(buffer), stdin)) {
		*out = NULL_VAL;
		return true;
	}
	if (strchr(buffer, '\n') == NULL) {
		// line doesnt fit, throw away any excess chars
		int c;
		while ((c = getchar()) != '\n' && c != EOF)
			;
	}

	buffer[strcspn(buffer, "\n")] = '\0'; // replace endline with null terminator
	*out = OBJ_VAL(copyString(buffer, strlen(buffer)));
	return true;
}

bool arrayNative(int argCount, Value* args, Value* out) {
	if (IS_NUMBER(args[0])) {
		int size = AS_NUMBER(args[0]);
		*out = OBJ_VAL(newArray(size));
		return true;
	}
	else if (IS_STRING(args[0])) {
		ObjString* str = AS_STRING(args[0]);
		ObjArray* arr = newArray(str->length);
		push(OBJ_VAL(arr)); // make the GC know about the array
		for (int i = 0; i < str->length; i++) {
			char chars[2];
			chars[0] = str->chars[i];
			chars[1] = '\0';
			arr->items[i] = OBJ_VAL(copyString(chars, 2));
			// the GC is insta aware of these strings as they are reachable from the arr
		}
		pop(); // pop the array
		*out = OBJ_VAL(arr);
		return true;
	}

	TYPE_ERROR("array", "Number or String", 1);
	*out = NULL_VAL;
	return false;
}

bool numberNative(int argCount, Value* args, Value* out) {
	if (IS_NUMBER(args[0])) {
		*out = args[0];
		return true;
	}
	else if (IS_STRING(args[0])) {
		char* end = NULL;
		const char* cstr = AS_CSTRING(args[0]);
		double num = strtod(cstr, &end);
		if (end == cstr || *end != '\0') {
			*out = NULL_VAL;
			runtimeError("number(): argument 1: could not convert string '%s' to number.", cstr);
			return false;
		}

		*out = NUMBER_VAL(num);
		return true;
	}
	else if (IS_NULL(args[0])) {
		*out = NUMBER_VAL(0);
		return true;
	}
	else if (IS_BOOL(args[0])) {
		*out = NUMBER_VAL(AS_BOOL(args[0]));
		return true;
	}
	*out = NULL_VAL;
	TYPE_ERROR("number", "Number, String, Null, or Boolean", 1);
	return false;
}

bool lengthNative(int argCount, Value* args, Value* out) {
	if (IS_STRING(args[0])) {
		*out = NUMBER_VAL(AS_STRING(args[0])->length);
		return true;
	}
	else if (IS_ARRAY(args[0])) {
		*out = NUMBER_VAL(AS_ARRAY(args[0])->length);
		return true;
	}
	*out = NULL_VAL;
	TYPE_ERROR("length", "String or Array", 1);
	return false;
}

bool typeNative(int argCount, Value* args, Value* out) {
	*out = seafoxType(args[0]);
	return true;
}

bool makeTypeNative(int argCount, Value* args, Value* out) {
	if (!IS_STRING(args[1])) {
		*out = NULL_VAL;
		TYPE_ERROR("Type", "String", 2);
		return false;
	}
	*out = seafoxType(args[0]);
	AS_SEAFOX_TYPE(*out)->name = AS_STRING(args[1]);
	return true;
}

bool stringNative(int argCount, Value* args, Value* out) {
	char* chars;
	*out = OBJ_VAL(copyString(chars, strlen(chars)));
	return true;
}

bool iteratorNative(int argCount, Value* args, Value* out) {
	if (IS_STRING(args[0])) {
		ObjString* str = AS_STRING(args[0]);
		ObjArray* arr = newArray(str->length);
		push(OBJ_VAL(arr)); // make the GC know about the array
		for (int i = 0; i < str->length; i++) {
			char chars[2];
			chars[0] = str->chars[i];
			chars[1] = '\0';
			arr->items[i] = OBJ_VAL(copyString(chars, 2));
			// the GC is insta aware of these strings as they are reachable from the arr
		}
		*out = OBJ_VAL(makeIterator(arr));
		pop(); // pop the array
		return true;
	}
	if (!IS_ARRAY(args[0])) {
		*out = NULL_VAL;
		TYPE_ERROR("Iterator", "Array", 1);
		return false;
	}
	*out = OBJ_VAL(makeIterator(AS_ARRAY(args[0])));
	return true;
}

bool nextNative(int argCount, Value* args, Value* out) {
	if (!IS_ITERATOR(args[0])) {
		*out = NULL_VAL;
		TYPE_ERROR("next", "Iterator", 1);
		return false;
	}

	ObjIterator* current = AS_ITERATOR(args[0]);
	int currentIndex = current->pointer - current->container->items;
	if (currentIndex >= current->container->length) {
		*out = NULL_VAL;
		runtimeError("next(): Cannot increment a past-end iterator");
		return false;
	}
	current->pointer++;
	*out = OBJ_VAL(current);
	return true;
}

bool isEndNative(int argCount, Value* args, Value* out) {
	if (!IS_ITERATOR(args[0])) {
		*out = NULL_VAL;
		TYPE_ERROR("isEnd", "Iterator", 1);
		return false;
	}
	ObjIterator* it = AS_ITERATOR(args[0]);
	*out = BOOL_VAL(it->pointer - it->container->items == it->container->length);
	return true;
}