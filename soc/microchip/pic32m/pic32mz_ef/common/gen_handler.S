/**********************************************************************************************
 * © 2026 Microchip Technology Inc. and its subsidiares. All rights reserved.
 * This software includes AI generated code created using significant prompting. This
 * software is provided AS IS; you are Responsible for reviewing, testing, and validating for
 * your application.
 **********************************************************************************************/
/*
 * Override XC32's default .gen_handler to jump to Zephyr's
 * _mips_interrupt instead of _general_exception_context.
 *
 * This is placed at EBASE+0x180 by the linker (.app_excpt section).
 * Zephyr's handler manages syscall (context switch), interrupts, and faults.
 */

#include <zephyr/toolchain.h>

.section .gen_handler_zephyr, "ax", @progbits
.set noreorder
.global _gen_exception
_gen_exception:
	la	$26, _pic32mz_general_exception
	jr	$26
	nop
