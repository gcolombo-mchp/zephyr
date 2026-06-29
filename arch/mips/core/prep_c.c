/**********************************************************************************************
 * © 2026 Microchip Technology Inc. and its subsidiares. All rights reserved.
 * This software includes AI generated code created using significant prompting. This
 * software is provided AS IS; you are Responsible for reviewing, testing, and validating for
 * your application.
 **********************************************************************************************/
/*
 * Copyright (c) 2020 Antony Pavlov <antonynpavlov@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Full C support initialization
 */

#include <zephyr/irq.h>
#include <zephyr/platform/hooks.h>
#include <zephyr/arch/cache.h>
#include <zephyr/arch/common/xip.h>
#include <zephyr/arch/common/init.h>

static void interrupt_init(void)
{
#ifdef CONFIG_SOC_SERIES_PIC32MZ_EFH
	/*
	 * On PIC32MZ with XC32 crt0:
	 * - EBASE is already set to 0x9D000000 (program flash) by crt0
	 * - Exception vector at EBASE+0x180 points to our pic32mz_isr.S
	 * - No need to copy vectors to RAM
	 * - Must clear BEV so exceptions go to EBASE, not boot flash
	 * - Must enable Status.IM2 since ALL EVIC interrupts use Cause.IP2
	 */
	extern uint32_t mips_cp0_status_int_mask;
	unsigned long status;

	irq_lock();
	mips_cp0_status_int_mask = 0;
	status = read_c0_status();
	status &= ~ST0_BEV;
	status &= ~(0x3F << 10);
	write_c0_status(status);
#else
	extern char __isr_vec[];
	extern uint32_t mips_cp0_status_int_mask;
	unsigned long ebase;

	irq_lock();

	mips_cp0_status_int_mask = 0;

	ebase = 0x80000000;

	memcpy((void *)(ebase + 0x180), __isr_vec, 0x80);

	/*
	 * Disable boot exception vector in BOOTROM,
	 * use exception vector in RAM.
	 */
	write_c0_status(read_c0_status() & ~(ST0_BEV));
#endif
}

/**
 *
 * @brief Prepare to and run C code
 *
 * This routine prepares for the execution of and runs C code.
 *
 * @return N/A
 */

FUNC_NORETURN void z_prep_c(void)
{
	soc_prep_hook();

	arch_bss_zero();

	interrupt_init();
#if CONFIG_ARCH_CACHE
	arch_cache_init();
#endif

	z_cstart();
	CODE_UNREACHABLE;
}
