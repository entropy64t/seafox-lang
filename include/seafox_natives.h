#pragma once

#include <stdio.h>
#include <time.h>

#include "value.h"
#include "object.h"

#define NATIVE(name) bool name##Native(int argCount, Value* args, Value* out)

bool getTimeNative(int argCount, Value* args, Value* out);

bool writeNative(int argCount, Value* args, Value* out);

bool writelnNative(int argCount, Value* args, Value* out);

bool readlnNative(int argCount, Value* args, Value* out);

bool arrayNative(int argCount, Value* args, Value* out);

NATIVE(number);

NATIVE(length);

NATIVE(type);

NATIVE(makeType);

NATIVE(string);

NATIVE(iterator);

NATIVE(next);

NATIVE(isEnd);