/*
 * GD32H759 LittleFS Port — SPI4 NOR Flash
 *
 * Block-device callbacks bridging LittleFS to the SPI flash driver
 * (Source/Bsp/Flash/spi_flash.c).
 *
 * Call lfs_port_init() once during system startup (before mounting)
 * to initialize the SPI4 peripheral and GPIO pins.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lfs_port.h"
#include "spi_flash.h"
#include <stddef.h>

/* FreeRTOS mutex for thread safety */
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t g_lfs_mutex = NULL;

/* ── Static buffers ─────────────────────────────────────────────── */

uint8_t lfs_read_buf     [LFS_FLASH_CACHE_SIZE];
uint8_t lfs_prog_buf     [LFS_FLASH_CACHE_SIZE];
uint8_t lfs_lookahead_buf[LFS_FLASH_LOOKAHEAD_SIZE];

/* ── Global configuration ───────────────────────────────────────── */

const struct lfs_config g_lfs_cfg = {
    .context       = NULL,
    .read          = lfs_port_read,
    .prog          = lfs_port_prog,
    .erase         = lfs_port_erase,
    .sync          = lfs_port_sync,
#ifdef LFS_THREADSAFE
    .lock          = lfs_port_lock,
    .unlock        = lfs_port_unlock,
#endif

    .read_size     = LFS_FLASH_READ_SIZE,      /*   1 */
    .prog_size     = LFS_FLASH_PROG_SIZE,      /* 256 */
    .block_size    = LFS_FLASH_BLOCK_SIZE,     /* 4096 */
    .block_count   = LFS_FLASH_BLOCK_COUNT,    /* see lfs_port.h */
    .block_cycles  = 500,

    .cache_size    = LFS_FLASH_CACHE_SIZE,     /* 256 */
    .lookahead_size= LFS_FLASH_LOOKAHEAD_SIZE, /*  32 */
    .read_buffer   = lfs_read_buf,
    .prog_buffer   = lfs_prog_buf,
    .lookahead_buffer = lfs_lookahead_buf,

    .name_max       = 0,
    .file_max       = 0,
    .attr_max       = 0,
    .metadata_max   = 0,
    .compact_thresh = 0,
    .inline_max     = 0,
};

/* ── Public API ─────────────────────────────────────────────────── */

void lfs_port_init(void)
{
    spi_flash_init();

    /* Create mutex for thread safety (if not already created) */
    if (g_lfs_mutex == NULL) {
        g_lfs_mutex = xSemaphoreCreateMutex();
        /* If creation fails, g_lfs_mutex remains NULL and lock/unlock will be no-ops */
    }
}

/* ── Block-device callbacks ───────────────────────────────────────
 *
 * Address translation:  physical_addr = block * LFS_FLASH_BLOCK_SIZE + off
 *
 * All writes use spi_flash_buffer_write() which auto-splits across
 * page boundaries (256 B per page).  The GD25X/Q series supports
 * page-program wrapping within a 256 B page — buffer_write handles
 * the split correctly.
 */

int lfs_port_read(const struct lfs_config *c, lfs_block_t block,
                  lfs_off_t off, void *buffer, lfs_size_t size)
{
    (void)c;
    uint32_t addr = (uint32_t)block * LFS_FLASH_BLOCK_SIZE + off;
    return spi_flash_buffer_read((uint8_t *)buffer, addr, size) == 0 ? 0 : LFS_ERR_IO;
}

int lfs_port_prog(const struct lfs_config *c, lfs_block_t block,
                  lfs_off_t off, const void *buffer, lfs_size_t size)
{
    (void)c;
    uint32_t addr = (uint32_t)block * LFS_FLASH_BLOCK_SIZE + off;
    return spi_flash_buffer_write((const uint8_t *)buffer, addr, size) == 0 ? 0 : LFS_ERR_IO;
}

int lfs_port_erase(const struct lfs_config *c, lfs_block_t block)
{
    (void)c;
    uint32_t addr = (uint32_t)block * LFS_FLASH_BLOCK_SIZE;
    return spi_flash_sector_erase(addr) == 0 ? 0 : LFS_ERR_IO;
}

int lfs_port_sync(const struct lfs_config *c)
{
    (void)c;
    /* SPI writes go directly to the device — no intermediate cache */
    return 0;
}

/* ── Thread safety callbacks ──────────────────────────────────────── */

int lfs_port_lock(const struct lfs_config *c)
{
    (void)c;
    if (g_lfs_mutex != NULL) {
        return xSemaphoreTake(g_lfs_mutex, portMAX_DELAY) == pdTRUE ? 0 : LFS_ERR_IO;
    }
    return 0;  /* No mutex created, assume single-threaded */
}

int lfs_port_unlock(const struct lfs_config *c)
{
    (void)c;
    if (g_lfs_mutex != NULL) {
        return xSemaphoreGive(g_lfs_mutex) == pdTRUE ? 0 : LFS_ERR_IO;
    }
    return 0;  /* No mutex created, assume single-threaded */
}
