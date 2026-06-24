/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PIC32MZ Enhanced Vectored Interrupt Controller (EVIC) driver.
 * Uses SINGLE-VECTOR MODE (INTCON.MVEC = 0) as configured by XC32 crt0.
 *
 * In single-vector mode, ALL interrupts go through the general exception
 * vector at EBASE + 0x180. XC32 places _general_exception_context there
 * which calls _general_exception_handler (weak symbol we override).
 *
 * Our handler checks Cause.ExcCode:
 *   - ExcCode == 0: interrupt → read INTSTAT, dispatch via _sw_isr_table
 *   - ExcCode != 0: general exception → software reset or hang
 *
 * Register layout (base 0xBF810000):
 *   INTCON   +0x0000  (MVEC stays 0 for single-vector mode)
 *   INTSTAT  +0x0020  (current vector/priority when interrupt active)
 *   IFS0-6   +0x0040  Interrupt Flag Status
 *   IEC0-6   +0x00C0  Interrupt Enable Control
 *   IPC0-53  +0x0140  Interrupt Priority Control
 */

#include <xc.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/sw_isr_table.h>
#include <zephyr/sys/util.h>

#define DT_DRV_COMPAT microchip_pic32mz_evic

/* EVIC base address from DT */
#define EVIC_BASE DT_INST_REG_ADDR(0)

/* Register offsets */
#define EVIC_INTCON   0x0000
#define EVIC_INTSTAT  0x0020
#define EVIC_IFS_BASE 0x0040
#define EVIC_IEC_BASE 0x00C0
#define EVIC_IPC_BASE 0x0140

/* SFR spacing (each register occupies 0x10 with CLR/SET/INV) */
#define SFR_SPACING   0x10
#define REG_CLR       0x04
#define REG_SET       0x08

/* IPC register: 4 IRQs per register, 8 bits each */
#define IPC_PRIO_MASK     0x07
#define IPC_SUBPRIO_MASK  0x03

/* Number of IRQs */
#define EVIC_NUM_IRQS DT_INST_PROP_OR(0, microchip_num_irqs, 213)

static inline uint32_t evic_read(uint32_t offset)
{
	return *(volatile uint32_t *)(EVIC_BASE + offset);
}

static inline void evic_write(uint32_t offset, uint32_t val)
{
	*(volatile uint32_t *)(EVIC_BASE + offset) = val;
}

static inline void evic_set(uint32_t offset, uint32_t bits)
{
	*(volatile uint32_t *)(EVIC_BASE + offset + REG_SET) = bits;
}

static inline void evic_clr(uint32_t offset, uint32_t bits)
{
	*(volatile uint32_t *)(EVIC_BASE + offset + REG_CLR) = bits;
}

/*
 * arch_irq_enable - Enable an EVIC interrupt source.
 */
void arch_irq_enable(unsigned int irq)
{
	if (irq >= EVIC_NUM_IRQS) {
		return;
	}
	uint32_t reg_offset = EVIC_IEC_BASE + (irq / 32) * SFR_SPACING;

	evic_set(reg_offset, BIT(irq % 32));
}

/*
 * arch_irq_disable - Disable an EVIC interrupt source.
 */
void arch_irq_disable(unsigned int irq)
{
	if (irq >= EVIC_NUM_IRQS) {
		return;
	}
	uint32_t reg_offset = EVIC_IEC_BASE + (irq / 32) * SFR_SPACING;

	evic_clr(reg_offset, BIT(irq % 32));
}

/*
 * arch_irq_is_enabled - Check if an EVIC interrupt source is enabled.
 */
int arch_irq_is_enabled(unsigned int irq)
{
	if (irq >= EVIC_NUM_IRQS) {
		return 0;
	}
	uint32_t reg_offset = EVIC_IEC_BASE + (irq / 32) * SFR_SPACING;

	return !!(evic_read(reg_offset) & BIT(irq % 32));
}

/*
 * Set priority for an IRQ.
 */
void pic32mz_evic_irq_set_priority(unsigned int irq, unsigned int priority,
				    unsigned int subpriority)
{
	if (irq >= EVIC_NUM_IRQS || priority > 7 || subpriority > 3) {
		return;
	}
	uint32_t ipc_offset = EVIC_IPC_BASE + (irq / 4) * SFR_SPACING;
	uint32_t shift = (irq % 4) * 8;
	uint32_t mask = 0x1F << shift;
	uint32_t val = ((priority & IPC_PRIO_MASK) << 2 |
			(subpriority & IPC_SUBPRIO_MASK)) << shift;

	evic_clr(ipc_offset, mask);
	evic_set(ipc_offset, val);
}

/*
 * Clear interrupt flag for an IRQ.
 */
void pic32mz_evic_irq_clear_flag(unsigned int irq)
{
	if (irq >= EVIC_NUM_IRQS) {
		return;
	}
	uint32_t reg_offset = EVIC_IFS_BASE + (irq / 32) * SFR_SPACING;

	evic_clr(reg_offset, BIT(irq % 32));
}

/*
 * pic32mz_evic_dispatch - Called from pic32mz_isr.S interrupt handler.
 * Reads INTSTAT to determine the interrupt source and dispatches via
 * the Zephyr _sw_isr_table.
 */
void pic32mz_evic_dispatch(void)
{
	uint32_t intstat = evic_read(EVIC_INTSTAT);
	unsigned int vector = intstat & 0xFF;

	if (vector < EVIC_NUM_IRQS) {
		const struct _isr_table_entry *ite = &_sw_isr_table[vector];

		ite->isr(ite->arg);
		pic32mz_evic_irq_clear_flag(vector);
	}
}

static int pic32mz_evic_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	/*
	 * Force single-vector mode: clear INTCON.MVEC.
	 * XC32's vector_offset_init.o may set MVEC=1 during boot.
	 * With MVEC=0, ALL interrupts go to EBASE+0x180 where our
	 * pic32mz_isr.S handler dispatches them via INTSTAT.
	 */
	evic_clr(EVIC_INTCON, (1 << 12));

	/* Disable all interrupt sources */
	for (int i = 0; i < (EVIC_NUM_IRQS + 31) / 32; i++) {
		evic_write(EVIC_IEC_BASE + i * SFR_SPACING, 0);
	}

	/* Clear all interrupt flags */
	for (int i = 0; i < (EVIC_NUM_IRQS + 31) / 32; i++) {
		evic_write(EVIC_IFS_BASE + i * SFR_SPACING, 0);
	}

	/* Set default priority to 1 for all vectors */
	for (unsigned int irq = 0; irq < EVIC_NUM_IRQS; irq++) {
		pic32mz_evic_irq_set_priority(irq, 1, 0);
	}

	/* Enable global interrupts (Status.IE = 1) */
	__builtin_enable_interrupts();

	return 0;
}

DEVICE_DT_INST_DEFINE(0, pic32mz_evic_init, NULL,
		      NULL, NULL,
		      PRE_KERNEL_1, CONFIG_INTC_INIT_PRIORITY,
		      NULL);
