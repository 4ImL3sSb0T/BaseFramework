/*
 * debug.c — SEGGER RTT debug output
 */

#include "debug.h"
#include "SEGGER_RTT.h"

void debug_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    SEGGER_RTT_vprintf(0, format, &args);
    va_end(args);
}

void debug_vprintf(const char *format, va_list args) {
    SEGGER_RTT_vprintf(0, format, &args);
}

void debug_println(const char *text) {
    SEGGER_RTT_WriteString(0, text);
    SEGGER_RTT_WriteString(0, "\n");
}
