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

	/* CP0 hazard barrier: ensure Status.IE takes effect */
	__asm__ volatile("ehb");

	/* Spin instead of wait - test if timer interrupt fires */
	__asm__ volatile("nop");
	__asm__ volatile("nop");
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
