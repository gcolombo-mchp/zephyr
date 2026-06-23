/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPIO driver for PIC32MZ family.
 *
 * Register layout per port (0x100 bytes):
 *   +0x00 ANSEL  (analog select)       +0x04 ANSELCLR  +0x08 ANSELSET
 *   +0x10 TRIS   (direction)           +0x14 TRISCLR   +0x18 TRISSET
 *   +0x20 PORT   (input read)
 *   +0x30 LAT    (output latch)        +0x34 LATCLR    +0x38 LATSET   +0x3C LATINV
 *   +0x40 ODC    (open drain)          +0x44 ODCCLR    +0x48 ODCSET
 *   +0x50 CNPU   (pull-up)             +0x54 CNPUCLR   +0x58 CNPUSET
 *   +0x60 CNPD   (pull-down)           +0x64 CNPDCLR   +0x68 CNPDSET
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>

#define DT_DRV_COMPAT microchip_pic32mz_gpio

/* Register offsets */
#define ANSEL    0x00
#define ANSELCLR 0x04
#define ANSELSET 0x08
#define TRIS     0x10
#define TRISCLR  0x14
#define TRISSET  0x18
#define PORT_REG 0x20
#define LAT      0x30
#define LATCLR   0x34
#define LATSET   0x38
#define LATINV   0x3C
#define ODC      0x40
#define ODCCLR   0x44
#define ODCSET   0x48
#define CNPU     0x50
#define CNPUCLR  0x54
#define CNPUSET  0x58
#define CNPD     0x60
#define CNPDCLR  0x64
#define CNPDSET  0x68

struct gpio_pic32mz_config {
	struct gpio_driver_config common;
	mem_addr_t base;
};

struct gpio_pic32mz_data {
	struct gpio_driver_data common;
};

static inline void reg_write(mem_addr_t base, uint32_t offset, uint32_t val)
{
	sys_write32(val, base + offset);
}

static inline uint32_t reg_read(mem_addr_t base, uint32_t offset)
{
	return sys_read32(base + offset);
}

static int gpio_pic32mz_configure(const struct device *dev,
				   gpio_pin_t pin, gpio_flags_t flags)
{
	const struct gpio_pic32mz_config *cfg = dev->config;
	mem_addr_t base = cfg->base;
	uint32_t bit = BIT(pin);

	/* Disable analog function (make pin digital) */
	reg_write(base, ANSELCLR, bit);

	/* Direction */
	if (flags & GPIO_OUTPUT) {
		reg_write(base, TRISCLR, bit);

		/* Initial value */
		if (flags & GPIO_OUTPUT_INIT_HIGH) {
			reg_write(base, LATSET, bit);
		} else if (flags & GPIO_OUTPUT_INIT_LOW) {
			reg_write(base, LATCLR, bit);
		}
	} else if (flags & GPIO_INPUT) {
		reg_write(base, TRISSET, bit);
	}

	/* Open drain */
	if (flags & GPIO_OPEN_DRAIN) {
		reg_write(base, ODCSET, bit);
	} else {
		reg_write(base, ODCCLR, bit);
	}

	/* Pull-up / pull-down */
	if (flags & GPIO_PULL_UP) {
		reg_write(base, CNPUSET, bit);
		reg_write(base, CNPDCLR, bit);
	} else if (flags & GPIO_PULL_DOWN) {
		reg_write(base, CNPDSET, bit);
		reg_write(base, CNPUCLR, bit);
	} else {
		reg_write(base, CNPUCLR, bit);
		reg_write(base, CNPDCLR, bit);
	}

	return 0;
}

static int gpio_pic32mz_port_get_raw(const struct device *dev,
				      gpio_port_value_t *value)
{
	const struct gpio_pic32mz_config *cfg = dev->config;

	*value = reg_read(cfg->base, PORT_REG);
	return 0;
}

static int gpio_pic32mz_port_set_masked_raw(const struct device *dev,
					     gpio_port_pins_t mask,
					     gpio_port_value_t value)
{
	const struct gpio_pic32mz_config *cfg = dev->config;
	mem_addr_t base = cfg->base;

	reg_write(base, LATCLR, mask & ~value);
	reg_write(base, LATSET, mask & value);
	return 0;
}

static int gpio_pic32mz_port_set_bits_raw(const struct device *dev,
					   gpio_port_pins_t pins)
{
	const struct gpio_pic32mz_config *cfg = dev->config;

	reg_write(cfg->base, LATSET, pins);
	return 0;
}

static int gpio_pic32mz_port_clear_bits_raw(const struct device *dev,
					     gpio_port_pins_t pins)
{
	const struct gpio_pic32mz_config *cfg = dev->config;

	reg_write(cfg->base, LATCLR, pins);
	return 0;
}

static int gpio_pic32mz_port_toggle_bits(const struct device *dev,
					  gpio_port_pins_t pins)
{
	const struct gpio_pic32mz_config *cfg = dev->config;

	reg_write(cfg->base, LATINV, pins);
	return 0;
}

static int gpio_pic32mz_init(const struct device *dev)
{
	return 0;
}

static DEVICE_API(gpio, gpio_pic32mz_api) = {
	.pin_configure = gpio_pic32mz_configure,
	.port_get_raw = gpio_pic32mz_port_get_raw,
	.port_set_masked_raw = gpio_pic32mz_port_set_masked_raw,
	.port_set_bits_raw = gpio_pic32mz_port_set_bits_raw,
	.port_clear_bits_raw = gpio_pic32mz_port_clear_bits_raw,
	.port_toggle_bits = gpio_pic32mz_port_toggle_bits,
};

#define GPIO_PIC32MZ_INIT(n)						\
	static struct gpio_pic32mz_data gpio_pic32mz_data_##n;		\
	static const struct gpio_pic32mz_config gpio_pic32mz_cfg_##n = {\
		.common = { .port_pin_mask = GPIO_PORT_PIN_MASK_FROM_DT_INST(n) }, \
		.base = DT_INST_REG_ADDR(n),				\
	};								\
	DEVICE_DT_INST_DEFINE(n, gpio_pic32mz_init, NULL,		\
			      &gpio_pic32mz_data_##n,			\
			      &gpio_pic32mz_cfg_##n,			\
			      PRE_KERNEL_1,				\
			      CONFIG_GPIO_INIT_PRIORITY,			\
			      &gpio_pic32mz_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_PIC32MZ_INIT)
