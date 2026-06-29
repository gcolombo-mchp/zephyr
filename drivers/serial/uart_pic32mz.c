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
 * UART driver for PIC32MZ family.
 *
 * Register layout per UART instance (0x10 spacing with CLR/SET/INV):
 *   +0x00  UxMODE   - Mode Control
 *   +0x10  UxSTA    - Status and Control
 *   +0x20  UxTXREG  - Transmit Data
 *   +0x30  UxRXREG  - Receive Data
 *   +0x40  UxBRG    - Baud Rate Generator
 *
 * Features:
 *   - 8-level deep TX and RX FIFOs
 *   - Baud rate: PBCLK2 / (16*(BRG+1)) or PBCLK2 / (4*(BRG+1)) in high-speed
 *   - Polling and interrupt-driven modes
 *   - Error detection: overrun, framing, parity
 *
 * Reference: DS60001107H - PIC32 FRM Section 21 "UART"
 * Reference: DS60001320H - PIC32MZ EF Data Sheet, Table 22-1
 * Reference: Harmony CSP peripheral/uart_02478
 */

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/mchp_clock_pic32mz.h>
#include <zephyr/drivers/interrupt_controller/intc_pic32mz_evic.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT microchip_pic32mz_uart

LOG_MODULE_REGISTER(uart_pic32mz, CONFIG_UART_LOG_LEVEL);

/* Register offsets (SFR spacing = 0x10 for CLR/SET/INV) */
#define UXMODE   0x00
#define UXSTA    0x10
#define UXTXREG  0x20
#define UXRXREG  0x30
#define UXBRG    0x40

/* CLR/SET offsets */
#define REG_CLR  0x04
#define REG_SET  0x08

/* UxMODE bits */
#define UXMODE_ON       BIT(15)
#define UXMODE_SIDL     BIT(13)
#define UXMODE_IREN     BIT(12)
#define UXMODE_RTSMD    BIT(11)
#define UXMODE_UEN_SHIFT 8
#define UXMODE_UEN_MASK (0x3 << UXMODE_UEN_SHIFT)
#define UXMODE_WAKE     BIT(7)
#define UXMODE_LPBACK   BIT(6)
#define UXMODE_ABAUD    BIT(5)
#define UXMODE_RXINV    BIT(4)
#define UXMODE_BRGH     BIT(3)
#define UXMODE_PDSEL_SHIFT 1
#define UXMODE_PDSEL_MASK (0x3 << UXMODE_PDSEL_SHIFT)
#define UXMODE_STSEL    BIT(0)

/* UxSTA bits */
#define UXSTA_UTXISEL1  BIT(15)
#define UXSTA_UTXISEL0  BIT(14)
#define UXSTA_UTXINV    BIT(13)
#define UXSTA_URXEN     BIT(12)
#define UXSTA_UTXBRK    BIT(11)
#define UXSTA_UTXEN     BIT(10)
#define UXSTA_UTXBF     BIT(9)
#define UXSTA_TRMT      BIT(8)
#define UXSTA_URXISEL_SHIFT 6
#define UXSTA_URXISEL_MASK (0x3 << UXSTA_URXISEL_SHIFT)
#define UXSTA_ADDEN     BIT(5)
#define UXSTA_RIDLE     BIT(4)
#define UXSTA_PERR      BIT(3)
#define UXSTA_FERR      BIT(2)
#define UXSTA_OERR      BIT(1)
#define UXSTA_URXDA     BIT(0)

struct uart_pic32mz_config {
	mem_addr_t base;
	uint32_t irq_fault;
	uint32_t irq_rx;
	uint32_t irq_tx;
	uint8_t irq_priority;
	const struct device *clk_dev;
	uint8_t clk_bus;
	uint32_t baud_rate;
	bool high_speed;
	const struct pinctrl_dev_config *pcfg;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	void (*irq_config_func)(const struct device *dev);
#endif
};

struct uart_pic32mz_data {
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t cb;
	void *cb_data;
#endif
};

static inline uint32_t uart_read(const struct uart_pic32mz_config *cfg, uint32_t offset)
{
	return sys_read32(cfg->base + offset);
}

static inline void uart_write(const struct uart_pic32mz_config *cfg, uint32_t offset,
			      uint32_t val)
{
	sys_write32(val, cfg->base + offset);
}

static inline void uart_set(const struct uart_pic32mz_config *cfg, uint32_t offset,
			    uint32_t bits)
{
	sys_write32(bits, cfg->base + offset + REG_SET);
}

static inline void uart_clr(const struct uart_pic32mz_config *cfg, uint32_t offset,
			    uint32_t bits)
{
	sys_write32(bits, cfg->base + offset + REG_CLR);
}

static int uart_pic32mz_poll_in(const struct device *dev, unsigned char *c)
{
	const struct uart_pic32mz_config *cfg = dev->config;
	uint32_t sta = uart_read(cfg, UXSTA);

	if (sta & UXSTA_OERR) {
		uart_clr(cfg, UXSTA, UXSTA_OERR);
	}

	if (!(sta & UXSTA_URXDA)) {
		return -1;
	}

	*c = (unsigned char)(uart_read(cfg, UXRXREG) & 0xFF);
	return 0;
}

static void uart_pic32mz_poll_out(const struct device *dev, unsigned char c)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	while (uart_read(cfg, UXSTA) & UXSTA_UTXBF) {
		/* Wait for TX FIFO to have space */
	}

	uart_write(cfg, UXTXREG, (uint32_t)c);
}

static int uart_pic32mz_err_check(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;
	uint32_t sta = uart_read(cfg, UXSTA);
	int errors = 0;

	if (sta & UXSTA_OERR) {
		errors |= UART_ERROR_OVERRUN;
		/*
		 * Per Harmony pattern: drain RX FIFO before clearing OERR.
		 * Clearing OERR resets the FIFO, losing any buffered data.
		 * Reading first preserves framing/parity error flags per-byte.
		 */
		while (uart_read(cfg, UXSTA) & UXSTA_URXDA) {
			(void)uart_read(cfg, UXRXREG);
		}
		uart_clr(cfg, UXSTA, UXSTA_OERR);
	}
	if (sta & UXSTA_FERR) {
		errors |= UART_ERROR_FRAMING;
	}
	if (sta & UXSTA_PERR) {
		errors |= UART_ERROR_PARITY;
	}

	return errors;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

static int uart_pic32mz_fifo_fill(const struct device *dev, const uint8_t *tx_data, int size)
{
	const struct uart_pic32mz_config *cfg = dev->config;
	int count = 0;

	while (count < size && !(uart_read(cfg, UXSTA) & UXSTA_UTXBF)) {
		uart_write(cfg, UXTXREG, tx_data[count]);
		count++;
	}

	return count;
}

static int uart_pic32mz_fifo_read(const struct device *dev, uint8_t *rx_data, const int size)
{
	const struct uart_pic32mz_config *cfg = dev->config;
	int count = 0;

	while (count < size && (uart_read(cfg, UXSTA) & UXSTA_URXDA)) {
		rx_data[count] = (uint8_t)(uart_read(cfg, UXRXREG) & 0xFF);
		count++;
	}

	return count;
}

static void uart_pic32mz_irq_tx_enable(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	irq_enable(cfg->irq_tx);
}

static void uart_pic32mz_irq_tx_disable(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	irq_disable(cfg->irq_tx);
}

static int uart_pic32mz_irq_tx_ready(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	return !(uart_read(cfg, UXSTA) & UXSTA_UTXBF);
}

static int uart_pic32mz_irq_tx_complete(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	return !!(uart_read(cfg, UXSTA) & UXSTA_TRMT);
}

static void uart_pic32mz_irq_rx_enable(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	irq_enable(cfg->irq_rx);
}

static void uart_pic32mz_irq_rx_disable(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	irq_disable(cfg->irq_rx);
}

static int uart_pic32mz_irq_rx_ready(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	return !!(uart_read(cfg, UXSTA) & UXSTA_URXDA);
}

static void uart_pic32mz_irq_err_enable(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	irq_enable(cfg->irq_fault);
}

static void uart_pic32mz_irq_err_disable(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;

	irq_disable(cfg->irq_fault);
}

static int uart_pic32mz_irq_is_pending(const struct device *dev)
{
	return uart_pic32mz_irq_rx_ready(dev) || uart_pic32mz_irq_tx_ready(dev);
}

static int uart_pic32mz_irq_update(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 1;
}

static void uart_pic32mz_irq_callback_set(const struct device *dev,
					   uart_irq_callback_user_data_t cb,
					   void *cb_data)
{
	struct uart_pic32mz_data *data = dev->data;

	data->cb = cb;
	data->cb_data = cb_data;
}

static void uart_pic32mz_isr(const struct device *dev)
{
	struct uart_pic32mz_data *data = dev->data;
	const struct uart_pic32mz_config *cfg = dev->config;

	/* Clear EVIC interrupt flags for this UART */
	pic32mz_evic_irq_clear_flag(cfg->irq_rx);
	pic32mz_evic_irq_clear_flag(cfg->irq_tx);
	pic32mz_evic_irq_clear_flag(cfg->irq_fault);

	if (data->cb) {
		data->cb(dev, data->cb_data);
	}
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int uart_pic32mz_init(const struct device *dev)
{
	const struct uart_pic32mz_config *cfg = dev->config;
	uint32_t pbclk_freq;
	uint32_t brg_val;
	uint32_t mode = 0;
	int ret;

	/* Configure pin routing via PPS */
	ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

	/* Get PBCLK2 frequency from clock driver */
	ret = clock_control_get_rate(cfg->clk_dev, (clock_control_subsys_t)(uintptr_t)cfg->clk_bus,
				     &pbclk_freq);
	if (ret < 0) {
		return ret;
	}

	/* Calculate baud rate generator value.
	 * Harmony formula with rounding:
	 *   BRGH=0: uxbrg = ((srcClk >> 4) + (baud >> 1)) / baud - 1
	 *   BRGH=1: uxbrg = ((srcClk >> 2) + (baud >> 1)) / baud - 1
	 */
	if (cfg->high_speed) {
		brg_val = ((pbclk_freq >> 2) + (cfg->baud_rate >> 1)) / cfg->baud_rate - 1;
		mode |= UXMODE_BRGH;
	} else {
		brg_val = ((pbclk_freq >> 4) + (cfg->baud_rate >> 1)) / cfg->baud_rate - 1;
	}

	if (brg_val > 0xFFFF) {
		LOG_ERR("BRG value overflow: %u (PBCLK=%u, baud=%u)",
			brg_val, pbclk_freq, cfg->baud_rate);
		return -EINVAL;
	}

	/* Disable UART before configuration */
	uart_clr(cfg, UXMODE, UXMODE_ON);

	/* Configure UxMODE: 8N1 default, no flow control */
	uart_write(cfg, UXMODE, mode);

	/* Configure UxSTA: enable TX and RX */
	uart_write(cfg, UXSTA, UXSTA_UTXEN | UXSTA_URXEN);

	/* Set baud rate */
	uart_write(cfg, UXBRG, brg_val);

	/* Enable UART (set ON bit) */
	uart_set(cfg, UXMODE, UXMODE_ON);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	/* Set interrupt priorities */
	pic32mz_evic_irq_set_priority(cfg->irq_fault, cfg->irq_priority, 0);
	pic32mz_evic_irq_set_priority(cfg->irq_rx, cfg->irq_priority, 0);
	pic32mz_evic_irq_set_priority(cfg->irq_tx, cfg->irq_priority, 0);

	cfg->irq_config_func(dev);
#endif

	LOG_INF("UART@0x%08x init: baud=%u, BRG=%u, PBCLK=%u Hz",
		(uint32_t)cfg->base, cfg->baud_rate, brg_val, pbclk_freq);

	return 0;
}

static DEVICE_API(uart, uart_pic32mz_api) = {
	.poll_in = uart_pic32mz_poll_in,
	.poll_out = uart_pic32mz_poll_out,
	.err_check = uart_pic32mz_err_check,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill = uart_pic32mz_fifo_fill,
	.fifo_read = uart_pic32mz_fifo_read,
	.irq_tx_enable = uart_pic32mz_irq_tx_enable,
	.irq_tx_disable = uart_pic32mz_irq_tx_disable,
	.irq_tx_ready = uart_pic32mz_irq_tx_ready,
	.irq_tx_complete = uart_pic32mz_irq_tx_complete,
	.irq_rx_enable = uart_pic32mz_irq_rx_enable,
	.irq_rx_disable = uart_pic32mz_irq_rx_disable,
	.irq_rx_ready = uart_pic32mz_irq_rx_ready,
	.irq_err_enable = uart_pic32mz_irq_err_enable,
	.irq_err_disable = uart_pic32mz_irq_err_disable,
	.irq_is_pending = uart_pic32mz_irq_is_pending,
	.irq_update = uart_pic32mz_irq_update,
	.irq_callback_set = uart_pic32mz_irq_callback_set,
#endif
};

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
#define UART_PIC32MZ_IRQ_CONFIG(n)						\
	static void uart_pic32mz_irq_config_##n(const struct device *dev)	\
	{									\
		IRQ_CONNECT(DT_INST_IRQ_BY_NAME(n, rx, irq),			\
			    DT_INST_IRQ_BY_NAME(n, rx, priority),		\
			    uart_pic32mz_isr, DEVICE_DT_INST_GET(n), 0);	\
		IRQ_CONNECT(DT_INST_IRQ_BY_NAME(n, tx, irq),			\
			    DT_INST_IRQ_BY_NAME(n, tx, priority),		\
			    uart_pic32mz_isr, DEVICE_DT_INST_GET(n), 0);	\
		IRQ_CONNECT(DT_INST_IRQ_BY_NAME(n, fault, irq),			\
			    DT_INST_IRQ_BY_NAME(n, fault, priority),		\
			    uart_pic32mz_isr, DEVICE_DT_INST_GET(n), 0);	\
	}
#define UART_PIC32MZ_IRQ_CONFIG_INIT(n) .irq_config_func = uart_pic32mz_irq_config_##n,
#else
#define UART_PIC32MZ_IRQ_CONFIG(n)
#define UART_PIC32MZ_IRQ_CONFIG_INIT(n)
#endif

#define UART_PIC32MZ_INIT(n)							\
	PINCTRL_DT_INST_DEFINE(n);						\
	UART_PIC32MZ_IRQ_CONFIG(n)						\
	static struct uart_pic32mz_data uart_pic32mz_data_##n;			\
	static const struct uart_pic32mz_config uart_pic32mz_cfg_##n = {	\
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),			\
		.base = DT_INST_REG_ADDR(n),					\
		.irq_fault = DT_INST_IRQ_BY_NAME(n, fault, irq),		\
		.irq_rx = DT_INST_IRQ_BY_NAME(n, rx, irq),			\
		.irq_tx = DT_INST_IRQ_BY_NAME(n, tx, irq),			\
		.irq_priority = DT_INST_IRQ_BY_NAME(n, rx, priority),		\
		.clk_dev = DEVICE_DT_GET(DT_NODELABEL(clock)),			\
		.clk_bus = 2, /* PBCLK2 for UART */				\
		.baud_rate = DT_INST_PROP(n, current_speed),			\
		.high_speed = DT_INST_PROP_OR(n, microchip_high_speed, false),	\
		UART_PIC32MZ_IRQ_CONFIG_INIT(n)					\
	};									\
	DEVICE_DT_INST_DEFINE(n, uart_pic32mz_init, NULL,			\
			      &uart_pic32mz_data_##n,				\
			      &uart_pic32mz_cfg_##n,				\
			      PRE_KERNEL_1, CONFIG_SERIAL_INIT_PRIORITY,		\
			      &uart_pic32mz_api);

DT_INST_FOREACH_STATUS_OKAY(UART_PIC32MZ_INIT)
