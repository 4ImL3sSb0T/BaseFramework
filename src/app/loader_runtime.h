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
    float power_setpoint;
    float power_measurement;
    float resistance_setpoint;
    float resistance_measurement;
    float temperature_measurement;
    float temperature_setpoint;
} loader_runtime_t;

exit_code_t loader_runtime_init(void);

/** 整份快照（短临界区，任务/ISR 均可） */
loader_runtime_t loader_runtime_get(void);

/**
 * 整份写回。仅用于 UI/CLI 改设定时谨慎使用；
 * 控制环请用 update_measurements / set_state，避免覆盖设定。
 */
exit_code_t loader_runtime_set(const loader_runtime_t *runtime);

/** 只更新测量字段（控制环写） */
void loader_runtime_update_measurements(float current, float voltage,
                                        float power, float resistance,
                                        float temperature);

/** 状态 / 故障（控制环或故障 ISR） */
void loader_runtime_set_state(loader_state_t state);
void loader_runtime_enter_fault(loader_error_t error);
void loader_runtime_clear_fault(void);

#endif /* LOADER_RUNTIME_H */
