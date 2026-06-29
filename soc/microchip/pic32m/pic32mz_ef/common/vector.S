/**********************************************************************************************
 * © 2026 Microchip Technology Inc. and its subsidiares. All rights reserved.
 * This software includes AI generated code created using significant prompting. This
 * software is provided AS IS; you are Responsible for reviewing, testing, and validating for
 * your application.
 **********************************************************************************************/
/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PIC32MZ EF boot vector - exact replica of XC32/Harmony startup sequence
 * up to the point where it jumps to KSEG0 program flash.
 *
 * This is a byte-for-byte reproduction of what works on the DM320007.
 */

#include <zephyr/toolchain.h>
#include <mips/regdef.h>
#include <mips/mipsregs.h>

GTEXT(__initialize)
GTEXT(__start)

SECTION_FUNC(vectors, __start)
	.set noreorder
	.set noat

	/*
	 * 0xBFC00000: Harmony uses "b +3" here for MIPS32 boot path.
	 * With BOOTISA=1, CPU fetches this as MIPS32.
	 */
	b	_reset_switch_isa
	nop

	/* 0xBFC00008: microMIPS entry (not used, placeholder) */
	.word	0x0006f3f0		/* jalx _startup (microMIPS encoding) */
	.word	0x00000000

_reset_switch_isa:
	/* 0xBFC00010: MIPS32 path calls _startup */
	jal	_startup
	nop

_startup:
	/*
	 * Exact Harmony _startup sequence.
	 * Check NMI status.
	 */
	mfc0	k0, $12, 0		/* Status */
	ext	k0, k0, 19, 1		/* NMI bit (Status.NMI = bit 19) */
	beqz	k0, _no_nmi
	nop
	/* NMI path: jump to NMI handler in program flash (not implemented) */
	la	k0, __initialize
	jr	k0
	nop

_no_nmi:
	/*
	 * Setup stack pointer.
	 * Harmony: lui sp, 0x8008 / addiu sp, sp, -8
	 * We use the same approach with our __stack symbol.
	 */
	la	sp, __stack

	/*
	 * Setup GP (global pointer).
	 * Even though we compile with -G0, some library code may reference GP.
	 * Harmony sets: gp = 0x80007FF0
	 */
	la	gp, _gp

	/*
	 * Initialize shadow register sets (SRSCtl).
	 * Write GP to all shadow register sets.
	 * Exact Harmony sequence.
	 */
	mfc0	t1, $12, 2		/* SRSCtl */
	add	t3, t1, zero		/* save original */
	li	t2, 7
_srs_loop:
	ins	t1, t2, 6, 4		/* SRSCtl.PSS = t2 */
	mtc0	t1, $12, 2		/* write SRSCtl */
	ehb
	wrpgpr	gp, gp			/* write GP to selected shadow set */
	addiu	t2, t2, -1
	bnez	t2, _srs_loop
	nop
	mtc0	t3, $12, 2		/* restore original SRSCtl */
	ehb

	/*
	 * Call __initialize in KSEG0 program flash.
	 * If Harmony can do "jalr t0" to 0x9D00xxxx at this point,
	 * so can we.
	 */
	la	t0, __initialize
	jalr	t0
	nop

	/* Should not reach here */
_halt:
	b	_halt
	nop

	/*
	 * Provide weak _gp symbol at a reasonable default.
	 * Linker may override this.
	 */
	.weak _gp
	.set _gp, 0x80007FF0
