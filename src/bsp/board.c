#include "bsp/board.h"

#include "spi.h"
#include "driver/flash/spi_flash.h"
#include "bsp/sys/sys_time.h"
#include "bsp/sys/sys_log.h"
#include "bsp/rtt/rtt.h"
#include "driver/uart/uart_async.h"
#include "service/shell/shell_port.h"
#include "lfs.h"
#include "service/storage/lfs_port.h"
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
