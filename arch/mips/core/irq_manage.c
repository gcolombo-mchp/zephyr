/*
 * Copyright (c) 2020 Antony Pavlov <antonynpavlov@gmail.com>
 *
 * based on arch/nios2/core/irq_manage.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <kswap.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(os, CONFIG_KERNEL_LOG_LEVEL);


uint32_t mips_cp0_status_int_mask;


FUNC_NORETURN void z_irq_spurious(const void *unused)
{
	unsigned long cause;

	ARG_UNUSED(unused);
	cause = (read_c0_cause() & CAUSE_EXP_MASK) >> CAUSE_EXP_SHIFT;

	LOG_ERR("Spurious interrupt detected! CAUSE: %ld", cause);

	z_mips_fatal_error(K_ERR_SPURIOUS_IRQ, NULL);
}

/*
 * Default arch_irq_enable/disable implementations for direct CP0 HW interrupts.
 * When PIC32MZ EVIC is used, these are provided by the EVIC driver instead,
 * since all 213 peripheral IRQs are managed through the EVIC registers.
 */
#ifndef CONFIG_INTC_PIC32MZ_EVIC

void arch_irq_enable(unsigned int irq)
{
	unsigned int key;
	uint32_t irq_mask;

	key = irq_lock();

	irq_mask = ST0_IP0 << irq;
	mips_cp0_status_int_mask |= irq_mask;
	write_c0_status(read_c0_status() | irq_mask);

	irq_unlock(key);
}

void arch_irq_disable(unsigned int irq)
{
	unsigned int key;
	uint32_t irq_mask;

	key = irq_lock();

	irq_mask = ST0_IP0 << irq;
	mips_cp0_status_int_mask &= ~irq_mask;
	write_c0_status(read_c0_status() & ~irq_mask);

	irq_unlock(key);
};

int arch_irq_is_enabled(unsigned int irq)
{
	return read_c0_status() & (ST0_IP0 << irq);
}

#endif /* !CONFIG_INTC_PIC32MZ_EVIC */

void z_mips_enter_irq(uint32_t ipending)
{
	_current_cpu->nested++;

#ifdef CONFIG_IRQ_OFFLOAD
	z_irq_do_offload();
#endif

	while (ipending) {
		int index;
		const struct _isr_table_entry *ite;

		if (IS_ENABLED(CONFIG_TRACING_ISR)) {
			sys_trace_isr_enter();
		}

		index = find_lsb_set(ipending) - 1;
		ipending &= ~BIT(index);

		ite = &_sw_isr_table[index];

		ite->isr(ite->arg);

		if (IS_ENABLED(CONFIG_TRACING_ISR)) {
			sys_trace_isr_exit();
		}
	}

	_current_cpu->nested--;

	if (IS_ENABLED(CONFIG_STACK_SENTINEL)) {
		z_check_stack_sentinel();
	}
}

/*
 * PIC32MZ EVIC variant: the CPU only sees IP2 asserting.
 * The EVIC driver's ISR (registered on HW IRQ 2) reads INTSTAT
 * and dispatches. So the z_mips_enter_irq still works: ipending
 * will have only bit 2 set, which dispatches to pic32mz_evic_isr.
 */

#ifdef CONFIG_DYNAMIC_INTERRUPTS
int arch_irq_connect_dynamic(unsigned int irq, unsigned int priority,
			     void (*routine)(const void *parameter),
			     const void *parameter, uint32_t flags)
{
	ARG_UNUSED(flags);
	ARG_UNUSED(priority);

	z_isr_install(irq, routine, parameter);
	return irq;
}
#endif /* CONFIG_DYNAMIC_INTERRUPTS */
