#include <stdarg.h>
#include <unistd.h>
#include <stdbool.h>

#include "runtime_error.h"
#include "value.h"
#include "object.h"

/* ============================
   Low-level output helpers
   ============================ */

static void put_char(char c) {
    write(2, &c, 1);
}

static void put_str(const char *s) {
    if (!s) {
        put_str("(null)");
        return;
    }
    while (*s) {
        put_char(*s++);
    }
}

static void put_uint(unsigned long n, int base, bool uppercase) {
    char buffer[32];
    const char *digits = uppercase ?
        "0123456789ABCDEF" :
        "0123456789abcdef";

    int i = 0;

    if (n == 0) {
        put_char('0');
        return;
    }

    while (n > 0) {
        buffer[i++] = digits[n % base];
        n /= base;
    }

    while (i--) {
        put_char(buffer[i]);
    }
}

static void put_int(long n) {
    if (n < 0) {
        put_char('-');
        put_uint((unsigned long)(-n), 10, false);
    } else {
        put_uint((unsigned long)n, 10, false);
    }
}


/* Predefined messages for enum values */

static const char* type_messages[VALUE_TYPE_COUNT] = {
    [VAL_BOOL] = "bool",
    [VAL_NUMBER] = "number",
    [VAL_NULL] = "null",
    [VAL_OBJECT] = "object"
};

static const char *objtypes[OBJ_TYPE_COUNT] = {
    [OBJ_STRING] = "string",
    [OBJ_ARRAY] = "array",
};

static const char* simpleValue(int v) {
    if (v >= 0 && v < VALUE_TYPE_COUNT && type_messages[v]) {
        return type_messages[v];
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
            put_str(simpleValue(v->type));
            break;
        case VAL_OBJECT:
            put_str(objectValue(v->as.object->type));
            break;
    }
}

/* ============================
   runtime error
   ============================ */

int rte(const char *fmt, va_list args) {
    int written = 0;

    for (; *fmt; fmt++) {

        if (*fmt != '%') {
            put_char(*fmt);
            written++;
            continue;
        }

        fmt++;

        switch (*fmt) {

        case 'd': {
            int v = va_arg(args, int);
            put_int(v);
            break;
        }

        case 'u': {
            unsigned int v = va_arg(args, unsigned int);
            put_uint(v, 10, false);
            break;
        }

        case 'x': {
            unsigned int v = va_arg(args, unsigned int);
            put_uint(v, 16, false);
            break;
        }

        case 'c': {
            char c = (char)va_arg(args, int);
            put_char(c);
            break;
        }

        case 's': {
            char *s = va_arg(args, char *);
            put_str(s);
            break;
        }

        case 't': {
            /* Custom enum printer */
            int v = va_arg(args, int);  /* enum promoted to int */
            put_str(simpleValue(v));
            break;
        }

        case 'T': {
            /* Custom enum printer */
            Value* v = va_arg(args, Value*);
            printType(v);
            break;
        }

        case '%':
            put_char('%');
            break;

        default:
            /* Unknown specifier */
            put_char('%');
            put_char(*fmt);
            break;
        }
    }

    // va_end(args);
    return written;
}