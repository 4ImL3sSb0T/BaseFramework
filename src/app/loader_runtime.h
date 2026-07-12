#ifndef LOADER_RUNTIME_H
#define LOADER_RUNTIME_H

#include "stdint.h"
#include "stdbool.h"
#include "common/tools/common_def.h"

typedef enum {
    LOADER_STATE_IDLE,
    LOADER_STATE_RUNNING,
    LOADER_STATE_PAUSED,
    LOADER_STATE_ERROR
} loader_state_t;

typedef enum {
    LOADER_ERROR_NONE,
    LOADER_ERROR_OVERCURRENT,
    LOADER_ERROR_OVERTEMPERATURE,
    LOADER_ERROR_UNDERVOLTAGE
} loader_error_t;

typedef enum {
    LOADER_MODE_CC,
    LOADER_MODE_CV,
    LOADER_MODE_CP,
    LOADER_MODE_CR
} loader_mode_t;

/* 共享运行时状态（唯一真相源）— 见 LOADER_DESIGN.md */
typedef struct {
    loader_state_t state;
    loader_error_t error;
    loader_mode_t mode;
    float current_setpoint;
    float current_measurement;
    float voltage_setpoint;
    float voltage_measurement;
    float temperature_measurement;
    float temperature_setpoint;
} loader_runtime_t;

exit_code_t loader_runtime_init(void);
loader_runtime_t loader_runtime_get();
exit_code_t loader_runtime_set(loader_runtime_t *runtime);


#endif /* LOADER_RUNTIME_H */
