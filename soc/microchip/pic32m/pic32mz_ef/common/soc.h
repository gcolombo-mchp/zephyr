/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SOC_MICROCHIP_PIC32MZ_EF_SOC_H_
#define SOC_MICROCHIP_PIC32MZ_EF_SOC_H_

/*
 * MIPS CP0 Core Timer interrupt.
 * On PIC32MZ EF, the Core Timer is EVIC IRQ 0 (vector 0).
 * This is used by drivers/timer/mips_cp0_timer.c
 */
#define MIPS_MACHINE_TIMER_IRQ 0

#endif /* SOC_MICROCHIP_PIC32MZ_EF_SOC_H_ */
