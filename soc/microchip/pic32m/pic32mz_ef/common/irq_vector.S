/*
 * Copyright (c) 2026 Microchip Technology Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Interrupt vector trampoline for PIC32MZ.
 * Placed at EBASE+0x200 (.vector_default section inside .vectors).
 * With Cause.IV=1 and INTCON.MVEC=0, ALL interrupts arrive here.
 */

.section .vector_irq_trampoline, "ax", @progbits
.set noreorder
.global _pic32mz_irq_trampoline
_pic32mz_irq_trampoline:
	la	$26, _pic32mz_interrupt
	jr	$26
	nop
