/*
 * rtt.c — SEGGER RTT output
 */

#include "rtt.h"
#include "SEGGER_RTT.h"

void log_rtt_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    SEGGER_RTT_vprintf(0, format, &args);
    va_end(args);
}

void log_rtt_vprintf(const char *format, va_list args) {
    SEGGER_RTT_vprintf(0, format, &args);
}

void log_rtt_println(const char *text) {
    SEGGER_RTT_WriteString(0, text);
    SEGGER_RTT_WriteString(0, "\n");
}
