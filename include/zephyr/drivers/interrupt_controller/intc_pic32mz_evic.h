/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_INTERRUPT_CONTROLLER_INTC_PIC32MZ_EVIC_H_
#define ZEPHYR_INCLUDE_DRIVERS_INTERRUPT_CONTROLLER_INTC_PIC32MZ_EVIC_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set interrupt priority and sub-priority for an EVIC vector.
 *
 * @param irq         IRQ number (0-212)
 * @param priority    Priority level (1-7, where 7 is highest; 0 = disabled)
 * @param subpriority Sub-priority level (0-3, where 3 is highest within same priority)
 */
void pic32mz_evic_irq_set_priority(unsigned int irq, unsigned int priority,
				    unsigned int subpriority);

/**
 * @brief Clear the interrupt flag for an EVIC vector.
 *
 * Must be called to acknowledge a non-persistent interrupt after servicing.
 * For persistent interrupts, the flag re-asserts while the condition is active.
 *
 * @param irq  IRQ number (0-212)
 */
void pic32mz_evic_irq_clear_flag(unsigned int irq);

/**
 * @brief Configure external interrupt edge polarity.
 *
 * PIC32MZ provides 5 external interrupt pins (INT0-INT4). Each can be
 * configured for rising or falling edge detection.
 *
 * @param ext_int  External interrupt number (0-4)
 * @param rising   true = rising edge trigger, false = falling edge trigger
 */
void pic32mz_evic_ext_int_polarity(unsigned int ext_int, bool rising);

/**
 * @brief Configure shadow register set assignment for interrupt priorities.
 *
 * Shadow register sets eliminate the need for software context save on ISR
 * entry, reducing interrupt latency. PIC32MZ EF has 7 shadow sets that can
 * be assigned to any priority level.
 *
 * @param priss_val  PRISS register value encoding priority-to-shadow-set mapping.
 *                   Each nibble (4 bits) assigns a shadow set to a priority level.
 *                   0 = feature disabled (no shadow sets).
 */
void pic32mz_evic_configure_shadow_sets(uint32_t priss_val);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_INTERRUPT_CONTROLLER_INTC_PIC32MZ_EVIC_H_ */
