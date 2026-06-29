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

#include <zephyr/irq.h>

#include <zephyr/tracing/tracing.h>

static ALWAYS_INLINE void mips_idle(unsigned int key)
{
#if defined(CONFIG_TRACING)
	sys_trace_idle();
#endif

	/* unlock interrupts */
	irq_unlock(key);

#ifdef CONFIG_SOC_SERIES_PIC32MZ_EFH
	/* CP0 hazard barrier: ensure Status.IE takes effect before wait */
	__asm__ volatile("ehb");
#endif

	/* wait for interrupt */
	__asm__ volatile("wait");
}

#ifndef CONFIG_ARCH_HAS_CUSTOM_CPU_IDLE
void arch_cpu_idle(void)
{
	mips_idle(1);
}
#endif

#ifndef CONFIG_ARCH_HAS_CUSTOM_CPU_ATOMIC_IDLE
void arch_cpu_atomic_idle(unsigned int key)
{
	mips_idle(key);
}
#endif
