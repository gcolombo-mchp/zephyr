/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file clock_control_mchp_pic32ck_sg_gc.c
 * @brief Clock control driver for Microchip PIC32CK SG/GC family.
 *
 * Minimal clock driver supporting MCLKPERIPH and GCLKPERIPH clock enable/disable.
 * The PIC32CK SG/GC uses MCLK CLKMSK[n] registers at offset 0x3C from MCLK base
 * and GCLK PCHCTRL registers at offset 0x80 from GCLK base.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#define DT_DRV_COMPAT microchip_pic32ck_sg_gc_clock

LOG_MODULE_REGISTER(clock_mchp_pic32ck_sg_gc, CONFIG_CLOCK_CONTROL_LOG_LEVEL);

/* Clock subsystem Types */
#define SUBSYS_TYPE_GCLKGEN    (6)
#define SUBSYS_TYPE_GCLKPERIPH (7)
#define SUBSYS_TYPE_MCLKPERIPH (9)

/* MCLK register offsets */
#define MCLK_CLKMSK_OFFSET 0x3C  /* Offset of CLKMSK[0] from MCLK base */

/* GCLK register offsets */
#define GCLK_PCHCTRL_OFFSET 0x80 /* Offset of PCHCTRL[0] from GCLK base */
#define GCLK_GENCTRL_OFFSET 0x20 /* Offset of GENCTRL[0] from GCLK base */

/* GCLK PCHCTRL bit definitions (use HAL if available, else define locally) */
#ifndef GCLK_PCHCTRL_GEN_Pos
#define GCLK_PCHCTRL_GEN_Pos  0
#endif
#ifndef GCLK_PCHCTRL_GEN_Msk
#define GCLK_PCHCTRL_GEN_Msk  (0xFu << GCLK_PCHCTRL_GEN_Pos)
#endif
#ifndef GCLK_PCHCTRL_CHEN_Pos
#define GCLK_PCHCTRL_CHEN_Pos 6
#endif
#ifndef GCLK_PCHCTRL_CHEN_Msk
#define GCLK_PCHCTRL_CHEN_Msk (1u << GCLK_PCHCTRL_CHEN_Pos)
#endif

/* GCLK GENCTRL bit definitions */
#ifndef GCLK_GENCTRL_GENEN_Pos
#define GCLK_GENCTRL_GENEN_Pos 8
#endif
#ifndef GCLK_GENCTRL_GENEN_Msk
#define GCLK_GENCTRL_GENEN_Msk (1u << GCLK_GENCTRL_GENEN_Pos)
#endif

/* mclkmaskreg / mclkmaskbit Not Applicable */
#define FIELD_NA (0x3f)

/* Maximum CLKMSK register index */
#define MMASKREG_MAX (3)

/* Maximum bit position in a CLKMSK register */
#define BIT_MASK_MAX (31)

/* Maximum PCHCTRL channel */
#define GCLK_PCHCTRL_MAX (63)

/*
 * Clock subsystem definition.
 *
 * Structure which can be used as a sys argument in the clock_control API.
 * Encode clock type, mclk bus, mclk mask bit, gclk pch and instance number.
 *
 * - 00..07 (8 bits): inst
 * - 08..13 (6 bits): gclkperiph
 * - 14..19 (6 bits): mclkmaskbit
 * - 20..25 (6 bits): mclkmaskreg
 * - 26..31 (6 bits): type
 */
union clock_mchp_subsys {
	uint32_t val;
	struct {
		uint32_t inst: 8;
		uint32_t gclkperiph: 6;
		uint32_t mclkmaskbit: 6;
		uint32_t mclkmaskreg: 6;
		uint32_t type: 6;
	} bits;
};

/* Clock driver configuration structure */
struct clock_mchp_pic32ck_config {
	uint8_t *mclk_base;  /* MCLK base address */
	uint8_t *gclk_base;  /* GCLK base address */
};

/**
 * @brief Get pointer to MCLK CLKMSK[reg] register.
 */
static volatile uint32_t *mclk_clkmsk_reg(const struct clock_mchp_pic32ck_config *config,
					   uint8_t reg)
{
	return (volatile uint32_t *)(config->mclk_base + MCLK_CLKMSK_OFFSET + (reg * 4));
}

/**
 * @brief Get pointer to GCLK PCHCTRL[ch] register.
 */
static volatile uint32_t *gclk_pchctrl_reg(const struct clock_mchp_pic32ck_config *config,
					    uint8_t ch)
{
	return (volatile uint32_t *)(config->gclk_base + GCLK_PCHCTRL_OFFSET + (ch * 4));
}

/**
 * @brief Get pointer to GCLK GENCTRL[gen] register.
 */
static volatile uint32_t *gclk_genctrl_reg(const struct clock_mchp_pic32ck_config *config,
					    uint8_t gen)
{
	return (volatile uint32_t *)(config->gclk_base + GCLK_GENCTRL_OFFSET + (gen * 4));
}

/**
 * @brief Validate the clock subsystem ID.
 */
static int clock_check_subsys(union clock_mchp_subsys subsys)
{
	switch (subsys.bits.type) {
	case SUBSYS_TYPE_GCLKGEN:
		/* Only inst is relevant */
		break;

	case SUBSYS_TYPE_GCLKPERIPH:
		if (subsys.bits.gclkperiph > GCLK_PCHCTRL_MAX) {
			return -EINVAL;
		}
		break;

	case SUBSYS_TYPE_MCLKPERIPH:
		if (subsys.bits.mclkmaskreg > MMASKREG_MAX ||
		    subsys.bits.mclkmaskbit > BIT_MASK_MAX) {
			return -EINVAL;
		}
		break;

	default:
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief Enable a clock subsystem.
 */
static int clock_mchp_on(const struct device *dev, clock_control_subsys_t sys)
{
	const struct clock_mchp_pic32ck_config *config = dev->config;
	union clock_mchp_subsys subsys = {.val = (uint32_t)sys};
	volatile uint32_t *reg;

	if (clock_check_subsys(subsys) != 0) {
		return -ENOTSUP;
	}

	switch (subsys.bits.type) {
	case SUBSYS_TYPE_GCLKGEN:
		/* Generator 0 is always on; others: set GENEN bit */
		if (subsys.bits.inst != 0) {
			reg = gclk_genctrl_reg(config, subsys.bits.inst);
			*reg |= GCLK_GENCTRL_GENEN_Msk;
		}
		break;

	case SUBSYS_TYPE_GCLKPERIPH:
		/* Enable peripheral channel: route from generator 0 and set CHEN */
		reg = gclk_pchctrl_reg(config, subsys.bits.gclkperiph);
		*reg = (*reg & ~GCLK_PCHCTRL_GEN_Msk) | GCLK_PCHCTRL_CHEN_Msk;
		break;

	case SUBSYS_TYPE_MCLKPERIPH:
		/* Set bit in CLKMSK register */
		reg = mclk_clkmsk_reg(config, subsys.bits.mclkmaskreg);
		*reg |= BIT(subsys.bits.mclkmaskbit);
		break;

	default:
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief Disable a clock subsystem.
 */
static int clock_mchp_off(const struct device *dev, clock_control_subsys_t sys)
{
	const struct clock_mchp_pic32ck_config *config = dev->config;
	union clock_mchp_subsys subsys = {.val = (uint32_t)sys};
	volatile uint32_t *reg;

	if (clock_check_subsys(subsys) != 0) {
		return -ENOTSUP;
	}

	switch (subsys.bits.type) {
	case SUBSYS_TYPE_GCLKGEN:
		/* Generator 0 is always on and cannot be disabled */
		if (subsys.bits.inst == 0) {
			return -ENOTSUP;
		}
		reg = gclk_genctrl_reg(config, subsys.bits.inst);
		*reg &= ~GCLK_GENCTRL_GENEN_Msk;
		break;

	case SUBSYS_TYPE_GCLKPERIPH:
		/* Clear CHEN in PCHCTRL */
		reg = gclk_pchctrl_reg(config, subsys.bits.gclkperiph);
		*reg &= ~GCLK_PCHCTRL_CHEN_Msk;
		break;

	case SUBSYS_TYPE_MCLKPERIPH:
		/* Clear bit in CLKMSK register */
		reg = mclk_clkmsk_reg(config, subsys.bits.mclkmaskreg);
		*reg &= ~BIT(subsys.bits.mclkmaskbit);
		break;

	default:
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief Get status of a clock subsystem.
 */
static enum clock_control_status clock_mchp_get_status(const struct device *dev,
						       clock_control_subsys_t sys)
{
	const struct clock_mchp_pic32ck_config *config = dev->config;
	union clock_mchp_subsys subsys = {.val = (uint32_t)sys};
	volatile uint32_t *reg;

	if (clock_check_subsys(subsys) != 0) {
		return CLOCK_CONTROL_STATUS_UNKNOWN;
	}

	switch (subsys.bits.type) {
	case SUBSYS_TYPE_GCLKGEN:
		if (subsys.bits.inst == 0) {
			/* GEN0 is always on */
			return CLOCK_CONTROL_STATUS_ON;
		}
		reg = gclk_genctrl_reg(config, subsys.bits.inst);
		return (*reg & GCLK_GENCTRL_GENEN_Msk) != 0
			       ? CLOCK_CONTROL_STATUS_ON
			       : CLOCK_CONTROL_STATUS_OFF;

	case SUBSYS_TYPE_GCLKPERIPH:
		reg = gclk_pchctrl_reg(config, subsys.bits.gclkperiph);
		return (*reg & GCLK_PCHCTRL_CHEN_Msk) != 0
			       ? CLOCK_CONTROL_STATUS_ON
			       : CLOCK_CONTROL_STATUS_OFF;

	case SUBSYS_TYPE_MCLKPERIPH:
		reg = mclk_clkmsk_reg(config, subsys.bits.mclkmaskreg);
		return (*reg & BIT(subsys.bits.mclkmaskbit)) != 0
			       ? CLOCK_CONTROL_STATUS_ON
			       : CLOCK_CONTROL_STATUS_OFF;

	default:
		return CLOCK_CONTROL_STATUS_UNKNOWN;
	}
}

/**
 * @brief Get the rate of a clock subsystem.
 *
 * After reset, GCLK_GEN0 runs at 48 MHz (DFLL48M, undivided).
 * For GCLKPERIPH subsystems routed through GEN0, return 48 MHz.
 * For MCLKPERIPH, the bus clock equals the CPU clock (48 MHz undivided).
 */
static int clock_mchp_get_rate(const struct device *dev, clock_control_subsys_t sys,
			       uint32_t *rate)
{
	const struct clock_mchp_pic32ck_config *config = dev->config;
	union clock_mchp_subsys subsys = {.val = (uint32_t)sys};

	if (rate == NULL) {
		return -EINVAL;
	}

	switch (subsys.bits.type) {
	case SUBSYS_TYPE_GCLKGEN:
		/* GEN0 = DFLL48M / 1 = 48 MHz after reset */
		if (subsys.bits.inst == 0) {
			*rate = 120000000U;
		} else {
			/* Read actual divider from GENCTRL register */
			volatile uint32_t *reg = gclk_genctrl_reg(config, subsys.bits.inst);
			uint32_t div = (*reg >> 16) & 0xFFFF;

			if (div <= 1) {
				div = 1;
			}
			/* Assume all other generators also source from GEN0/DFLL48M */
			*rate = 48000000U / div;
		}
		return 0;

	case SUBSYS_TYPE_GCLKPERIPH: {
		/* Read which generator feeds this peripheral channel */
		volatile uint32_t *reg = gclk_pchctrl_reg(config, subsys.bits.gclkperiph);
		uint32_t gen = *reg & GCLK_PCHCTRL_GEN_Msk;

		/* For GEN0 (default after reset): 48 MHz */
		if (gen == 0) {
			*rate = 120000000U;
		} else {
			/* Read generator's divider */
			volatile uint32_t *genreg = gclk_genctrl_reg(config, gen);
			uint32_t div = (*genreg >> 16) & 0xFFFF;

			if (div <= 1) {
				div = 1;
			}
			*rate = 48000000U / div;
		}
		return 0;
	}

	case SUBSYS_TYPE_MCLKPERIPH:
		/* APB bus clock = CPU clock = 48 MHz (undivided after reset) */
		*rate = 120000000U;
		return 0;

	default:
		return -ENOTSUP;
	}
}

/**
 * @brief Initialize the clock driver.
 *
 * Configure PLL0 at 120MHz and switch GCLK0 to PLL0 output.
 * This matches the Harmony clock configuration required for ETH.
 * PLL0: DFLL48M / 12 * 240 / 8 = 120MHz
 */
static int clock_mchp_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* Enable additional voltage regulator for PLL */
	volatile uint32_t *supc_vregctrl = (volatile uint32_t *)(0x44008000 + 0x1C);
	volatile uint32_t *supc_status = (volatile uint32_t *)(0x44008000 + 0x0C);

	*supc_vregctrl |= (1U << 16); /* AVREGEN */
	while ((*supc_status & (1U << 8)) == 0) { /* ADDVREGRDY0 */
	}

	/* Configure PLL0: 48MHz/12*240/8 = 120MHz */
	volatile uint32_t *oscctrl_pll0refdiv = (volatile uint32_t *)(0x4400C000 + 0x48);
	volatile uint32_t *oscctrl_pll0fbdiv = (volatile uint32_t *)(0x4400C000 + 0x44);
	volatile uint32_t *oscctrl_fracdiv0 = (volatile uint32_t *)(0x4400C000 + 0x6C);
	volatile uint32_t *oscctrl_syncbusy = (volatile uint32_t *)(0x4400C000 + 0x78);
	volatile uint32_t *oscctrl_pll0postdiva = (volatile uint32_t *)(0x4400C000 + 0x4C);
	volatile uint32_t *oscctrl_pll0ctrl = (volatile uint32_t *)(0x4400C000 + 0x40);
	volatile uint32_t *oscctrl_status = (volatile uint32_t *)(0x4400C000 + 0x10);

	*oscctrl_pll0refdiv = 12U;
	*oscctrl_pll0fbdiv = 240U;
	*oscctrl_fracdiv0 = 0;
	while (*oscctrl_syncbusy & (1U << 6)) {
	}
	*oscctrl_pll0postdiva = (1U << 7) | 8U; /* OUTEN0 | POSTDIV0=8 */
	*oscctrl_pll0ctrl |= (1U << 11) | (2U << 8) | (1U << 1); /* BWSEL=1 | REFSEL=2(DFLL) | ENABLE */
	while ((*oscctrl_status & (1U << 24)) == 0) { /* PLL0LOCK */
	}

	/* Switch GCLK0 to PLL0_0 (120MHz) */
	volatile uint32_t *gclk_genctrl0 = (volatile uint32_t *)(0x44010000 + 0x20);
	volatile uint32_t *gclk_syncbusy = (volatile uint32_t *)(0x44010000 + 0x04);

	*gclk_genctrl0 = (1U << 8) | (6U << 0); /* GENEN | SRC=PLL0_0 */
	while (*gclk_syncbusy & (1U << 2)) { /* GENCTRL0 */
	}

	/* Configure GCLK GEN1 from DFLL48M (48MHz) for USB */
	volatile uint32_t *gclk_genctrl1 = (volatile uint32_t *)(0x44010000 + 0x20 + 4);
	*gclk_genctrl1 = (1U << 8) | (5U << 0); /* GENEN | SRC=5(DFLL48M) */
	while (*gclk_syncbusy & (1U << 3)) { /* GENCTRL1 */
	}

	LOG_DBG("PIC32CK clock: GEN0=120MHz(PLL0), GEN1=48MHz(DFLL)");
	return 0;
}

static DEVICE_API(clock_control, clock_mchp_pic32ck_driver_api) = {
	.on = clock_mchp_on,
	.off = clock_mchp_off,
	.get_status = clock_mchp_get_status,
	.get_rate = clock_mchp_get_rate,
};

#define CLOCK_NODE DT_NODELABEL(clock)

static const struct clock_mchp_pic32ck_config clock_mchp_pic32ck_config_0 = {
	.mclk_base = (uint8_t *)DT_REG_ADDR_BY_NAME(CLOCK_NODE, mclk),
	.gclk_base = (uint8_t *)DT_REG_ADDR_BY_NAME(CLOCK_NODE, gclk),
};

#define CLOCK_MCHP_PIC32CK_DEVICE_INIT(n)                                                         \
	DEVICE_DT_INST_DEFINE(n, clock_mchp_init, NULL, NULL,                                      \
			      &clock_mchp_pic32ck_config_0, PRE_KERNEL_1,                          \
			      CONFIG_CLOCK_CONTROL_INIT_PRIORITY,                                   \
			      &clock_mchp_pic32ck_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CLOCK_MCHP_PIC32CK_DEVICE_INIT)
