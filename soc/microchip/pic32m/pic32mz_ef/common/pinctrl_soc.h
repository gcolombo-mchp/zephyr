/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PIC32MZ PPS (Peripheral Pin Select) pinctrl SOC definitions.
 *
 * Pinmux encoding for device tree:
 *   bits [31:16] = PPS register offset from base 0xBF801400
 *   bits [15:8]  = value to write to the register
 *   bits [7:0]   = direction (0 = input, 1 = output)
 */

#ifndef ZEPHYR_SOC_MICROCHIP_PIC32M_PIC32MZ_EF_COMMON_PINCTRL_SOC_H
#define ZEPHYR_SOC_MICROCHIP_PIC32M_PIC32MZ_EF_COMMON_PINCTRL_SOC_H

#include <zephyr/devicetree.h>
#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pinctrl_soc_pin {
	uint32_t pinmux;
} pinctrl_soc_pin_t;

/**
 * @brief Utility macro to initialize each pin from device tree.
 */
#define Z_PINCTRL_STATE_PIN_INIT(node_id, prop, idx) \
	{ .pinmux = DT_PROP_BY_IDX(node_id, prop, idx) },

/**
 * @brief Utility macro to initialize state pins contained in a given property.
 */
#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop) \
	{ DT_FOREACH_CHILD_VARGS(DT_PHANDLE(node_id, prop), DT_FOREACH_PROP_ELEM, pinmux, \
				 Z_PINCTRL_STATE_PIN_INIT) }

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SOC_MICROCHIP_PIC32M_PIC32MZ_EF_COMMON_PINCTRL_SOC_H */
