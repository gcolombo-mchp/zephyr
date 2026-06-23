/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PIC32MZ Device Configuration Words.
 *
 * These 4 words are placed in Boot Flash configuration space at fixed
 * addresses. The hardware reads them at power-on reset BEFORE code execution,
 * configuring oscillator source, PLL, watchdog, debug, and protection settings.
 *
 * Physical addresses:
 *   DEVCFG3: 0x1FC0FFC0  (virtual 0xBFC0FFC0)
 *   DEVCFG2: 0x1FC0FFC4  (virtual 0xBFC0FFC4)
 *   DEVCFG1: 0x1FC0FFC8  (virtual 0xBFC0FFC8)
 *   DEVCFG0: 0x1FC0FFCC  (virtual 0xBFC0FFCC)
 *
 * Values are controlled by Kconfig symbols PIC32MZ_DEVCFG0..3.
 * Board defconfig should override the SoC defaults for correct clock and
 * peripheral setup.
 */

#include <zephyr/kernel.h>
#include <zephyr/linker/sections.h>

#define PIC32MZ_DEVCFG_SECTION __attribute__((section(".pic32mz_devcfg"), used))

static const uint32_t PIC32MZ_DEVCFG_SECTION pic32mz_devcfg[] = {
	CONFIG_PIC32MZ_DEVCFG3,  /* 0xBFC0FFC0 */
	CONFIG_PIC32MZ_DEVCFG2,  /* 0xBFC0FFC4 */
	CONFIG_PIC32MZ_DEVCFG1,  /* 0xBFC0FFC8 */
	CONFIG_PIC32MZ_DEVCFG0,  /* 0xBFC0FFCC */
};
