#ifndef LOADER_CORE_H
#define LOADER_CORE_H

/* fsm + mode + control；唯一写执行器 — 见 LOADER_DESIGN.md */
#include "loader_runtime.h"
#include "service/sense/sense.h"
#include "service/load/load_out.h"
#include "common/tools/common_def.h"
#include "common/pid/pid.h"

exit_code_t loader_core_init(void);

/** TIM 控制中断：测 → 护 → 目标 → PID → 写输出 */
void loader_core_control_update(void);

/**
 * 任务侧状态推进（可选）：处理请求类逻辑。
 * 启停请优先用 request_run / request_stop / clear_fault。
 */
void loader_core_state_update(void *arg);

/** 请求进入 RUN（IDLE/PAUSED 且无故障） */
exit_code_t loader_core_request_run(void);

/** 请求停机到 IDLE 并关断输出 */
exit_code_t loader_core_request_stop(void);

/** 清除 FAULT → IDLE（需先停机） */
exit_code_t loader_core_clear_fault(void);

/** 故障入口（可 ISR）：立刻关断并进 ERROR */
void loader_core_fault_trigger(loader_error_t error);

#endif /* LOADER_CORE_H */
