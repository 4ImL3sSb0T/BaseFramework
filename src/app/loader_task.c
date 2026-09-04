#include "loader_task.h"

#include "cmsis_os2.h"
#include "loader_core.h"
#include "multi_button.h"
#include "bsp/sys/sys_log.h"

/* -------------------------------------------------------------------------- */
/* 任务参数                                                                    */
/* -------------------------------------------------------------------------- */

#ifndef LOADER_STATE_TASK_STACK
#define LOADER_STATE_TASK_STACK  256u
#endif

#ifndef LOADER_STATE_TASK_PRIO
#define LOADER_STATE_TASK_PRIO   osPriorityAboveNormal
#endif

#ifndef LOADER_UI_TASK_STACK
#define LOADER_UI_TASK_STACK     1024u
#endif

#ifndef LOADER_UI_TASK_PRIO
#define LOADER_UI_TASK_PRIO      osPriorityBelowNormal
#endif

/* -------------------------------------------------------------------------- */
/* 中频：状态 / 保护 + 按键扫描                                                  */
/* -------------------------------------------------------------------------- */

/**
 * 中频任务（默认 5 ms）
 * - loader_core_state_update：OCP/OTP 等软件保护与状态侧逻辑
 * - button_ticks：multi_button 状态机（与 TICKS_INTERVAL 一致）
 *
 * 不写 DAC 业务公式；执行器仍由 TIM 内 control_update 独占。
 */
static void loader_state_task(void *argument)
{
    (void)argument;

    sys_log_text(info, "loader_state task running, period=%ums",
                 (unsigned)LOADER_STATE_PERIOD_MS);

    for (;;) {
        loader_core_state_update(NULL);
        button_ticks();
        osDelay(LOADER_STATE_PERIOD_MS);
    }
}

/* -------------------------------------------------------------------------- */
/* 低频：UI                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * 低频任务（默认 ~67 ms / 15 fps）
 * 只做人机：读 runtime、消费按键队列、绘制；不跑控制环。
 */
static void loader_ui_task(void *argument)
{
    (void)argument;

    loader_ui_init();
    sys_log_text(info, "loader_ui task running, poll=%ums",
                 (unsigned)LOADER_UI_POLL_MS);

    for (;;) {
        loader_ui_poll();
        osDelay(LOADER_UI_POLL_MS);
    }
}

/* -------------------------------------------------------------------------- */
/* 产品入口                                                                    */
/* -------------------------------------------------------------------------- */

void loader_task_start(void)
{
    /*
     * 高频：core_init 内
     *   sense / load_out / fan 初始化
     *   bsp_timer_init + 注册 loader_core_control_update
     *   → TIM16 IRQ 周期调用控制环（测→模式→写输出）
     */
    exit_code_t cret = loader_core_init();
    if (cret != EXIT_OK) {
        sys_log_text(error, "loader_core_init fail %d — HF control not started",
                     (int)cret);
        /* 仍尝试起 UI，便于排障显示 */
    } else {
        sys_log_text(info, "loader_core_init OK — HF TIM control armed");
    }

    /* 中频状态任务 */
    {
        static const osThreadAttr_t state_attr = {
            .name       = "loaderState",
            .stack_size = LOADER_STATE_TASK_STACK * 4u,
            .priority   = LOADER_STATE_TASK_PRIO,
        };

        osThreadId_t tid = osThreadNew(loader_state_task, NULL, &state_attr);
        if (tid == NULL) {
            sys_log_text(error, "loader_state task create failed");
        } else {
            sys_log_text(info, "loader_state task created (%ums)",
                        (unsigned)LOADER_STATE_PERIOD_MS);
        }
    }

    /* 低频 UI 任务 */
    {
        static const osThreadAttr_t ui_attr = {
            .name       = "loaderUi",
            .stack_size = LOADER_UI_TASK_STACK * 4u,
            .priority   = LOADER_UI_TASK_PRIO,
        };

        osThreadId_t tid = osThreadNew(loader_ui_task, NULL, &ui_attr);
        if (tid == NULL) {
            sys_log_text(error, "loader_ui task create failed");
        } else {
            sys_log_text(info, "loader_ui task created (%ums)",
                        (unsigned)LOADER_UI_POLL_MS);
        }
    }

    sys_log_text(info, "loader_task_start done (HF TIM / MF state / LF UI)");
}
