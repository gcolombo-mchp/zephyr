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
 * PIC32MZ EF Starter Kit (DM320007) - Device Configuration Bits.
 *
 * Board hardware:
 *   - 24 MHz external clock oscillator (EC mode) on OSC1
 *   - System PLL: 24/3 * 50 / 2 = 200 MHz SYSCLK
 *   - USB PLL input: 24 MHz (UPLLFSEL = 1)
 *
 * ALL 4 DEVCFG words must be present for correct programming.
 */

/* DEVCFG3 */
#pragma config USERID = 0xFFFF
#pragma config FMIIEN = ON
#pragma config FETHIO = ON
#pragma config PGL1WAY = ON
#pragma config PMDL1WAY = ON
#pragma config IOL1WAY = ON
#pragma config FUSBIDIO = ON

/* DEVCFG2: Clock PLL from 24 MHz EC oscillator → 200 MHz */
#pragma config FPLLIDIV = DIV_3
#pragma config FPLLRNG = RANGE_5_10_MHZ
#pragma config FPLLICLK = PLL_POSC
#pragma config FPLLMULT = MUL_50
#pragma config FPLLODIV = DIV_2
#pragma config UPLLFSEL = FREQ_24MHZ

/* DEVCFG1: SPLL oscillator, WDT and DMT disabled */
#pragma config FNOSC = SPLL
#pragma config DMTINTV = WIN_127_128
#pragma config FSOSCEN = OFF
#pragma config IESO = OFF
#pragma config POSCMOD = EC
#pragma config OSCIOFNC = OFF
#pragma config FCKSM = CSECME
#pragma config WDTPS = PS1048576
#pragma config WDTSPGM = STOP
#pragma config FWDTEN = OFF
#pragma config WINDIS = NORMAL
#pragma config FWDTWINSZ = WINSZ_25
#pragma config DMTCNT = DMT31
#pragma config FDMTEN = OFF

/* DEVCFG0: Debug and JTAG */
#pragma config DEBUG = OFF
#pragma config JTAGEN = OFF
#pragma config ICESEL = ICS_PGx2
#pragma config TRCEN = OFF
#pragma config BOOTISA = MIPS32
#pragma config FECCCON = OFF_UNLOCKED
#pragma config FSLEEP = OFF
#pragma config EJTAGBEN = NORMAL
