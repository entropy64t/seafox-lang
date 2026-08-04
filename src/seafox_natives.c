#include "seafox_natives.h"

#include "object.h"
#include "vm.h"
#include <stdlib.h>

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
	if (!IS_NUMBER(args[0])) {
		runtimeError("array(): parameter 1: expected number, got %T.", &args[0]);
		*out = NULL_VAL;
		return false;
	}
	int size = AS_NUMBER(args[0]);
	*out = OBJ_VAL(newArray(size));
	return true;
}

bool numberNative(int argCount, Value* args, Value* out) {
	if (IS_STRING(args[0])) {
		char* end = NULL;
		const char* cstr = AS_CSTRING(args[0]);
		double num = strtod(cstr, &end);
		if (end == cstr || *end != '\0') {
			*out = NULL_VAL;
			runtimeError("number(): parameter 1: could not convert string '%s' to number.", cstr);
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
	runtimeError("number(): parameter 1: expected string, null, or boolean, got %T.", &args[0]);
	return false;
}

bool stringNative(int argCount, Value* args, Value* out) {
	char* chars;
	*out = OBJ_VAL(copyString(chars, strlen(chars)));
	return true;
}