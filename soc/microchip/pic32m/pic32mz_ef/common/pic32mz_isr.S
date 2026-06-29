/**********************************************************************************************
 * © 2026 Microchip Technology Inc. and its subsidiares. All rights reserved.
 * This software includes AI generated code created using significant prompting. This
 * software is provided AS IS; you are Responsible for reviewing, testing, and validating for
 * your application.
 **********************************************************************************************/
/*
 * Copyright (c) 2026 Microchip Technology Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * PIC32MZ exception and interrupt handlers for Zephyr.
 *
 * On PIC32MZ with Cause.IV=1 and INTCON.MVEC=0:
 *   - General exceptions (syscall, faults) → EBASE+0x180
 *   - Interrupts                           → EBASE+0x200
 *
 * Two separate entry points:
 *   _pic32mz_general_exception (EBASE+0x180): handles syscall + faults
 *   _pic32mz_interrupt         (EBASE+0x200): handles all EVIC interrupts
 *
 * Both share _reschedule and _no_reschedule for context switch.
 */

#include <zephyr/toolchain.h>
#include <zephyr/kernel_structs.h>
#include <offsets_short.h>
#include <zephyr/arch/cpu.h>

/* CP0 register numbers */
#define CP0_BADVADDR	$8
#define CP0_COUNT	$9
#define CP0_STATUS	$12
#define CP0_CAUSE	$13
#define CP0_EPC		$14

/* Cause register fields */
#define CAUSE_EXCCODE_MASK  0x7C
#define CAUSE_EXCCODE_SHIFT 2
#define EXCCODE_SYSCALL     8

/* ESF (Exception Stack Frame) field offsets */
#define ESF_RA      __struct_arch_esf_ra_OFFSET
#define ESF_GP      __struct_arch_esf_gp_OFFSET
#define ESF_T0      __struct_arch_esf_t0_OFFSET
#define ESF_T1      __struct_arch_esf_t1_OFFSET
#define ESF_T2      __struct_arch_esf_t2_OFFSET
#define ESF_T3      __struct_arch_esf_t3_OFFSET
#define ESF_T4      __struct_arch_esf_t4_OFFSET
#define ESF_T5      __struct_arch_esf_t5_OFFSET
#define ESF_T6      __struct_arch_esf_t6_OFFSET
#define ESF_T7      __struct_arch_esf_t7_OFFSET
#define ESF_T8      __struct_arch_esf_t8_OFFSET
#define ESF_T9      __struct_arch_esf_t9_OFFSET
#define ESF_A0      __struct_arch_esf_a0_OFFSET
#define ESF_A1      __struct_arch_esf_a1_OFFSET
#define ESF_A2      __struct_arch_esf_a2_OFFSET
#define ESF_A3      __struct_arch_esf_a3_OFFSET
#define ESF_V0      __struct_arch_esf_v0_OFFSET
#define ESF_V1      __struct_arch_esf_v1_OFFSET
#define ESF_AT      __struct_arch_esf_at_OFFSET
#define ESF_EPC     __struct_arch_esf_epc_OFFSET
#define ESF_BADVADDR __struct_arch_esf_badvaddr_OFFSET
#define ESF_HI      __struct_arch_esf_hi_OFFSET
#define ESF_LO      __struct_arch_esf_lo_OFFSET
#define ESF_STATUS  __struct_arch_esf_status_OFFSET
#define ESF_CAUSE   __struct_arch_esf_cause_OFFSET
#define ESF_SIZEOF  __struct_arch_esf_SIZEOF

.extern _kernel
.extern _sw_isr_table
.extern z_mips_fatal_error
.extern pic32mz_evic_dispatch
.extern _errno_neg_eagain

.global _pic32mz_general_exception
.global _pic32mz_interrupt

.set noreorder
.set noat

/*============================================================================
 * Macro: save all caller-saved registers + CP0 to ESF on stack
 *============================================================================*/
.macro SAVE_CONTEXT
	addiu	$29, $29, -ESF_SIZEOF

	sw	$31, ESF_RA($29)
	sw	$28, ESF_GP($29)
	sw	$1,  ESF_AT($29)
	sw	$8,  ESF_T0($29)
	sw	$9,  ESF_T1($29)
	sw	$10, ESF_T2($29)
	sw	$11, ESF_T3($29)
	sw	$12, ESF_T4($29)
	sw	$13, ESF_T5($29)
	sw	$14, ESF_T6($29)
	sw	$15, ESF_T7($29)
	sw	$24, ESF_T8($29)
	sw	$25, ESF_T9($29)
	sw	$4,  ESF_A0($29)
	sw	$5,  ESF_A1($29)
	sw	$6,  ESF_A2($29)
	sw	$7,  ESF_A3($29)
	sw	$2,  ESF_V0($29)
	sw	$3,  ESF_V1($29)

	mfhi	$8
	mflo	$9
	sw	$8,  ESF_HI($29)
	sw	$9,  ESF_LO($29)

	mfc0	$8,  CP0_EPC
	sw	$8,  ESF_EPC($29)
	mfc0	$9,  CP0_BADVADDR
	sw	$9,  ESF_BADVADDR($29)
	mfc0	$8,  CP0_STATUS
	sw	$8,  ESF_STATUS($29)
	mfc0	$9,  CP0_CAUSE
	sw	$9,  ESF_CAUSE($29)
.endm

/*============================================================================
 * GENERAL EXCEPTION HANDLER (EBASE+0x180)
 * Handles: syscall (context switch) and faults.
 * Called via gen_handler.S trampoline.
 *============================================================================*/
.section .exception.other, "ax", @progbits

_pic32mz_general_exception:
	SAVE_CONTEXT

	/* Extract ExcCode from Cause */
	andi	$26, $9, CAUSE_EXCCODE_MASK
	srl	$26, $26, CAUSE_EXCCODE_SHIFT

	/* ExcCode == 8 (SYSCALL)? */
	li	$27, EXCCODE_SYSCALL
	beq	$26, $27, _is_syscall
	nop

	/* Other exception → fatal */
	move	$4, $29
	jal	z_mips_fatal_error
	nop
	eret

_is_syscall:
	/* Increment EPC past syscall instruction */
	lw	$26, ESF_EPC($29)
	addiu	$26, $26, 4
	sw	$26, ESF_EPC($29)

	j	_reschedule
	nop

/*============================================================================
 * INTERRUPT HANDLER (EBASE+0x200)
 * Handles: all EVIC interrupts via INTSTAT dispatch.
 * Called via interrupt vector trampoline at .vectors section.
 *============================================================================*/
.section .exception.interrupt, "ax", @progbits

_pic32mz_interrupt:
	SAVE_CONTEXT

	/* Switch to interrupt stack */
	move	$26, $29

	la	$27, _kernel
	lw	$29, _kernel_offset_to_irq_stack($27)

	/* Push thread stack pointer on interrupt stack */
	addiu	$29, $29, -16
	sw	$26, 0($29)

	/* Increment nested counter */
	lw	$8, _kernel_offset_to_nested($27)
	addiu	$8, $8, 1
	sw	$8, _kernel_offset_to_nested($27)

	/* Call C dispatch: reads INTSTAT, calls ISR, clears flag */
	jal	pic32mz_evic_dispatch
	nop

	/* Decrement nested counter */
	la	$27, _kernel
	lw	$8, _kernel_offset_to_nested($27)
	addiu	$8, $8, -1
	sw	$8, _kernel_offset_to_nested($27)

	/* Restore thread stack pointer */
	lw	$29, 0($29)

#ifdef CONFIG_PREEMPT_ENABLED
	/* Check if reschedule needed */
	lw	$8, _kernel_offset_to_current($27)
	lw	$9, _kernel_offset_to_ready_q_cache($27)
	beq	$8, $9, _no_reschedule
	nop
	j	_reschedule
	nop
#else
	j	_no_reschedule
	nop
#endif /* CONFIG_PREEMPT_ENABLED */

/*============================================================================
 * SHARED: Context switch and return from exception.
 *============================================================================*/
.section .exception.other, "ax", @progbits

/*
 * Context switch: save current thread, load next thread.
 */
_reschedule:
	la	$8, _kernel
	lw	$9, _kernel_offset_to_current($8)
	lw	$10, _kernel_offset_to_ready_q_cache($8)
	beq	$9, $10, _no_reschedule
	nop

	/* Save callee-saved registers of current thread */
	sw	$16, _thread_offset_to_s0($9)
	sw	$17, _thread_offset_to_s1($9)
	sw	$18, _thread_offset_to_s2($9)
	sw	$19, _thread_offset_to_s3($9)
	sw	$20, _thread_offset_to_s4($9)
	sw	$21, _thread_offset_to_s5($9)
	sw	$22, _thread_offset_to_s6($9)
	sw	$23, _thread_offset_to_s7($9)
	sw	$30, _thread_offset_to_s8($9)

	/* Save stack pointer */
	sw	$29, _thread_offset_to_sp($9)

	/* Set swap return value = -EAGAIN */
	la	$11, _errno_neg_eagain
	lw	$11, 0($11)
	sw	$11, _thread_offset_to_swap_return_value($9)

	/* Load next thread */
	lw	$10, _kernel_offset_to_ready_q_cache($8)
	sw	$10, _kernel_offset_to_current($8)

	/* Restore next thread stack pointer */
	lw	$29, _thread_offset_to_sp($10)

	/* Restore next thread callee-saved registers */
	lw	$16, _thread_offset_to_s0($10)
	lw	$17, _thread_offset_to_s1($10)
	lw	$18, _thread_offset_to_s2($10)
	lw	$19, _thread_offset_to_s3($10)
	lw	$20, _thread_offset_to_s4($10)
	lw	$21, _thread_offset_to_s5($10)
	lw	$22, _thread_offset_to_s6($10)
	lw	$23, _thread_offset_to_s7($10)
	lw	$30, _thread_offset_to_s8($10)

/*
 * Return from exception: restore CP0 and caller-saved registers.
 */
_no_reschedule:
	/* Restore hi/lo */
	lw	$8,  ESF_HI($29)
	lw	$9,  ESF_LO($29)
	mthi	$8
	mtlo	$9

	/* Restore EPC and Status */
	lw	$26, ESF_EPC($29)
	mtc0	$26, CP0_EPC
	lw	$27, ESF_STATUS($29)
	mtc0	$27, CP0_STATUS
	ehb

	/* Restore caller-saved registers */
	lw	$31, ESF_RA($29)
	lw	$28, ESF_GP($29)
	lw	$1,  ESF_AT($29)
	lw	$8,  ESF_T0($29)
	lw	$9,  ESF_T1($29)
	lw	$10, ESF_T2($29)
	lw	$11, ESF_T3($29)
	lw	$12, ESF_T4($29)
	lw	$13, ESF_T5($29)
	lw	$14, ESF_T6($29)
	lw	$15, ESF_T7($29)
	lw	$24, ESF_T8($29)
	lw	$25, ESF_T9($29)
	lw	$4,  ESF_A0($29)
	lw	$5,  ESF_A1($29)
	lw	$6,  ESF_A2($29)
	lw	$7,  ESF_A3($29)
	lw	$2,  ESF_V0($29)
	lw	$3,  ESF_V1($29)

	/* Deallocate ESF */
	addiu	$29, $29, ESF_SIZEOF

	/* Return from exception */
	eret
