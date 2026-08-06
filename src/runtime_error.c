#include <stdarg.h>
#include <unistd.h>
#include <stdbool.h>

#include "runtime_error.h"
#include "value.h"
#include "object.h"

/* ============================
   Low-level output helpers
   ============================ */

static void putChar(char c) {
	write(2, &c, 1);
}

static void putStr(const char* s) {
	if (!s) {
		putStr("(null)");
		return;
	}
	while (*s) {
		putChar(*s++);
	}
}

static void putUint(unsigned long n, int base, bool uppercase) {
	char buffer[32];
	const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

	int i = 0;

	if (n == 0) {
		putChar('0');
		return;
	}

	while (n > 0) {
		buffer[i++] = digits[n % base];
		n /= base;
	}

	while (i--) {
		putChar(buffer[i]);
	}
}

static void putInt(long n) {
	if (n < 0) {
		putChar('-');
		putUint((unsigned long) (-n), 10, false);
	}
	else {
		putUint((unsigned long) n, 10, false);
	}
}

/* Predefined messages for enum values */

static const char* simpleValue(int v) {
	if (v >= 0 && v < VALUE_TYPE_COUNT && types[v]) {
		return types[v];
	}
	return "<invalid enum>";
}

static const char* objectValue(int v) {
	if (v >= 0 && v < OBJ_TYPE_COUNT && objtypes[v]) {
		return objtypes[v];
	}
	return "<invalid enum>";
}

static void printType(Value* v) {
	switch (v->type) {
		case VAL_BOOL:
		case VAL_NULL:
		case VAL_NUMBER:
			putStr(simpleValue(v->type));
			break;
		case VAL_OBJECT:
			putStr(objectValue(v->as.object->type));
			break;
	}
}

/* ============================
   runtime error
   ============================ */

int rte(const char* fmt, va_list args) {
	int written = 0;

	for (; *fmt; fmt++) {
		if (*fmt != '%') {
			putChar(*fmt);
			written++;
			continue;
		}

		fmt++;

		switch (*fmt) {
			case 'd':
				{
					int v = va_arg(args, int);
					putInt(v);
					break;
				}

			case 'u':
				{
					unsigned int v = va_arg(args, unsigned int);
					putUint(v, 10, false);
					break;
				}

			case 'x':
				{
					unsigned int v = va_arg(args, unsigned int);
					putUint(v, 16, false);
					break;
				}

			case 'c':
				{
					char c = (char) va_arg(args, int);
					putChar(c);
					break;
				}

			case 's':
				{
					char* s = va_arg(args, char*);
					putStr(s);
					break;
				}

			case 't':
				{
					/* Custom enum printer */
					int v = va_arg(args, int); /* enum promoted to int */
					putStr(simpleValue(v));
					break;
				}

			case 'T':
				{
					/* Custom enum printer */
					Value* v = va_arg(args, Value*);
					printType(v);
					break;
				}

			case 'V':
				{
					/* Custom enum printer */
					Value* v = va_arg(args, Value*);
					printValue(*v, "\n");
					break;
				}

			case '%':
				putChar('%');
				break;

			default:
				/* Unknown specifier */
				putChar('%');
				putChar(*fmt);
				break;
		}
	}

	// va_end(args);
	return written;
}