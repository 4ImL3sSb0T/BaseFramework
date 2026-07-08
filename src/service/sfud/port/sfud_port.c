/*
 * This file is part of the Serial Flash Universal Driver Library.
 *
 * Copyright (c) 2016-2018, Armink, <armink.ztl@gmail.com>
 *
 * Function: Portable interface for STM32H750 — thin adapter over BSP layer.
 */

#include "../inc/sfud.h"
#include "spi_flash.h"
#include "debug.h"
#include <stdarg.h>
#include <stdio.h>

/* ── SFUD required: SPI write then read ──────────────────────────────── */

static sfud_err spi_write_read(const sfud_spi *spi, const uint8_t *write_buf,
                               size_t write_size, uint8_t *read_buf, size_t read_size) {
    (void)spi;
    int rc = spi_flash_write_read(write_buf, write_size, read_buf, read_size);
    if (rc != 0) {
        return (rc == -1) ? SFUD_ERR_WRITE : SFUD_ERR_READ;
    }
    return SFUD_SUCCESS;
}

/* ── Lock / unlock ───────────────────────────────────────────────────── */

static void spi_lock(const sfud_spi *spi)   { (void)spi; }
static void spi_unlock(const sfud_spi *spi) { (void)spi; }

/* ── Port init — SPI2 already configured by CubeMX ───────────────────── */

sfud_err sfud_spi_port_init(sfud_flash *flash) {
    flash->spi.wr        = spi_write_read;
    flash->spi.lock      = spi_lock;
    flash->spi.unlock    = spi_unlock;
    flash->spi.user_data = NULL;
    flash->retry.delay   = NULL;
    flash->retry.times   = 10000;

    spi_flash_init();

    return SFUD_SUCCESS;
}

/* ── Log forwarding ──────────────────────────────────────────────────── */

void sfud_log_debug(const char *file, const long line, const char *format, ...) {
    log_rtt_printf("[SFUD](%s:%ld) ", file, line);
    va_list args;
    va_start(args, format);
    log_rtt_vprintf(format, args);
    va_end(args);
    log_rtt_println("");
}

void sfud_log_info(const char *format, ...) {
    log_rtt_printf("[SFUD]");
    va_list args;
    va_start(args, format);
    log_rtt_vprintf(format, args);
    va_end(args);
    log_rtt_println("");
}
