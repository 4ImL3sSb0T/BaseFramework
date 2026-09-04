#include "app/app.h"

#include "bsp/board.h"
#include "loader_task.h"
#include "bsp/rtt/rtt.h"

void app_start(void)
{
    board_init();
    loader_task_start();
    log_rtt_println("app_start done");
}
