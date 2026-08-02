#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DEBUG_TRACE_EXECUTION
#define DEBUG_PRINT_CODE

#define LOCALS_COUNT (UINT8_MAX + 1) // 1 byte should be enough (256)

typedef uint8_t byte;
#define BYTE_MAX UINT8_MAX