/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Peripheral Pin Select (PPS) pinctrl driver for PIC32MZ devices.
 *
 * PIC32MZ PPS has two types of registers:
 *   - Input registers: one per peripheral input function (e.g., U2RXR, SDI1R)
 *     Write the pin code to select which pin drives that input.
 *   - Output registers: one per remappable pin (e.g., RPG9R, RPB0R)
 *     Write the function code to select which peripheral drives that pin.
 *
 * Both register sets are protected by CFGCON.IOLOCK and SYSKEY.
 *
 * The pinmux value in the device tree encodes:
 *   bits [31:16] = register offset from PPS base (0xBF801400)
 *   bits [15:8]  = value to write
 *   bits [7:0]   = direction (0 = input register, 1 = output register)
 *
 * Reference: DS60001320H Section 12.0 "I/O Ports"
 * Reference: Harmony CSP peripheral/gpio_02467
 */

#include <zephyr/drivers/pinctrl.h>
#include <zephyr/irq.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pinctrl_pic32mz, CONFIG_PINCTRL_LOG_LEVEL);

/* PPS register base address */
#define PPS_BASE 0xBF801400

/* CFGCON register for IOLOCK */
#define CFGCON_ADDR    0xBF800000
#define CFGCON_IOLOCK  BIT(13)

/* SYSKEY register (PIC32MZ EF: 0xBF800030) */
#define SYSKEY_ADDR    0xBF800030

/* CLR/SET offsets */
#define REG_CLR 0x04
#define REG_SET 0x08

/* Pinmux encoding macros (matching dt-bindings header) */
#define PPS_PINMUX_REG_OFFSET(pinmux)  (((pinmux) >> 16) & 0xFFFF)
#define PPS_PINMUX_VALUE(pinmux)       (((pinmux) >> 8) & 0xFF)
#define PPS_PINMUX_DIR(pinmux)         ((pinmux) & 0xFF)

#define PPS_DIR_INPUT  0
#define PPS_DIR_OUTPUT 1

static void pps_syskey_unlock(void)
{
	volatile uint32_t *syskey = (volatile uint32_t *)SYSKEY_ADDR;

	*syskey = 0x00000000;
	*syskey = 0xAA996655;
	*syskey = 0x556699AA;
}

static void pps_syskey_lock(void)
{
	volatile uint32_t *syskey = (volatile uint32_t *)SYSKEY_ADDR;

	*syskey = 0x33333333;
}

static void pps_iolock_clear(void)
{
	volatile uint32_t *cfgcon_clr = (volatile uint32_t *)(CFGCON_ADDR + REG_CLR);

	*cfgcon_clr = CFGCON_IOLOCK;
}

static void pps_iolock_set(void)
{
	volatile uint32_t *cfgcon_set = (volatile uint32_t *)(CFGCON_ADDR + REG_SET);

	*cfgcon_set = CFGCON_IOLOCK;
}

int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg)
{
	ARG_UNUSED(reg);

	if (pin_cnt == 0) {
		return 0;
	}

	/* Disable interrupts - SYSKEY sequence is atomic and cannot be interrupted */
	unsigned int key = arch_irq_lock();

	/* Unlock PPS registers */
	pps_syskey_unlock();
	pps_iolock_clear();

	for (uint8_t i = 0; i < pin_cnt; i++) {
		uint32_t pinmux = pins[i].pinmux;
		uint32_t reg_offset = PPS_PINMUX_REG_OFFSET(pinmux);
		uint32_t value = PPS_PINMUX_VALUE(pinmux);
		volatile uint32_t *pps_reg;

		pps_reg = (volatile uint32_t *)(PPS_BASE + reg_offset);
		*pps_reg = value;
	}

	/* Lock PPS registers */
	pps_iolock_set();
	pps_syskey_lock();

	arch_irq_unlock(key);

	return 0;
}
