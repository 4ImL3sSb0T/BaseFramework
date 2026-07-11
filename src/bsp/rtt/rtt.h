/*
 * rtt.h — SEGGER RTT output BSP
 *
 * SEGGER RTT channel 0 printf.
 */

#ifndef _BSP_RTT_H_
#define _BSP_RTT_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void log_rtt_printf(const char *format, ...);
void log_rtt_vprintf(const char *format, va_list args);
void log_rtt_println(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_RTT_H_ */
