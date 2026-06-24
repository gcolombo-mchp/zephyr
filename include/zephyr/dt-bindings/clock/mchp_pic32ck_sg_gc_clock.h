/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file mchp_pic32ck_sg_gc_clock.h
 * @brief List clock subsystem IDs for pic32ck_sg_gc family.
 *
 * Clock subsystem IDs. To be used in devicetree nodes, and as argument for clock API.
 */

#ifndef INCLUDE_ZEPHYR_DT_BINDINGS_CLOCK_MCHP_PIC32CK_SG_GC_CLOCK_H_
#define INCLUDE_ZEPHYR_DT_BINDINGS_CLOCK_MCHP_PIC32CK_SG_GC_CLOCK_H_

/**
 * @brief Derive a 32-bit clock subsystem identifier.
 *
 * Encodes the clock subsystem type, MCLK bus information,
 * GCLK peripheral channel, and instance number into a
 * single 32-bit identifier.
 *
 * Bit field layout:
 * - 00..07 (8 bits): inst
 *
 * - 08..13 (6 bits): gclkperiph
 * (values from 0 to 63)
 *
 * - 14..19 (6 bits): mclkmaskbit
 * (values from 0 to 31)
 *
 * - 20..25 (6 bits): mclkmaskreg
 * following values
 * MCLKMSK0 (0)
 * MCLKMSK1 (1)
 * MCLKMSK2 (2)
 * MCLKMSK3 (3)
 *
 * - 26..31 (6 bits): type
 * following values
 * SUBSYS_TYPE_GCLKGEN    (6)
 * SUBSYS_TYPE_GCLKPERIPH (7)
 * SUBSYS_TYPE_MCLKPERIPH (9)
 *
 * @param type clock subsystem type
 * @param mclkmaskreg select from the CLKMSK registers
 * @param mclkmaskbit select the module connected to bus (0 to 31)
 * @param gclkperiph gclk peripheral channel number m in PCHTRLm (0 to 63)
 * @param inst instance number of the specified clock type
 *
 * @return Encoded clock subsystem identifier
 */
#define MCHP_CLOCK_DERIVE_ID(type, mclkmaskreg, mclkmaskbit, gclkperiph, inst)                     \
	(((type) << 26) | ((mclkmaskreg) << 20) | ((mclkmaskbit) << 14) | ((gclkperiph) << 8) |    \
	 inst)

/**
 * @name GCLKGEN_TYPE Clock IDs
 * @{
 */
/** @brief Generic Clock Generator 0 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN0  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 0)
/** @brief Generic Clock Generator 1 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN1  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 1)
/** @brief Generic Clock Generator 2 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN2  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 2)
/** @brief Generic Clock Generator 3 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN3  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 3)
/** @brief Generic Clock Generator 4 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN4  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 4)
/** @brief Generic Clock Generator 5 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN5  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 5)
/** @brief Generic Clock Generator 6 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN6  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 6)
/** @brief Generic Clock Generator 7 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN7  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 7)
/** @brief Generic Clock Generator 8 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN8  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 8)
/** @brief Generic Clock Generator 9 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN9  MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 9)
/** @brief Generic Clock Generator 10 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN10 MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 10)
/** @brief Generic Clock Generator 11 ID. */
#define CLOCK_MCHP_GCLKGEN_ID_GEN11 MCHP_CLOCK_DERIVE_ID(6, 0x3f, 0x3f, 0x3f, 11)
/** @brief Maximum index for Generic Clock Generator IDs. */
#define CLOCK_MCHP_GCLKGEN_ID_MAX   (11)
/** @} */

/**
 * @name GCLKPERIPH_TYPE Clock IDs
 * @{
 */
/** @brief GCLK Peripheral ID: SERCOM slow clock (shared ch18 for SERCOM0-3). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM0_SLOW MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 18, 0)
/** @brief GCLK Peripheral ID: SERCOM1 Slow clock (shared ch18). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM1_SLOW MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 18, 1)
/** @brief GCLK Peripheral ID: SERCOM2 Slow clock (shared ch18). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM2_SLOW MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 18, 2)
/** @brief GCLK Peripheral ID: SERCOM3 Slow clock (shared ch18). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM3_SLOW MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 18, 3)
/** @brief GCLK Peripheral ID: SERCOM4 Slow clock (shared ch18). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM4_SLOW MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 18, 4)
/** @brief GCLK Peripheral ID: SERCOM5 Slow clock (shared ch18). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM5_SLOW MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 18, 5)
/** @brief GCLK Peripheral ID: SERCOM6 Slow clock (shared ch18). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM6_SLOW MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 18, 6)
/** @brief GCLK Peripheral ID: SERCOM7 Slow clock (shared ch18). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM7_SLOW MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 18, 7)
/** @brief GCLK Peripheral ID: SERCOM0 Core clock (ch19). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM0_CORE MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 19, 8)
/** @brief GCLK Peripheral ID: SERCOM1 Core clock (ch20). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM1_CORE MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 20, 9)
/** @brief GCLK Peripheral ID: SERCOM2 Core clock (ch21). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM2_CORE MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 21, 10)
/** @brief GCLK Peripheral ID: SERCOM3 Core clock (ch22). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM3_CORE MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 22, 11)
/** @brief GCLK Peripheral ID: SERCOM4 Core clock (ch25). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM4_CORE MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 25, 12)
/** @brief GCLK Peripheral ID: SERCOM5 Core clock (ch26). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM5_CORE MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 26, 13)
/** @brief GCLK Peripheral ID: SERCOM6 Core clock (ch27). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM6_CORE MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 27, 14)
/** @brief GCLK Peripheral ID: SERCOM7 Core clock (ch28). */
#define CLOCK_MCHP_GCLKPERIPH_ID_SERCOM7_CORE MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 28, 15)
/** @brief GCLK Peripheral ID: EIC (ch5). */
#define CLOCK_MCHP_GCLKPERIPH_ID_EIC         MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 5, 16)
/** @brief GCLK Peripheral ID: ETH TX (ch41). */
#define CLOCK_MCHP_GCLKPERIPH_ID_ETH_TX      MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 41, 17)
/** @brief GCLK Peripheral ID: ETH TSU (ch42). */
#define CLOCK_MCHP_GCLKPERIPH_ID_ETH_TSU     MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 42, 18)
/** @brief GCLK Peripheral ID: USB (ch46). */
#define CLOCK_MCHP_GCLKPERIPH_ID_USB         MCHP_CLOCK_DERIVE_ID(7, 0x3f, 0x3f, 46, 19)
/** @brief Maximum index for GCLK Peripheral IDs. */
#define CLOCK_MCHP_GCLKPERIPH_ID_MAX          (19)
/** @} */

/**
 * @name MCLKPERIPH_TYPE Clock IDs
 * @{
 */
/** @brief MCLK Peripheral ID: SERCOM0 (MCLK_ID=71, reg=2, bit=7). */
#define CLOCK_MCHP_MCLKPERIPH_ID_SERCOM0 MCHP_CLOCK_DERIVE_ID(9, 2, 7, 0x3f, 0)
/** @brief MCLK Peripheral ID: SERCOM1 (MCLK_ID=72, reg=2, bit=8). */
#define CLOCK_MCHP_MCLKPERIPH_ID_SERCOM1 MCHP_CLOCK_DERIVE_ID(9, 2, 8, 0x3f, 1)
/** @brief MCLK Peripheral ID: SERCOM2 (MCLK_ID=73, reg=2, bit=9). */
#define CLOCK_MCHP_MCLKPERIPH_ID_SERCOM2 MCHP_CLOCK_DERIVE_ID(9, 2, 9, 0x3f, 2)
/** @brief MCLK Peripheral ID: SERCOM3 (MCLK_ID=74, reg=2, bit=10). */
#define CLOCK_MCHP_MCLKPERIPH_ID_SERCOM3 MCHP_CLOCK_DERIVE_ID(9, 2, 10, 0x3f, 3)
/** @brief MCLK Peripheral ID: SERCOM4 (MCLK_ID=96, reg=3, bit=0). */
#define CLOCK_MCHP_MCLKPERIPH_ID_SERCOM4 MCHP_CLOCK_DERIVE_ID(9, 3, 0, 0x3f, 4)
/** @brief MCLK Peripheral ID: SERCOM5 (MCLK_ID=97, reg=3, bit=1). */
#define CLOCK_MCHP_MCLKPERIPH_ID_SERCOM5 MCHP_CLOCK_DERIVE_ID(9, 3, 1, 0x3f, 5)
/** @brief MCLK Peripheral ID: SERCOM6 (MCLK_ID=98, reg=3, bit=2). */
#define CLOCK_MCHP_MCLKPERIPH_ID_SERCOM6 MCHP_CLOCK_DERIVE_ID(9, 3, 2, 0x3f, 6)
/** @brief MCLK Peripheral ID: SERCOM7 (MCLK_ID=99, reg=3, bit=3). */
#define CLOCK_MCHP_MCLKPERIPH_ID_SERCOM7 MCHP_CLOCK_DERIVE_ID(9, 3, 3, 0x3f, 7)
/** @brief MCLK Peripheral ID: EIC APB clock (MCLK_ID=45, reg=1, bit=13). */
#define CLOCK_MCHP_MCLKPERIPH_ID_EIC     MCHP_CLOCK_DERIVE_ID(9, 1, 13, 0x3f, 8)
/** @brief MCLK Peripheral ID: ETH AHB clock (MCLK_ID=15, reg=0, bit=15). */
#define CLOCK_MCHP_MCLKPERIPH_ID_ETH_AHB MCHP_CLOCK_DERIVE_ID(9, 0, 15, 0x3f, 9)
/** @brief MCLK Peripheral ID: ETH APB clock (MCLK_ID=111, reg=3, bit=15). */
#define CLOCK_MCHP_MCLKPERIPH_ID_ETH_APB MCHP_CLOCK_DERIVE_ID(9, 3, 15, 0x3f, 10)
/** @brief MCLK Peripheral ID: USB AHB clock (MCLK_ID=19, reg=0, bit=19). */
#define CLOCK_MCHP_MCLKPERIPH_ID_USB_AHB MCHP_CLOCK_DERIVE_ID(9, 0, 19, 0x3f, 11)
/** @brief MCLK Peripheral ID: USB APB clock (MCLK_ID=113, reg=3, bit=17). */
#define CLOCK_MCHP_MCLKPERIPH_ID_USB_APB MCHP_CLOCK_DERIVE_ID(9, 3, 17, 0x3f, 12)
/** @brief MCLK Peripheral ID: TRNG APB clock (MCLK_ID=112, reg=3, bit=16). */
#define CLOCK_MCHP_MCLKPERIPH_ID_TRNG   MCHP_CLOCK_DERIVE_ID(9, 3, 16, 0x3f, 13)
/** @brief Maximum index for MCLK Peripheral IDs. */
#define CLOCK_MCHP_MCLKPERIPH_ID_MAX     (13)
/** @} */

#endif /* INCLUDE_ZEPHYR_DT_BINDINGS_CLOCK_MCHP_PIC32CK_SG_GC_CLOCK_H_ */
