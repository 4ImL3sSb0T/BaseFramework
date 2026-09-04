#ifndef _SYS_LOG_H_
#define _SYS_LOG_H_

#include <stdint.h>
#include "sys_time.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYS_LOG_RTT = 0,
    /* aliases kept for call-site compatibility; both map to RTT */
    SYS_LOG_WIFI = SYS_LOG_RTT,
    SYS_LOG_UART = SYS_LOG_RTT,
} sys_log_type_e;

void sys_log_init(sys_log_type_e log_type);
void sys_log_send_data(const uint8_t *data, uint32_t len);
void sys_log_printf(const char *fmt, ...);

#define sys_log_text(window, fmt, args...) \
    sys_log_printf("{" #window "}" fmt "\n", ##args)
#define sys_log_stamp(window, fmt, ...) \
    sys_log_printf("<%u>{" #window "}" fmt "\n", (unsigned)sys_time_get_ms(), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* _SYS_LOG_H_ */
