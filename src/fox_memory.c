#include <stdlib.h>

#include "fox_memory.h"
#include "object.h"
#include "vm.h"
#include "value.h"
#include "compiler.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

static void freeObject(Object* object) {
#ifdef DEBUG_LOG_GC
	printf("%p free type %d\n", (void*) object, object->type);
#endif

	switch (object->type) {
		case OBJ_STRING:
			{
				ObjString* string = (ObjString*) object;
				FREE_ARRAY(char, string->chars, string->length + 1);
				FREE(ObjString, object);
				break;
			}
		case OBJ_ARRAY:
			{
				ObjArray* array = (ObjArray*) object;
				FREE_ARRAY(Value, array->items, array->length);
				FREE(ObjArray, object);
				break;
			}
		case OBJ_FUNCTION:
			{
				ObjFunction* function = (ObjFunction*) object;
				freeChunk(&function->chunk);
				FREE(ObjFunction, function);
				break;
			}
		case OBJ_NATIVE_FN:
			{
				FREE(ObjNativeFn, object);
				break;
			}
		case OBJ_UPVALUE:
			{
				FREE(ObjUpvalue, object);
				break;
			}
		case OBJ_CLOSURE:
			{
				ObjClosure* closure = (ObjClosure*) object;
				FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
				FREE(ObjClosure, object);
				break;
			}
		case OBJ_SEAFOX_TYPE:
			{
				FREE(ObjSeafoxType, object);
				break;
			}
		case OBJ_CLASS:
			{
				FREE(ObjClass, object);
				break;
			}
		case OBJ_INSTANCE:
			{
				ObjInstance* instance = (ObjInstance*) object;
				freeTable(&instance->fields);
				FREE(ObjInstance, object);
				break;
			}
	}
}

static void markRoots() {
	for (Value* slot = vm.stack; slot != vm.stackTop; slot++) {
		markValue(*slot);
	}

	for (int i = 0; i < vm.frameCount; i++) {
		markObject((Object*) vm.frames[i].closure);
	}

	for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
		markObject((Object*) upvalue);
	}

	markTable(&vm.globals);
	markCompilerRoots();
}

static void markArray(ValueArray* array) {
	for (int i = 0; i < array->count; i++) {
		markValue(array->values[i]);
	}
}

static void blackenObject(Object* object) {
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void*) object);
	printValue(OBJ_VAL(object), "\n");
#endif
	switch (object->type) {
		case OBJ_CLOSURE:
			{
				ObjClosure* closure = (ObjClosure*) object;
				markObject((Object*) closure->function);
				for (int i = 0; i < closure->upvalueCount; i++) {
					markObject((Object*) closure->upvalues[i]);
				}
				break;
			}
		case OBJ_UPVALUE:
			markValue(((ObjUpvalue*) object)->closed);
			break;
		case OBJ_FUNCTION:
			{
				ObjFunction* function = (ObjFunction*) object;
				markObject((Object*) function->name);
				markArray(&function->chunk.constants);
				break;
			}
		case OBJ_ARRAY:
			{
				ObjArray* array = (ObjArray*) object;
				for (int i = 0; i < array->length; i++)
					markValue(array->items[i]);
				break;
			}
		case OBJ_SEAFOX_TYPE:
			{
				ObjSeafoxType* type = (ObjSeafoxType*) object;
				markObject((Object*) type->function);
				markObject((Object*) type->name);
				break;
			}
		case OBJ_ITERATOR:
			markObject((Object*) ((ObjIterator*) object)->container);
			break;
		case OBJ_CLASS:
			{
				ObjClass* clas = (ObjClass*) object;
				markObject((Object*) clas->name);
				break;
			}
		case OBJ_INSTANCE:
			{
				ObjInstance* instance = (ObjInstance*) object;
				markObject((Object*) instance->clas);
				markTable(&instance->fields);
				break;
			}
		case OBJ_NATIVE_FN:
		case OBJ_STRING:
			break;
	}
}

static void traceReferences() {
	while (vm.grayCount > 0) {
		Object* object = vm.grayStack[--vm.grayCount];

		blackenObject(object);
	}
}

static void sweep() {
	Object* previous = NULL;
	Object* object = vm.objects;
	while (object != NULL) {
		if (object->marked) {
			object->marked = false;
			previous = object;
			object = object->next;
		}
		else {
			Object* unreached = object;
			object = object->next;
			if (previous != NULL) {
				previous->next = object;
			}
			else {
				vm.objects = object;
			}

			freeObject(unreached);
		}
	}
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
	vm.bytesAllocated += newSize - oldSize;
	if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
		collectGarbage();

		if (vm.bytesAllocated > vm.nextGC) {
			collectGarbage();
		}
#endif
	}

	if (newSize == 0) {
		free(pointer);
		return NULL;
	}

	void* result = realloc(pointer, newSize);
	if (result == NULL)
		exit(1);
	return result;
}

void freeObjects() {
	Object* object = vm.objects;

	while (object != NULL) {
		Object* next = object->next;
		freeObject(object);
		object = next;
	}
}

// TODO: optimize - don't add strings and natives as they don't reference anything
void markObject(Object* object) {
	if (object == NULL)
		return;
	if (object->marked)
		return;

#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void*) object);
	printValue(OBJ_VAL(object), "\n");
#endif
	object->marked = true;

	if (vm.grayCapacity < vm.grayCount + 1) {
		vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
		vm.grayStack = (Object**) realloc(vm.grayStack, sizeof(Object*) * vm.grayCapacity);
	}

	// no space for the gray stack
	if (vm.grayStack == NULL)
		exit(1);

	vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
	if (!IS_OBJECT(value))
		return;

	markObject(AS_OBJECT(value));
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
	printf("-- GC begin --\n");
	size_t before = vm.bytesAllocated;
#endif

	markRoots();

	traceReferences();

	tableRemoveWhite(&vm.strings);

	sweep();

	vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
	printf("-- GC end --\n");
	printf("   Collected %zu bytes (from %zu to %zu) next at %zu\n",
		   before - vm.bytesAllocated,
		   before, vm.bytesAllocated,
		   vm.nextGC);
#endif
}