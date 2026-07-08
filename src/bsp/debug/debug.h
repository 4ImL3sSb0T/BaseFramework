/*
 * debug.h — Debug output BSP
 *
 * SEGGER RTT channel 0 printf.
 */

#ifndef _BSP_DEBUG_H_
#define _BSP_DEBUG_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void debug_printf(const char *format, ...);
void debug_vprintf(const char *format, va_list args);
void debug_println(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_DEBUG_H_ */
