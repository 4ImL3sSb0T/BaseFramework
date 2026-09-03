#include "board/board.h"

#include "spi.h"
#include "spi_flash.h"
#include "sys_time.h"
#include "sys_log.h"
#include "rtt.h"
#include "uart_async.h"
#include "shell_port.h"
#include "lfs.h"
#include "lfs_port.h"
#include "lib/tools/common_def.h"

void board_early_init(void)
{
    hspi2.Instance->CFG1 = (hspi2.Instance->CFG1 & ~SPI_CFG1_MBR)
                         | (2UL << SPI_CFG1_MBR_Pos);
    __HAL_SPI_ENABLE(&hspi2);
    spi_flash_read_write_byte(0xFF);
}

void board_init(void)
{
    dwt_init();
    sys_log_init(SYS_LOG_RTT);
    log_rtt_println("=== board_init ===");

    if (uart_async_init() != EXIT_OK) {
        log_rtt_println("uart_async_init FAIL");
    } else if (uart_async_start() != EXIT_OK) {
        log_rtt_println("uart_async_start FAIL");
    } else if (shell_port_init() != EXIT_OK) {
        log_rtt_println("shell_port_init FAIL");
    } else if (shell_port_start() != EXIT_OK) {
        log_rtt_println("shell_port_start FAIL");
    } else {
        log_rtt_println("shell + uart_async OK (USART1)");
    }

    lfs_port_init();
    log_rtt_println("lfs_port_init OK");

    lfs_t lfs;
    int err = lfs_mount(&lfs, &g_lfs_cfg);
    if (err) {
        log_rtt_println("lfs_mount failed");
    } else {
        log_rtt_println("lfs_mount OK");
    }
}
