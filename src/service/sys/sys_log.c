/*
 * sys_log.c — system log backend via SEGGER RTT (bsp/rtt)
 */

#include "sys_log.h"
#include "rtt.h"
#include "SEGGER_RTT.h"
#include <stdarg.h>
#include <stdio.h>

void sys_log_init(sys_log_type_e log_type)
{
    (void)log_type;
    sys_log_text(info, "sys_log -> RTT channel 0");
}

void sys_log_send_data(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len == 0) {
        return;
    }
    SEGGER_RTT_Write(0, data, len);
}

void sys_log_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_rtt_vprintf(fmt, args);
    va_end(args);
}
