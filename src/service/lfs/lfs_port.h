/*
 * GD32H759 LittleFS Port — SPI4 NOR Flash
 *
 * Provides the lfs_config structure and block-device callbacks
 * bridging LittleFS to the SPI flash driver (Source/Bsp/Flash/).
 *
 * Call lfs_port_init() once before lfs_mount() to initialize SPI4.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LFS_PORT_H
#define LFS_PORT_H

#include "lfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Flash geometry ───────────────────────────────────────────────
 *
 * Values common to GD25Q64 (8 MB) and other 3-byte-address parts:
 *   Page (program unit):    256 bytes  → lfs_config.prog_size
 *   Sector (erase unit):   4096 bytes  → lfs_config.block_size
 *
 * read_size = 1 (SPI supports byte-granular reads).
 *
 * The driver uses 3-byte addressing (no EN4B) — max 16 MB.
 *   GD25Q64   ( 8 MB): 2048   ← default, matches confirmed board chip
 *   GD25Q128  (16 MB): 4096
 * >16 MB parts (e.g. GD25X512ME 64 MB) need 4-byte mode, which this
 * driver does NOT implement.  Confirm the chip with spi_flash_read_id().
 */

#define LFS_FLASH_READ_SIZE    1
#define LFS_FLASH_PROG_SIZE    256
#define LFS_FLASH_BLOCK_SIZE   4096
#define LFS_FLASH_BLOCK_COUNT  2048   /* 2048 × 4 KB = 8 MB (GD25Q64) */

/* ── Cache / lookahead sizes ────────────────────────────────────── */

#define LFS_FLASH_CACHE_SIZE      LFS_FLASH_PROG_SIZE   /* 256 B */
#define LFS_FLASH_LOOKAHEAD_SIZE  32                    /* tracks 256 blocks */

/* ── Static buffers (defined in lfs_port.c) ─────────────────────── */

extern uint8_t lfs_read_buf[LFS_FLASH_CACHE_SIZE];
extern uint8_t lfs_prog_buf[LFS_FLASH_CACHE_SIZE];
extern uint8_t lfs_lookahead_buf[LFS_FLASH_LOOKAHEAD_SIZE];

/* ── Global config ──────────────────────────────────────────────── */

extern const struct lfs_config g_lfs_cfg;

/* ── Public API ─────────────────────────────────────────────────── */

void lfs_port_init(void);   /* call once before mount */

int lfs_port_read (const struct lfs_config *c, lfs_block_t block,
                   lfs_off_t off, void *buffer, lfs_size_t size);
int lfs_port_prog (const struct lfs_config *c, lfs_block_t block,
                   lfs_off_t off, const void *buffer, lfs_size_t size);
int lfs_port_erase(const struct lfs_config *c, lfs_block_t block);
int lfs_port_sync (const struct lfs_config *c);

#ifdef LFS_THREADSAFE
int lfs_port_lock  (const struct lfs_config *c);
int lfs_port_unlock(const struct lfs_config *c);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LFS_PORT_H */
