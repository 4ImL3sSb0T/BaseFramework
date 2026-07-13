#include "loader_task.h"

#include "cmsis_os2.h"
#include "service/sys/sys_log.h"

#ifndef LOADER_UI_TASK_STACK
#define LOADER_UI_TASK_STACK    1024u
#endif

#ifndef LOADER_UI_TASK_PRIO
#define LOADER_UI_TASK_PRIO     osPriorityBelowNormal
#endif

/* LOADER_UI_POLL_MS 定义在 loader_task.h（默认 67ms ≈ 15fps） */

static void loader_ui_task(void *argument)
{
    (void)argument;

    loader_ui_init();
    sys_log_text(info, "loader_ui task running, poll=%ums", (unsigned)LOADER_UI_POLL_MS);

    for (;;) {
        loader_ui_poll();
        osDelay(LOADER_UI_POLL_MS);
    }
}

void loader_task_init(void)
{
    static const osThreadAttr_t ui_attr = {
        .name       = "loaderUi",
        .stack_size = LOADER_UI_TASK_STACK * 4u, /* CMSIS: bytes; words*4 for ARM */
        .priority   = LOADER_UI_TASK_PRIO,
    };

    osThreadId_t tid = osThreadNew(loader_ui_task, NULL, &ui_attr);
    if (tid == NULL) {
        sys_log_text(error, "loader_ui task create failed");
    } else {
        sys_log_text(info, "loader_ui task created");
    }
}
