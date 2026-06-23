/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Clock control driver for PIC32MZ family (MIPS microAptiv core).
 *
 * Boot behavior:
 *   The DEVCFG configuration words (set at flash programming time) configure
 *   the oscillator source (FNOSC) and PLL parameters at reset. This driver
 *   reads the current hardware state and computes clock frequencies.
 *   It does NOT reprogram the PLL at init.
 *
 * Runtime behavior:
 *   Supports switching between operating points (OPP table) via
 *   clock_control_configure(). This involves:
 *   1. SYSKEY unlock sequence
 *   2. Writing new OSCCON.NOSC and triggering OSWEN
 *   3. Waiting for switch completion
 *   4. Adjusting PBxDIV registers
 *   5. SYSKEY lock
 *
 * Reference: DS60001320H Section 8.0 "Oscillator Configuration"
 */

#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/mchp_clock_pic32mz.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT microchip_pic32mz_clock

LOG_MODULE_REGISTER(clock_pic32mz, CONFIG_CLOCK_CONTROL_LOG_LEVEL);

/* Register offsets from oscillator base (0xBF801200) */
#define OSCCON       0x0000
#define OSCTUN       0x0010
#define SPLLCON      0x0020

/* OSCCON bit fields */
#define OSCCON_OSWEN     BIT(0)
#define OSCCON_SOSCEN    BIT(1)
#define OSCCON_CF        BIT(3)
#define OSCCON_SLPEN     BIT(4)
#define OSCCON_CLKLOCK   BIT(7)
#define OSCCON_NOSC_SHIFT 8
#define OSCCON_NOSC_MASK (0x7 << OSCCON_NOSC_SHIFT)
#define OSCCON_COSC_SHIFT 12
#define OSCCON_COSC_MASK (0x7 << OSCCON_COSC_SHIFT)
#define OSCCON_FRCDIV_SHIFT 24
#define OSCCON_FRCDIV_MASK (0x7 << OSCCON_FRCDIV_SHIFT)

/* SPLLCON bit fields (from PIC32MZ EF datasheet) */
#define SPLLCON_PLLIDIV_SHIFT 8
#define SPLLCON_PLLIDIV_MASK  (0x7 << SPLLCON_PLLIDIV_SHIFT)
#define SPLLCON_PLLMULT_SHIFT 16
#define SPLLCON_PLLMULT_MASK  (0x7F << SPLLCON_PLLMULT_SHIFT)
#define SPLLCON_PLLODIV_SHIFT 24
#define SPLLCON_PLLODIV_MASK  (0x7 << SPLLCON_PLLODIV_SHIFT)
#define SPLLCON_PLLICLK       BIT(7)

/* PBxDIV register layout (offset from PB base, spacing 0x10) */
#define PBDIV_REG(n)     (((n) - 1) * 0x10)
#define PBDIV_ON         BIT(15)
#define PBDIV_PBDIV_MASK 0x7F

/* SYSKEY register (for unlock sequence) */
#define SYSKEY_ADDR 0xBF800000

/* CLR/SET/INV register offsets */
#define REG_CLR 0x04
#define REG_SET 0x08
#define REG_INV 0x0C

/* Clock source IDs (COSC/NOSC field encoding) */
#define CLKSRC_FRC      0x0
#define CLKSRC_FRCDIV   0x7
#define CLKSRC_SPLL     0x1
#define CLKSRC_POSC     0x2
#define CLKSRC_SOSC     0x4
#define CLKSRC_LPRC     0x5

/* FRC nominal frequency */
#define FRC_FREQ_HZ 8000000U

/* USB PLL - generates 48 MHz for USB HS PHY.
 * On PIC32MZ EF, USB PLL input is POSC (12 or 24 MHz) selected by
 * UPLLFSEL bit in DEVCFG2. The PLL is internal and not runtime-configurable,
 * but we must verify POSC freq matches UPLLFSEL to confirm valid 48 MHz output.
 *
 * UPLLFSEL (DEVCFG2<30>):
 *   1 = USB PLL expects 24 MHz POSC input
 *   0 = USB PLL expects 12 MHz POSC input
 *
 * If POSC frequency doesn't match UPLLFSEL setting, USB clock is invalid.
 */
#define UPLL_OUTPUT_FREQ_HZ  48000000U
#define UPLL_INPUT_24MHZ     24000000U
#define UPLL_INPUT_12MHZ     12000000U

/* DEVCFG2 register (read-only at runtime, in config space) */
#define DEVCFG2_ADDR         0xBFC0FFC4
#define DEVCFG2_UPLLFSEL     BIT(30)

/* Reference clock registers (offset from osc base) */
#define REFO1CON     0x0080
#define REFO1TRIM    0x0090
#define REFO_SPACING 0x0020
#define REFOCON_REG(n)   (REFO1CON + ((n) - 1) * REFO_SPACING)
#define REFOTRIM_REG(n)  (REFO1TRIM + ((n) - 1) * REFO_SPACING)
#define REFOCON_ON       BIT(15)
#define REFOCON_OE       BIT(12)
#define REFOCON_ROSEL_MASK  0x0F
#define REFOCON_RODIV_SHIFT 16
#define REFOCON_RODIV_MASK  (0x7FFF << REFOCON_RODIV_SHIFT)
#define REFOCON_ACTIVE   BIT(8)
#define REFOCON_DIVSWEN  BIT(9)
#define PIC32MZ_NUM_REFO 4

/* PMD registers base (offset 0 from system config base 0xBF800000) */
#define PMD_BASE_ADDR    0xBF800040
#define PMD_REG(n)       (((n) - 1) * 0x10)
#define PIC32MZ_NUM_PMD  7

/* Maximum timeout for oscillator switch (microseconds) */
#define OSC_SWITCH_TIMEOUT_US 10000

struct pic32mz_clock_config {
	volatile uint32_t *osc_base;
	volatile uint32_t *pb_base;
	uint32_t posc_freq;
};

struct pic32mz_clock_data {
	uint32_t sysclk_freq;
	uint32_t pbclk_freq[8];
	uint32_t upll_freq;
	uint8_t current_source;
};

static inline uint32_t reg_read(volatile uint32_t *base, uint32_t offset)
{
	return *(volatile uint32_t *)((uint8_t *)base + offset);
}

static inline void reg_write(volatile uint32_t *base, uint32_t offset, uint32_t val)
{
	*(volatile uint32_t *)((uint8_t *)base + offset) = val;
}

static inline void reg_set(volatile uint32_t *base, uint32_t offset, uint32_t bits)
{
	*(volatile uint32_t *)((uint8_t *)base + offset + REG_SET) = bits;
}

static inline void reg_clr(volatile uint32_t *base, uint32_t offset, uint32_t bits)
{
	*(volatile uint32_t *)((uint8_t *)base + offset + REG_CLR) = bits;
}

static void syskey_unlock(void)
{
	volatile uint32_t *syskey = (volatile uint32_t *)SYSKEY_ADDR;

	*syskey = 0x00000000;
	*syskey = 0xAA996655;
	*syskey = 0x556699AA;
}

static void syskey_lock(void)
{
	volatile uint32_t *syskey = (volatile uint32_t *)SYSKEY_ADDR;

	*syskey = 0x33333333;
}

static uint32_t pic32mz_get_spll_freq(const struct pic32mz_clock_config *cfg)
{
	uint32_t spllcon = reg_read(cfg->osc_base, SPLLCON);
	uint32_t input_freq;
	uint32_t idiv, mult, odiv;

	/* PLL input source: FRC or POSC */
	if (spllcon & SPLLCON_PLLICLK) {
		input_freq = FRC_FREQ_HZ;
	} else {
		input_freq = cfg->posc_freq;
	}

	/* Input divider: value + 1 */
	idiv = ((spllcon & SPLLCON_PLLIDIV_MASK) >> SPLLCON_PLLIDIV_SHIFT) + 1;

	/* Multiplier: value + 1 */
	mult = ((spllcon & SPLLCON_PLLMULT_MASK) >> SPLLCON_PLLMULT_SHIFT) + 1;

	/* Output divider: encoded as power of 2 */
	uint32_t odiv_field = (spllcon & SPLLCON_PLLODIV_MASK) >> SPLLCON_PLLODIV_SHIFT;

	switch (odiv_field) {
	case 0: odiv = 2; break;
	case 1: odiv = 2; break;
	case 2: odiv = 4; break;
	case 3: odiv = 8; break;
	case 4: odiv = 16; break;
	default: odiv = 32; break;
	}

	return (input_freq / idiv) * mult / odiv;
}

static uint32_t pic32mz_get_sysclk(const struct pic32mz_clock_config *cfg, uint8_t source)
{
	switch (source) {
	case CLKSRC_FRC:
		return FRC_FREQ_HZ;
	case CLKSRC_FRCDIV: {
		uint32_t osccon = reg_read(cfg->osc_base, OSCCON);
		uint32_t frcdiv = (osccon & OSCCON_FRCDIV_MASK) >> OSCCON_FRCDIV_SHIFT;

		return FRC_FREQ_HZ >> frcdiv;
	}
	case CLKSRC_SPLL:
		return pic32mz_get_spll_freq(cfg);
	case CLKSRC_POSC:
		return cfg->posc_freq;
	case CLKSRC_SOSC:
		return 32768U;
	case CLKSRC_LPRC:
		return 32000U;
	default:
		return FRC_FREQ_HZ;
	}
}

static void pic32mz_update_pbclk(const struct pic32mz_clock_config *cfg,
				  struct pic32mz_clock_data *data)
{
	for (int i = 0; i < 8; i++) {
		uint32_t pbdiv_reg = reg_read(cfg->pb_base, PBDIV_REG(i + 1));

		if (pbdiv_reg & PBDIV_ON) {
			uint32_t div = (pbdiv_reg & PBDIV_PBDIV_MASK) + 1;

			data->pbclk_freq[i] = data->sysclk_freq / div;
		} else {
			data->pbclk_freq[i] = 0;
		}
	}
}

static int pic32mz_clock_on(const struct device *dev, clock_control_subsys_t sys)
{
	const struct pic32mz_clock_config *cfg = dev->config;
	uint32_t bus = (uint32_t)(uintptr_t)sys;

	if (bus < 1 || bus > 8) {
		return -EINVAL;
	}

	reg_set(cfg->pb_base, PBDIV_REG(bus), PBDIV_ON);
	return 0;
}

static int pic32mz_clock_off(const struct device *dev, clock_control_subsys_t sys)
{
	const struct pic32mz_clock_config *cfg = dev->config;
	uint32_t bus = (uint32_t)(uintptr_t)sys;

	if (bus < 1 || bus > 8) {
		return -EINVAL;
	}

	/* PBCLK1 and PBCLK7 cannot be disabled */
	if (bus == 1 || bus == 7) {
		return -ENOTSUP;
	}

	reg_clr(cfg->pb_base, PBDIV_REG(bus), PBDIV_ON);
	return 0;
}

static int pic32mz_clock_get_rate(const struct device *dev, clock_control_subsys_t sys,
				   uint32_t *rate)
{
	struct pic32mz_clock_data *data = dev->data;
	uint32_t subsys = (uint32_t)(uintptr_t)sys;

	/* PBCLK1-8: subsys = 1..8 */
	if (subsys >= 1 && subsys <= 8) {
		*rate = data->pbclk_freq[subsys - 1];
		return 0;
	}

	/* USB PLL: subsys = PIC32MZ_CLK_USB (0x10) */
	if (subsys == 0x10) {
		*rate = data->upll_freq;
		if (data->upll_freq == 0) {
			return -ENOTSUP;
		}
		return 0;
	}

	/* SYSCLK: subsys = PIC32MZ_CLK_SYSCLK (0x20) */
	if (subsys == 0x20) {
		*rate = data->sysclk_freq;
		return 0;
	}

	return -EINVAL;
}

static enum clock_control_status pic32mz_clock_get_status(const struct device *dev,
							   clock_control_subsys_t sys)
{
	const struct pic32mz_clock_config *cfg = dev->config;
	uint32_t subsys = (uint32_t)(uintptr_t)sys;

	if (subsys >= 1 && subsys <= 8) {
		uint32_t pbdiv_reg = reg_read(cfg->pb_base, PBDIV_REG(subsys));

		return (pbdiv_reg & PBDIV_ON) ? CLOCK_CONTROL_STATUS_ON
					      : CLOCK_CONTROL_STATUS_OFF;
	}

	if (subsys == 0x10) {
		/* USB PLL is always on if POSC is active and USB not PMD-disabled */
		volatile uint32_t *pmd_base = (volatile uint32_t *)PMD_BASE_ADDR;
		uint32_t pmd5 = reg_read(pmd_base, PMD_REG(5));

		return (pmd5 & BIT(24)) ? CLOCK_CONTROL_STATUS_OFF
					: CLOCK_CONTROL_STATUS_ON;
	}

	return CLOCK_CONTROL_STATUS_UNKNOWN;
}

static int pic32mz_clock_configure(const struct device *dev, clock_control_subsys_t sys,
				    void *cfg_data)
{
	const struct pic32mz_clock_config *cfg = dev->config;
	struct pic32mz_clock_data *data = dev->data;
	struct pic32mz_clock_opp *opp = (struct pic32mz_clock_opp *)cfg_data;
	uint32_t osccon;
	uint32_t timeout;

	if (opp == NULL) {
		return -EINVAL;
	}

	uint8_t new_source = opp->clock_source;

	/* If already on the requested source, just update dividers */
	if (new_source == data->current_source) {
		goto update_dividers;
	}

	/* Perform oscillator switch */
	syskey_unlock();

	/* Clear NOSC field and set new source */
	osccon = reg_read(cfg->osc_base, OSCCON);
	osccon &= ~OSCCON_NOSC_MASK;
	osccon |= ((uint32_t)new_source << OSCCON_NOSC_SHIFT);
	osccon |= OSCCON_OSWEN;
	reg_write(cfg->osc_base, OSCCON, osccon);

	syskey_lock();

	/* Wait for switch to complete (OSWEN clears when done) */
	timeout = OSC_SWITCH_TIMEOUT_US;
	while ((reg_read(cfg->osc_base, OSCCON) & OSCCON_OSWEN) && timeout > 0) {
		k_busy_wait(1);
		timeout--;
	}

	if (timeout == 0) {
		LOG_ERR("Oscillator switch timeout");
		return -ETIMEDOUT;
	}

	data->current_source = new_source;
	data->sysclk_freq = pic32mz_get_sysclk(cfg, new_source);

update_dividers:
	/* Update PBCLK dividers */
	for (int i = 0; i < 8; i++) {
		if (opp->pbclk_div[i] != 0) {
			uint32_t reg_val = reg_read(cfg->pb_base, PBDIV_REG(i + 1));

			reg_val &= ~PBDIV_PBDIV_MASK;
			reg_val |= (opp->pbclk_div[i] - 1) & PBDIV_PBDIV_MASK;
			reg_write(cfg->pb_base, PBDIV_REG(i + 1), reg_val);
		}
	}

	/* Refresh cached frequencies */
	pic32mz_update_pbclk(cfg, data);

	LOG_INF("Clock switched: SYSCLK=%u Hz, source=%u",
		data->sysclk_freq, data->current_source);

	return 0;
}

/*
 * Peripheral Module Disable (PMD) support.
 * Provides finer-grained clock gating than PBCLKn on/off.
 * Each peripheral has a specific bit in PMD1-7.
 */
static int pic32mz_pmd_enable(uint8_t pmd_reg, uint8_t bit)
{
	volatile uint32_t *pmd_base = (volatile uint32_t *)PMD_BASE_ADDR;

	if (pmd_reg < 1 || pmd_reg > PIC32MZ_NUM_PMD || bit > 31) {
		return -EINVAL;
	}

	syskey_unlock();
	reg_clr(pmd_base, PMD_REG(pmd_reg), BIT(bit));
	syskey_lock();
	return 0;
}

static int pic32mz_pmd_disable(uint8_t pmd_reg, uint8_t bit)
{
	volatile uint32_t *pmd_base = (volatile uint32_t *)PMD_BASE_ADDR;

	if (pmd_reg < 1 || pmd_reg > PIC32MZ_NUM_PMD || bit > 31) {
		return -EINVAL;
	}

	syskey_unlock();
	reg_set(pmd_base, PMD_REG(pmd_reg), BIT(bit));
	syskey_lock();
	return 0;
}

/*
 * Reference Clock (REFO1-4) support.
 * Provides configurable clock output for external peripherals.
 */
static int pic32mz_refclk_configure(const struct pic32mz_clock_config *cfg,
				     const struct pic32mz_refclk_cfg *refcfg)
{
	if (refcfg->refo_id < 1 || refcfg->refo_id > PIC32MZ_NUM_REFO) {
		return -EINVAL;
	}

	uint32_t refocon_offset = REFOCON_REG(refcfg->refo_id);

	/* Wait for ACTIVE to clear before modifying */
	uint32_t timeout = 1000;

	while ((reg_read(cfg->osc_base, refocon_offset) & REFOCON_ACTIVE) && timeout > 0) {
		k_busy_wait(1);
		timeout--;
	}

	if (timeout == 0) {
		return -EBUSY;
	}

	/* Disable REFO before reconfiguring */
	reg_clr(cfg->osc_base, refocon_offset, REFOCON_ON);

	/* Build new register value */
	uint32_t refocon = 0;

	refocon |= (refcfg->source & REFOCON_ROSEL_MASK);
	refocon |= ((uint32_t)refcfg->divider << REFOCON_RODIV_SHIFT) & REFOCON_RODIV_MASK;

	if (refcfg->output_enable) {
		refocon |= REFOCON_OE;
	}

	if (refcfg->enable) {
		refocon |= REFOCON_ON;
	}

	reg_write(cfg->osc_base, refocon_offset, refocon);

	/* Write trim value if non-zero */
	if (refcfg->trim != 0) {
		reg_write(cfg->osc_base, REFOTRIM_REG(refcfg->refo_id),
			  (uint32_t)refcfg->trim << 23);
	}

	return 0;
}

static DEVICE_API(clock_control, pic32mz_clock_api) = {
	.on = pic32mz_clock_on,
	.off = pic32mz_clock_off,
	.get_rate = pic32mz_clock_get_rate,
	.get_status = pic32mz_clock_get_status,
	.configure = pic32mz_clock_configure,
};

static int pic32mz_clock_init(const struct device *dev)
{
	const struct pic32mz_clock_config *cfg = dev->config;
	struct pic32mz_clock_data *data = dev->data;
	uint32_t osccon;

	/* Read current oscillator source from hardware */
	osccon = reg_read(cfg->osc_base, OSCCON);
	data->current_source = (osccon & OSCCON_COSC_MASK) >> OSCCON_COSC_SHIFT;

	/* Calculate SYSCLK based on actual hardware state */
	data->sysclk_freq = pic32mz_get_sysclk(cfg, data->current_source);

	/* Read all PBCLK dividers */
	pic32mz_update_pbclk(cfg, data);

	/*
	 * Validate USB PLL configuration.
	 * Read DEVCFG2.UPLLFSEL and compare with actual POSC frequency.
	 * USB PLL is valid only when:
	 *   - UPLLFSEL=1 and POSC=24 MHz, or
	 *   - UPLLFSEL=0 and POSC=12 MHz
	 * If POSC is disabled (freq=0) or mismatched, USB PLL output is invalid.
	 */
	volatile uint32_t *devcfg2_ptr = (volatile uint32_t *)DEVCFG2_ADDR;
	uint32_t devcfg2 = *devcfg2_ptr;
	uint32_t expected_posc;

	if (devcfg2 & DEVCFG2_UPLLFSEL) {
		expected_posc = UPLL_INPUT_24MHZ;
	} else {
		expected_posc = UPLL_INPUT_12MHZ;
	}

	if (cfg->posc_freq == expected_posc) {
		data->upll_freq = UPLL_OUTPUT_FREQ_HZ;
	} else if (cfg->posc_freq == 0) {
		data->upll_freq = 0;
		LOG_WRN("USB PLL unavailable: POSC is disabled");
	} else {
		data->upll_freq = 0;
		LOG_ERR("USB PLL misconfigured: UPLLFSEL expects %u Hz POSC, "
			"but DT declares %u Hz",
			expected_posc, cfg->posc_freq);
	}

	LOG_INF("PIC32MZ clock init: SYSCLK=%u Hz (source=%u), UPLL=%u Hz",
		data->sysclk_freq, data->current_source, data->upll_freq);

	return 0;
}

static const struct pic32mz_clock_config pic32mz_clock_cfg = {
	.osc_base = (volatile uint32_t *)DT_INST_REG_ADDR_BY_NAME(0, osc),
	.pb_base = (volatile uint32_t *)DT_INST_REG_ADDR_BY_NAME(0, pb),
	.posc_freq = DT_PROP(DT_CHILD(DT_DRV_INST(0), posc), clock_frequency),
};

static struct pic32mz_clock_data pic32mz_clock_data_0;

DEVICE_DT_INST_DEFINE(0, pic32mz_clock_init, NULL,
		      &pic32mz_clock_data_0, &pic32mz_clock_cfg,
		      PRE_KERNEL_1, CONFIG_CLOCK_CONTROL_INIT_PRIORITY,
		      &pic32mz_clock_api);
