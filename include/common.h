#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Useful for debugging
#pragma region Debugging

// Trace the execution of every bytecode instruction
// #define DEBUG_TRACE_EXECUTION

// Print the bytecode at the end of compilation
// #define DEBUG_PRINT_CODE

// Run the GC on every allocation
#define DEBUG_STRESS_GC

// Log all of GC's actions
// #define DEBUG_LOG_GC

// Warn when print statement is used
// #define WARN_ON_PRINT

// Do not use print statement
// #define DONT_USE_PRINT

#pragma endregion

#define LOCALS_COUNT (UINT8_MAX + 1) // 1 byte should be enough (256)

typedef uint8_t byte;
#define BYTE_MAX UINT8_MAX