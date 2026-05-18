/*
 * Copyright (c) 2021 IP-Logix Inc.
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT microchip_pic32ck_mdio

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <soc.h>
#include <zephyr/drivers/mdio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/net/mdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mdio_pic32ck_gc, CONFIG_MDIO_LOG_LEVEL);

struct mdio_sam_dev_data {
	struct k_sem sem;
};

struct mdio_sam_dev_config {
	Gmac * const regs;
	const struct pinctrl_dev_config *pcfg;
};

static int mdio_transfer(const struct device *dev, uint8_t prtad, uint8_t regad,
			 enum mdio_opcode op, bool c45, uint16_t data_in,
			 uint16_t *data_out)
{
	const struct mdio_sam_dev_config *const cfg = dev->config;
	struct mdio_sam_dev_data *const data = dev->data;
	int timeout = 50;

	k_sem_take(&data->sem, K_FOREVER);

	/* Write mdio transaction */
	uint32_t man_val = (c45 ? 0U : GMAC_MAN_CLTTO)
			    |  GMAC_MAN_OP(op)
			    |  GMAC_MAN_WTN(0x02)
			    |  GMAC_MAN_PHYA(prtad)
			    |  GMAC_MAN_REGA(regad)
			    |  GMAC_MAN_DATA(data_in);
	cfg->regs->GMAC_MAN = man_val;

	/* Wait until done */
	while (!(cfg->regs->GMAC_NSR & GMAC_NSR_IDLE)) {
		if (timeout-- == 0U) {
			k_sem_give(&data->sem);
			return -ETIMEDOUT;
		}

		k_sleep(K_MSEC(5));
	}

	if (data_out) {
		*data_out = cfg->regs->GMAC_MAN & GMAC_MAN_DATA_Msk;
	}

	k_sem_give(&data->sem);

	return 0;
}

static int mdio_sam_read(const struct device *dev, uint8_t prtad, uint8_t regad,
			 uint16_t *data)
{
	return mdio_transfer(dev, prtad, regad, MDIO_OP_C22_READ, false,
			     0, data);
}

static int mdio_sam_write(const struct device *dev, uint8_t prtad,
			  uint8_t regad, uint16_t data)
{
	return mdio_transfer(dev, prtad, regad, MDIO_OP_C22_WRITE, false,
			     data, NULL);
}

static int mdio_sam_read_c45(const struct device *dev, uint8_t prtad,
			     uint8_t devad, uint16_t regad, uint16_t *data)
{
	int err;

	err = mdio_transfer(dev, prtad, devad, MDIO_OP_C45_ADDRESS, true,
			    regad, NULL);
	if (!err) {
		err = mdio_transfer(dev, prtad, devad, MDIO_OP_C45_READ, true,
				    0, data);
	}

	return err;
}

static int mdio_sam_write_c45(const struct device *dev, uint8_t prtad,
			      uint8_t devad, uint16_t regad, uint16_t data)
{
	int err;

	err = mdio_transfer(dev, prtad, devad, MDIO_OP_C45_ADDRESS, true,
			    regad, NULL);
	if (!err) {
		err = mdio_transfer(dev, prtad, devad, MDIO_OP_C45_WRITE, true,
				    data, NULL);
	}

	return err;
}

static int mdio_sam_initialize(const struct device *dev)
{
	const struct mdio_sam_dev_config *const cfg = dev->config;
	struct mdio_sam_dev_data *const data = dev->data;
	int retval;

	k_sem_init(&data->sem, 1, 1);

	retval = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);

	if (retval >= 0) {
		Gmac *gmac = cfg->regs;

		/* Enable GCLK_TX and GCLK_TSU before ETH wrapper can sync */
		volatile uint32_t *gclk_pchctrl41 = (volatile uint32_t *)(0x44010000 + 0x80 + 41 * 4);
		volatile uint32_t *gclk_pchctrl42 = (volatile uint32_t *)(0x44010000 + 0x80 + 42 * 4);
		*gclk_pchctrl41 = (0U << 0) | (1U << 6); /* GEN0 + CHEN */
		while ((*gclk_pchctrl41 & (1U << 6)) == 0) {
		}
		*gclk_pchctrl42 = (0U << 0) | (1U << 6); /* GEN0 + CHEN */
		while ((*gclk_pchctrl42 & (1U << 6)) == 0) {
		}

		/* Enable ETH peripheral wrapper */
		gmac->ETH_CTRLA = ETH_CTRLA_ENABLE_Msk;
		while (gmac->ETH_SYNCB) {
		}
		/* RMII mode */
		gmac->ETH_CTRLB &= ~(ETH_CTRLB_GMIIEN_Msk | ETH_CTRLB_GBITCLKREQ_Msk);
		while (gmac->ETH_SYNCB) {
		}
		/* Set MDC clock: 120MHz/48 = 2.5MHz */
		gmac->ETH_NCFGR |= GMAC_NCFGR_CLK_MCK_48;
		/* Enable MDIO */
		gmac->ETH_NCR |= GMAC_NCR_MPE;
	}

	return retval;
}

static DEVICE_API(mdio, mdio_sam_driver_api) = {
	.read = mdio_sam_read,
	.write = mdio_sam_write,
	.read_c45 = mdio_sam_read_c45,
	.write_c45 = mdio_sam_write_c45,
};

/* No clock config needed - PIC32CK handles clocks in init */
#define MDIO_SAM_CLOCK(n)

#define MDIO_SAM_CONFIG(n)						\
static const struct mdio_sam_dev_config mdio_sam_dev_config_##n = {	\
	.regs = (Gmac *)DT_REG_ADDR(DT_INST_PARENT(n)),			\
	.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),			\
	MDIO_SAM_CLOCK(n)						\
};

#define MDIO_SAM_DEVICE(n)						\
	PINCTRL_DT_INST_DEFINE(n);					\
	MDIO_SAM_CONFIG(n);						\
	static struct mdio_sam_dev_data mdio_sam_dev_data##n;		\
	DEVICE_DT_INST_DEFINE(n,					\
			      &mdio_sam_initialize,			\
			      NULL,					\
			      &mdio_sam_dev_data##n,			\
			      &mdio_sam_dev_config_##n, POST_KERNEL,	\
			      CONFIG_MDIO_INIT_PRIORITY,		\
			      &mdio_sam_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MDIO_SAM_DEVICE)
