/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPS pin mapping macros for PIC32MZ2048EFH144.
 *
 * Encoding: PPS_PINMUX(reg_offset, value, direction)
 *   reg_offset: offset from PPS base (0xBF801400)
 *   value: value to write to the register
 *   direction: 0 = input register, 1 = output register
 *
 * Input registers (write pin code to select which pin drives a peripheral input):
 *   Register name = peripheral function + "R" (e.g., U2RXR for UART2 RX)
 *   Value = pin encoding from INPUT PIN SELECTION table in datasheet
 *
 * Output registers (write function code to select which peripheral drives a pin):
 *   Register name = "RP" + port + pin + "R" (e.g., RPG9R for port G pin 9)
 *   Value = peripheral function code from OUTPUT PIN SELECTION table
 *
 * Reference: DS60001320H Tables 12-1 and 12-2
 */

#ifndef ZEPHYR_DT_BINDINGS_PINCTRL_PIC32MZ_EFH144_PPS_H_
#define ZEPHYR_DT_BINDINGS_PINCTRL_PIC32MZ_EFH144_PPS_H_

/* Macro to build a pinmux value (DTS-compatible, no C-style casts) */
#define PPS_PINMUX(reg_offset, value, direction) \
	(((reg_offset) << 16) | ((value) << 8) | (direction))

/* Direction constants */
#define PPS_INPUT  0
#define PPS_OUTPUT 1

/*
 * ============================================================================
 * INPUT PIN SELECTION registers (offset from PPS base 0xBF801400)
 *
 * Each register selects which pin drives a specific peripheral input.
 * Offsets derived from PIC32MZ EF datasheet Table 12-1.
 * ============================================================================
 */

/* UART input registers */
#define U1RXR_OFFSET   0x0024
#define U2RXR_OFFSET   0x0034
#define U3RXR_OFFSET   0x0020
#define U4RXR_OFFSET   0x0040
#define U5RXR_OFFSET   0x0028
#define U6RXR_OFFSET   0x0050

/* SPI SDI input registers */
#define SDI1R_OFFSET   0x0010
#define SDI2R_OFFSET   0x0038
#define SDI3R_OFFSET   0x001C
#define SDI4R_OFFSET   0x0044
#define SDI5R_OFFSET   0x002C
#define SDI6R_OFFSET   0x0054

/*
 * ============================================================================
 * OUTPUT PIN SELECTION registers (offset from PPS base 0xBF801400)
 *
 * Each register selects which peripheral function drives a specific pin.
 * Offsets from PIC32MZ EF datasheet Table 12-2.
 * Base for output registers: 0xBF801500 (offset 0x100 from PPS base)
 * Spacing: 4 bytes per pin register
 * ============================================================================
 */

/* Output function codes (value written to RPnR register) */
#define PPS_OUT_NONE    0x00
#define PPS_OUT_U1TX    0x01
#define PPS_OUT_U2TX    0x02
#define PPS_OUT_U3TX    0x01
#define PPS_OUT_U4TX    0x02
#define PPS_OUT_U5TX    0x03
#define PPS_OUT_U6TX    0x04
#define PPS_OUT_SDO1    0x05
#define PPS_OUT_SDO2    0x06
#define PPS_OUT_SDO3    0x07
#define PPS_OUT_SDO4    0x08
#define PPS_OUT_SDO5    0x09
#define PPS_OUT_SDO6    0x0A
#define PPS_OUT_OC1     0x0C
#define PPS_OUT_OC2     0x0B
#define PPS_OUT_OC3     0x0B
#define PPS_OUT_C1TX    0x0F
#define PPS_OUT_C2TX    0x10
#define PPS_OUT_SS1     0x05
#define PPS_OUT_SS2     0x06

/* Example output pin register offsets (from PPS output base = PPS base + 0x100) */
#define RPA14R_OFFSET  0x0138
#define RPA15R_OFFSET  0x013C
#define RPB0R_OFFSET   0x0140
#define RPB1R_OFFSET   0x0144
#define RPB2R_OFFSET   0x0148
#define RPB3R_OFFSET   0x014C
#define RPB5R_OFFSET   0x0154
#define RPB6R_OFFSET   0x0158
#define RPB7R_OFFSET   0x015C
#define RPB8R_OFFSET   0x0160
#define RPB9R_OFFSET   0x0164
#define RPB10R_OFFSET  0x0168
#define RPB14R_OFFSET  0x0178
#define RPB15R_OFFSET  0x017C
#define RPC1R_OFFSET   0x0184
#define RPC2R_OFFSET   0x0188
#define RPC3R_OFFSET   0x018C
#define RPC4R_OFFSET   0x0190
#define RPD0R_OFFSET   0x01C0
#define RPD1R_OFFSET   0x01C4
#define RPD2R_OFFSET   0x01C8
#define RPD3R_OFFSET   0x01CC
#define RPD4R_OFFSET   0x01D0
#define RPD5R_OFFSET   0x01D4
#define RPD9R_OFFSET   0x01E4
#define RPD10R_OFFSET  0x01E8
#define RPD11R_OFFSET  0x01EC
#define RPD12R_OFFSET  0x01F0
#define RPD14R_OFFSET  0x01F8
#define RPD15R_OFFSET  0x01FC
#define RPE3R_OFFSET   0x020C
#define RPE5R_OFFSET   0x0214
#define RPE8R_OFFSET   0x0220
#define RPE9R_OFFSET   0x0224
#define RPF0R_OFFSET   0x0240
#define RPF1R_OFFSET   0x0244
#define RPF2R_OFFSET   0x0248
#define RPF3R_OFFSET   0x024C
#define RPF4R_OFFSET   0x0250
#define RPF5R_OFFSET   0x0254
#define RPF8R_OFFSET   0x0260
#define RPF12R_OFFSET  0x0270
#define RPF13R_OFFSET  0x0274
#define RPG0R_OFFSET   0x0280
#define RPG1R_OFFSET   0x0284
#define RPG6R_OFFSET   0x0298
#define RPG7R_OFFSET   0x029C
#define RPG8R_OFFSET   0x02A0
#define RPG9R_OFFSET   0x02A4

/* Input pin codes (value written to peripheral input register) */
#define PPS_IN_RPD2    0x00
#define PPS_IN_RPG8    0x01
#define PPS_IN_RPF4    0x02
#define PPS_IN_RPD10   0x03
#define PPS_IN_RPF1    0x04
#define PPS_IN_RPB9    0x05
#define PPS_IN_RPB10   0x06
#define PPS_IN_RPC14   0x07
#define PPS_IN_RPB5    0x08
#define PPS_IN_RPC1    0x0A
#define PPS_IN_RPD14   0x0B
#define PPS_IN_RPG1    0x0C
#define PPS_IN_RPA14   0x0D
#define PPS_IN_RPD6    0x0E
#define PPS_IN_RPD3    0x00
#define PPS_IN_RPG7    0x01
#define PPS_IN_RPF5    0x02
#define PPS_IN_RPD11   0x03
#define PPS_IN_RPF0    0x04
#define PPS_IN_RPB1    0x05
#define PPS_IN_RPE5    0x06
#define PPS_IN_RPC13   0x07
#define PPS_IN_RPB3    0x08
#define PPS_IN_RPC4    0x0A
#define PPS_IN_RPD15   0x0B
#define PPS_IN_RPG0    0x0C
#define PPS_IN_RPA15   0x0D
#define PPS_IN_RPD7    0x0E

/*
 * ============================================================================
 * Convenience macros for common UART pin assignments on the EF Starter Kit
 *
 * DM320007 uses UART2 on:
 *   U2TX → RPB14 (output group: RPB14R = 0x0010 = U2TX)
 *   U2RX → RPG6  (input group: U2RXR = 0x0001 = RPG6)
 * ============================================================================
 */

/* UART2 TX on RPB14: write function code 0x02 (U2TX) to RPB14R register */
#define PPS_U2TX_RPB14  PPS_PINMUX(RPB14R_OFFSET, 0x02, PPS_OUTPUT)

/* UART2 RX from RPG6: write pin code 0x01 (RPG6) to U2RXR register */
#define PPS_U2RX_RPG6   PPS_PINMUX(U2RXR_OFFSET, 0x01, PPS_INPUT)

/* Legacy convenience (alternate pin assignments) */
#define PPS_U2TX_RPG9   PPS_PINMUX(RPG9R_OFFSET, PPS_OUT_U2TX, PPS_OUTPUT)
#define PPS_U2RX_RPB0   PPS_PINMUX(U2RXR_OFFSET, 0x05, PPS_INPUT)

#endif /* ZEPHYR_DT_BINDINGS_PINCTRL_PIC32MZ_EFH144_PPS_H_ */
