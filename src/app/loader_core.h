#ifndef LOADER_CORE_H
#define LOADER_CORE_H

/* fsm + mode + control；唯一写执行器 — 见 LOADER_DESIGN.md */
#include "loader_runtime.h"
#include "service/sense/sense.h"
#include "service/load/load_out.h"
#include "common/tools/common_def.h"
#include "common/pid/pid.h"

/**
 * 初始化测量/执行器/风扇/PID，并启动 TIM 高频控制环回调。
 * 由 loader_task_start() 调用。
 */
exit_code_t loader_core_init(void);

/**
 * 高频控制环（TIM ISR，~2 kHz）
 * 测 → 更新 runtime 测量 → 按状态跑模式/PID → 写 load_out
 */
void loader_core_control_update(void);

/**
 * 中频状态/保护（loader_state 任务，~5 ms）
 * OCP/OTP 等软件保护；不跑控制公式。
 * 启停请用 request_run / request_stop / clear_fault。
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
