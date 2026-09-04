#include "tim.h"
#include "lib/tools/common_def.h"

typedef void (*timer_callback_t)(void);

exit_code_t bsp_timer_init(void);
exit_code_t bsp_timer_register_callback(timer_callback_t callback);

exit_code_t bsp_timer_handler(void);