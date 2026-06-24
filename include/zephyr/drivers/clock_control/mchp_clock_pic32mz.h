/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCHP_CLOCK_PIC32MZ_H_
#define ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCHP_CLOCK_PIC32MZ_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PIC32MZ clock source identifiers.
 *
 * These match the NOSC/COSC field encoding in the OSCCON register.
 */
#define PIC32MZ_CLKSRC_FRC      0x0
#define PIC32MZ_CLKSRC_SPLL     0x1
#define PIC32MZ_CLKSRC_POSC     0x2
#define PIC32MZ_CLKSRC_SOSC     0x4
#define PIC32MZ_CLKSRC_LPRC     0x5
#define PIC32MZ_CLKSRC_FRCDIV   0x7

/**
 * @brief PIC32MZ clock subsystem identifiers.
 *
 * Used as clock_control_subsys_t when calling clock_control APIs.
 * Example: clock_control_get_rate(dev, (void *)PIC32MZ_PBCLK2, &rate);
 */

/* Peripheral Bus Clocks (1-8) */
#define PIC32MZ_PBCLK1 1
#define PIC32MZ_PBCLK2 2
#define PIC32MZ_PBCLK3 3
#define PIC32MZ_PBCLK4 4
#define PIC32MZ_PBCLK5 5
#define PIC32MZ_PBCLK6 6
#define PIC32MZ_PBCLK7 7
#define PIC32MZ_PBCLK8 8

/**
 * @brief USB PLL clock (48 MHz fixed output for USB HS PHY).
 *
 * On PIC32MZ EF, the USB PLL takes POSC as input (12 or 24 MHz, selected
 * by UPLLFSEL in DEVCFG2) and produces a fixed 48 MHz output clock for
 * the USB High-Speed PHY. This PLL is not runtime-configurable.
 *
 * Prerequisites:
 *   - POSC must be enabled (POSCMOD != OFF in DEVCFG1)
 *   - UPLLFSEL in DEVCFG2 must match the POSC frequency (12 or 24 MHz)
 *   - USB module must not be PMD-disabled (PMD5<24> = 0)
 */
#define PIC32MZ_CLK_USB    0x10

/**
 * @brief System clock (SYSCLK) - the main CPU clock after PLL/oscillator.
 */
#define PIC32MZ_CLK_SYSCLK 0x20

/**
 * @brief Operating Performance Point for runtime clock switching.
 *
 * Pass a pointer to this structure as cfg_data in clock_control_configure()
 * to switch the PIC32MZ to a different operating point.
 *
 * Example:
 *   struct pic32mz_clock_opp low_power = {
 *       .clock_source = PIC32MZ_CLKSRC_FRC,
 *       .pbclk_div = {1, 1, 1, 1, 1, 0, 1, 1},
 *   };
 *   clock_control_configure(clk_dev, NULL, &low_power);
 */
struct pic32mz_clock_opp {
	/** Clock source (PIC32MZ_CLKSRC_*) to switch SYSCLK to */
	uint8_t clock_source;
	/**
	 * PBCLK dividers [0..7] for PBCLK1..PBCLK8.
	 * Value = division factor (1-128). 0 = do not change.
	 */
	uint8_t pbclk_div[8];
};

/**
 * @brief Reference clock source selection (ROSEL field in REFOxCON).
 */
#define PIC32MZ_REFO_SRC_SYSCLK  0x0
#define PIC32MZ_REFO_SRC_PBCLK1  0x1
#define PIC32MZ_REFO_SRC_POSC    0x2
#define PIC32MZ_REFO_SRC_FRC     0x3
#define PIC32MZ_REFO_SRC_LPRC    0x4
#define PIC32MZ_REFO_SRC_SOSC    0x5
#define PIC32MZ_REFO_SRC_SPLL    0x7
#define PIC32MZ_REFO_SRC_REFCLKI 0x8

/**
 * @brief Reference clock configuration.
 *
 * Used with pic32mz_refclk_configure() to set up REFO1-4.
 */
struct pic32mz_refclk_cfg {
	/** Reference clock index (1-4) */
	uint8_t refo_id;
	/** Source selection (PIC32MZ_REFO_SRC_*) */
	uint8_t source;
	/** Divider value (0 = no division, N = divide by 2*N) */
	uint16_t divider;
	/** Trim value for fine frequency adjustment (0 = no trim) */
	uint16_t trim;
	/** Enable the reference clock module */
	bool enable;
	/** Enable output on REFCLKOx pin */
	bool output_enable;
};

/**
 * @brief PMD register and bit encoding for peripheral module disable.
 *
 * Encode as: PIC32MZ_PMD(register, bit)
 * Example: PIC32MZ_PMD(5, 0) = UART1 (PMD5<0>)
 */
#define PIC32MZ_PMD(reg, bit) (((uint16_t)(reg) << 8) | (uint16_t)(bit))
#define PIC32MZ_PMD_REG(val)  ((uint8_t)((val) >> 8))
#define PIC32MZ_PMD_BIT(val)  ((uint8_t)((val) & 0xFF))

/* Common PMD assignments for PIC32MZ EF */
#define PIC32MZ_PMD_ADC1     PIC32MZ_PMD(1, 0)
#define PIC32MZ_PMD_CVREF    PIC32MZ_PMD(1, 12)
#define PIC32MZ_PMD_CMP1     PIC32MZ_PMD(2, 0)
#define PIC32MZ_PMD_CMP2     PIC32MZ_PMD(2, 1)
#define PIC32MZ_PMD_IC1      PIC32MZ_PMD(3, 0)
#define PIC32MZ_PMD_OC1      PIC32MZ_PMD(3, 16)
#define PIC32MZ_PMD_T1       PIC32MZ_PMD(4, 0)
#define PIC32MZ_PMD_T2       PIC32MZ_PMD(4, 1)
#define PIC32MZ_PMD_T3       PIC32MZ_PMD(4, 2)
#define PIC32MZ_PMD_U1       PIC32MZ_PMD(5, 0)
#define PIC32MZ_PMD_U2       PIC32MZ_PMD(5, 1)
#define PIC32MZ_PMD_U3       PIC32MZ_PMD(5, 2)
#define PIC32MZ_PMD_U4       PIC32MZ_PMD(5, 3)
#define PIC32MZ_PMD_U5       PIC32MZ_PMD(5, 4)
#define PIC32MZ_PMD_U6       PIC32MZ_PMD(5, 5)
#define PIC32MZ_PMD_SPI1     PIC32MZ_PMD(5, 8)
#define PIC32MZ_PMD_SPI2     PIC32MZ_PMD(5, 9)
#define PIC32MZ_PMD_I2C1     PIC32MZ_PMD(5, 16)
#define PIC32MZ_PMD_I2C2     PIC32MZ_PMD(5, 17)
#define PIC32MZ_PMD_USB      PIC32MZ_PMD(5, 24)
#define PIC32MZ_PMD_CAN1     PIC32MZ_PMD(5, 28)
#define PIC32MZ_PMD_CAN2     PIC32MZ_PMD(5, 29)
#define PIC32MZ_PMD_ETH      PIC32MZ_PMD(6, 4)
#define PIC32MZ_PMD_SQI1     PIC32MZ_PMD(6, 23)
#define PIC32MZ_PMD_CRYPTO   PIC32MZ_PMD(7, 0)
#define PIC32MZ_PMD_RNG      PIC32MZ_PMD(7, 1)

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCHP_CLOCK_PIC32MZ_H_ */
