#include "FreeRTOS.h"
#include "task.h"
#include "lib/shell/shell.h"
#include "lib/shell/log/log.h"

/*
 * Task list (name / state / prio / stack HWM / number).
 * Full run-time % needs configGENERATE_RUN_TIME_STATS — not enabled yet.
 */
int cpu_usage(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

#if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS > 0)
    static char buf[512];
    vTaskList(buf);
    logPrintln("Name            State  Prio  Stack  Num");
    logPrintln("----------------------------------------");
    logPrintln("%s", buf);
#else
    logPrintln("task list unavailable (enable configUSE_STATS_FORMATTING_FUNCTIONS)");
#endif
    return 0;
}
