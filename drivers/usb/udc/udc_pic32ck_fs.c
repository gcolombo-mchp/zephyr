/*
 * Copyright (c) 2024 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "udc_common.h"

#include <string.h>
#include <stdio.h>

#include <soc.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/usb/udc.h>

#include <zephyr/drivers/clock_control.h>
#include <zephyr/dt-bindings/clock/mchp_pic32ck_sg_gc_clock.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(udc_pic32ck_fs, CONFIG_UDC_DRIVER_LOG_LEVEL);

/*
 * Although the manual refers to this as an "Endpoint Descriptor structure", it
 * is actually an endpoint buffer descriptor and has a similar function to the
 * buffer descriptor in the UDC Kinetis driver.
 */
struct pic32ck_ebd_bank0 {
	uint32_t addr;
	/* PCKSIZE offset 0x04 */
	unsigned int byte_count : 14;
	unsigned int multi_packet_size : 14;
	unsigned int size : 3;
	unsigned int auto_zlp : 1;
	/* EXTREG offset 0x08*/
	unsigned int subpid : 4;
	unsigned int variable : 11;
	unsigned int reserved0 : 1;
	/* STATUS_BK offset 0x0A*/
	unsigned int erroflow : 1;
	unsigned int crcerr : 1;
	unsigned int reserved1 : 6;
	/* RESERVED */
	uint8_t reserved2[5];
} __packed;

struct pic32ck_ebd_bank1 {
	uint32_t addr;
	/* PCKSIZE offset 0x14 */
	unsigned int byte_count : 14;
	unsigned int multi_packet_size : 14;
	unsigned int size : 3;
	unsigned int auto_zlp : 1;
	/* RESERVED, no EXTREG */
	uint8_t reserved0[2];
	/* STATUS_BK offset 0x1A*/
	unsigned int erroflow : 1;
	unsigned int crcerr : 1;
	unsigned int reserved1 : 6;
	/* RESERVED */
	uint8_t reserved2[5];
} __packed;

struct pic32ck_ep_buffer_desc {
	/* Used for OUT endpoints 0x00, 0x01 ... 0x08 */
	struct pic32ck_ebd_bank0 bank0;
	/* Used for IN endpoints 0x80, 0x81 ... 0x88 */
	struct pic32ck_ebd_bank1 bank1;
} __packed;

BUILD_ASSERT(sizeof(struct pic32ck_ep_buffer_desc) == 32, "Broken endpoint buffer descriptor");

/* PIC32CK FS is always full-speed only */
#define PIC32CK_FS_HS_ENABLED 0
#define UDC_PIC32CK_FS_EP_MPS 64

struct udc_pic32ck_fs_config {
	usb_device_registers_t *base;
	struct pic32ck_ep_buffer_desc *bdt;
	size_t num_of_eps;
	struct udc_ep_config *ep_cfg_in;
	struct udc_ep_config *ep_cfg_out;
	struct pinctrl_dev_config *const pcfg;
	void (*irq_enable_func)(const struct device *dev);
	void (*irq_disable_func)(const struct device *dev);
	void (*make_thread)(const struct device *dev);
};

enum pic32ck_event_type {
	/* Setup packet received */
	PIC32CK_EVT_SETUP,
	/* Trigger new transfer (except control OUT) */
	PIC32CK_EVT_XFER_NEW,
	/* Transfer for specific endpoint is finished */
	PIC32CK_EVT_XFER_FINISHED,
};

struct udc_pic32ck_fs_data {
	struct k_thread thread_data;
	/*
	 * events are events that the driver thread waits.
	 * xfer_new and xfer_finished contain information on which endpoints
	 * events PIC32CK_EVT_XFER_NEW or PIC32CK_EVT_XFER_FINISHED are triggered.
	 * The mapping is bits 31..16 for IN endpoints and bits 15..0 for OUT
	 * endpoints.
	 */
	struct k_event events;
	atomic_t xfer_new;
	atomic_t xfer_finished;
	/*
	 * This control OUT endpoint buffer is persistent because we have no
	 * control over when the host sends a setup packet. All other endpoints
	 * use multi-packet transfers and transfer buffers directly.
	 */
	uint8_t ctrl_out_buf[64];
	uint8_t setup[8];
};

static inline int udc_ep_to_bnum(const uint8_t ep)
{
	if (USB_EP_DIR_IS_IN(ep)) {
		return 16UL + USB_EP_GET_IDX(ep);
	}

	return USB_EP_GET_IDX(ep);
}

static inline uint8_t udc_pull_ep_from_bmsk(uint32_t *const bitmap)
{
	unsigned int bit;

	__ASSERT_NO_MSG(bitmap && *bitmap);

	bit = find_lsb_set(*bitmap) - 1;
	*bitmap &= ~BIT(bit);

	if (bit >= 16U) {
		return USB_EP_DIR_IN | (bit - 16U);
	} else {
		return USB_EP_DIR_OUT | bit;
	}
}

/* For CTRLA.ENABLE and CTRLA.SWRST */
static void pic32ck_wait_syncbusy(const struct device *dev)
{
	const struct udc_pic32ck_fs_config *config = dev->config;
	usb_device_registers_t *const base = config->base;

	while (base->USB_SYNCBUSY != 0) {
	}
}

static void pic32ck_load_padcal(const struct device *dev)
{
	const struct udc_pic32ck_fs_config *config = dev->config;
	usb_device_registers_t *const base = config->base;

	/* Use default calibration values for PIC32CK */
	base->USB_PADCAL = USB_PADCAL_TRANSN(5) |
			   USB_PADCAL_TRANSP(29) |
			   USB_PADCAL_TRIM(3);
}

static uint8_t pic32ck_get_bd_size(const uint16_t mps)
{
	switch (mps) {
	case 8:
		return 0;
	case 16:
		return 1;
	case 32:
		return 2;
	case 64:
		return 3;
	case 128:
		return 4;
	case 256:
		return 5;
	case 512:
		return 6;
	case 1023:
		return 7;
	default:
		__ASSERT(true, "Wrong maximum packet size value");
		return 0;
	}
}

static struct pic32ck_ep_buffer_desc *pic32ck_get_ebd(const struct device *dev, const uint8_t ep)
{
	const struct udc_pic32ck_fs_config *config = dev->config;

	return &config->bdt[USB_EP_GET_IDX(ep)];
}

static usb_device_endpoint_registers_t *pic32ck_get_ep_reg(const struct device *dev,
							   const uint8_t ep)
{
	const struct udc_pic32ck_fs_config *config = dev->config;
	usb_device_registers_t *const base = config->base;

	return &base->DEVICE_ENDPOINT[USB_EP_GET_IDX(ep)];
}

static int pic32ck_prep_out(const struct device *dev,
			    struct net_buf *const buf, struct udc_ep_config *const ep_cfg)
{
	usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, ep_cfg->addr);
	struct pic32ck_ep_buffer_desc *const bd = pic32ck_get_ebd(dev, ep_cfg->addr);
	const uint16_t size = MIN(16383U, net_buf_tailroom(buf));
	unsigned int lock_key;

	if (!(endpoint->USB_EPSTATUS & USB_DEVICE_EPSTATUS_BK0RDY_Msk)) {
		LOG_ERR("ep 0x%02x buffer is used by the controller", ep_cfg->addr);
		return -EBUSY;
	}

	lock_key = irq_lock();
	if (ep_cfg->addr != USB_CONTROL_EP_OUT) {
		bd->bank0.addr = (uintptr_t)buf->data;
		bd->bank0.byte_count = 0;

		bd->bank0.multi_packet_size = size;
		bd->bank0.size = pic32ck_get_bd_size(udc_mps_ep_size(ep_cfg));
	}

	endpoint->USB_EPSTATUSCLR = USB_DEVICE_EPSTATUSCLR_BK0RDY_Msk;
	irq_unlock(lock_key);

	LOG_DBG("Prepare OUT ep 0x%02x size %u", ep_cfg->addr, size);

	return 0;
}

static int pic32ck_prep_in(const struct device *dev,
			   struct net_buf *const buf, struct udc_ep_config *const ep_cfg)
{
	usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, ep_cfg->addr);
	struct pic32ck_ep_buffer_desc *const bd = pic32ck_get_ebd(dev, ep_cfg->addr);
	const uint16_t len = MIN(16383U, buf->len);
	unsigned int lock_key;

	if (endpoint->USB_EPSTATUS & USB_DEVICE_EPSTATUS_BK1RDY_Msk) {
		LOG_ERR("ep 0x%02x buffer is used by the controller", ep_cfg->addr);
		return -EAGAIN;
	}

	lock_key = irq_lock();

	bd->bank1.addr = (uintptr_t)buf->data;
	bd->bank1.size = pic32ck_get_bd_size(udc_mps_ep_size(ep_cfg));

	bd->bank1.multi_packet_size = 0;
	bd->bank1.byte_count = len;
	bd->bank1.auto_zlp = 0;

	endpoint->USB_EPSTATUSSET = USB_DEVICE_EPSTATUSSET_BK1RDY_Msk;
	irq_unlock(lock_key);

	LOG_DBG("Prepare IN ep 0x%02x length %u", ep_cfg->addr, len);

	return 0;
}

static int pic32ck_handle_evt_finished(const struct device *dev,
				       struct udc_ep_config *const ep_cfg)
{
	struct net_buf *buf;

	buf = udc_buf_get(ep_cfg);
	if (buf == NULL) {
		LOG_ERR("No buffer for ep 0x%02x", ep_cfg->addr);
		return -ENOBUFS;
	}

	udc_ep_set_busy(ep_cfg, false);

	return udc_submit_ep_event(dev, buf, 0);
}

static inline int pic32ck_handle_evt_dout(const struct device *dev,
					  struct udc_ep_config *const ep_cfg)
{
	struct net_buf *buf;

	buf = udc_buf_get(ep_cfg);
	if (buf == NULL) {
		LOG_ERR("No buffer for OUT ep 0x%02x", ep_cfg->addr);
		return -ENODATA;
	}

	udc_ep_set_busy(ep_cfg, false);

	return udc_submit_ep_event(dev, buf, 0);
}

static void pic32ck_handle_xfer_next(const struct device *dev,
				     struct udc_ep_config *const ep_cfg)
{
	struct net_buf *buf;
	int err;

	buf = udc_buf_peek(ep_cfg);
	if (buf == NULL) {
		return;
	}

	if (ep_cfg->addr == USB_CONTROL_EP_OUT) {
		struct udc_buf_info *bi = udc_get_buf_info(buf);

		if (bi->setup) {
			/* SETUP data will be received without any action */
			return;
		}
	}

	if (USB_EP_DIR_IS_OUT(ep_cfg->addr)) {
		err = pic32ck_prep_out(dev, buf, ep_cfg);
	} else {
		err = pic32ck_prep_in(dev, buf, ep_cfg);
	}

	if (err != 0) {
		buf = udc_buf_get(ep_cfg);
		udc_submit_ep_event(dev, buf, -ECONNREFUSED);
	} else {
		udc_ep_set_busy(ep_cfg, true);
	}
}

static ALWAYS_INLINE void pic32ck_thread_handler(const struct device *const dev)
{
	struct udc_pic32ck_fs_data *const priv = udc_get_private(dev);
	struct udc_ep_config *ep_cfg;
	uint32_t evt;
	uint32_t eps;
	uint8_t ep;
	int err;

	evt = k_event_wait(&priv->events, UINT32_MAX, false, K_FOREVER);
	udc_lock_internal(dev, K_FOREVER);

	if (evt & BIT(PIC32CK_EVT_XFER_FINISHED)) {
		k_event_clear(&priv->events, BIT(PIC32CK_EVT_XFER_FINISHED));

		eps = atomic_clear(&priv->xfer_finished);

		while (eps) {
			ep = udc_pull_ep_from_bmsk(&eps);
			ep_cfg = udc_get_ep_cfg(dev, ep);
			LOG_DBG("Finished event ep 0x%02x", ep);

			err = pic32ck_handle_evt_finished(dev, ep_cfg);
			if (err) {
				udc_submit_event(dev, UDC_EVT_ERROR, err);
			}

			if (!udc_ep_is_busy(ep_cfg)) {
				pic32ck_handle_xfer_next(dev, ep_cfg);
			} else {
				LOG_ERR("Endpoint 0x%02x busy", ep);
			}
		}
	}

	if (evt & BIT(PIC32CK_EVT_XFER_NEW)) {
		k_event_clear(&priv->events, BIT(PIC32CK_EVT_XFER_NEW));

		eps = atomic_clear(&priv->xfer_new);

		while (eps) {
			ep = udc_pull_ep_from_bmsk(&eps);
			ep_cfg = udc_get_ep_cfg(dev, ep);
			LOG_INF("New transfer ep 0x%02x in the queue", ep);

			if (!udc_ep_is_busy(ep_cfg)) {
				pic32ck_handle_xfer_next(dev, ep_cfg);
			} else {
				LOG_ERR("Endpoint 0x%02x busy", ep);
			}
		}
	}

	if (evt & BIT(PIC32CK_EVT_SETUP)) {
		k_event_clear(&priv->events, BIT(PIC32CK_EVT_SETUP));
		/*
		 * BK0RDY bit is set and BK1RDY bit is cleared on receiving the
		 * setup packet, which cancels ongoing transfer and NAKs any
		 * OUT/IN data.
		 */
		udc_setup_received(dev, priv->setup);
	}

	udc_unlock_internal(dev);
}

static void pic32ck_handle_setup_isr(const struct device *dev)
{
	struct pic32ck_ep_buffer_desc *const bd = pic32ck_get_ebd(dev, 0);
	struct udc_pic32ck_fs_data *const priv = udc_get_private(dev);

	if (bd->bank0.byte_count != 8) {
		LOG_ERR("Wrong byte count %u for setup packet",
			bd->bank0.byte_count);
	}

	memcpy(priv->setup, priv->ctrl_out_buf, sizeof(priv->setup));
	k_event_post(&priv->events, BIT(PIC32CK_EVT_SETUP));
}

static void pic32ck_handle_out_isr(const struct device *dev, const uint8_t ep)
{
	struct pic32ck_ep_buffer_desc *const bd = pic32ck_get_ebd(dev, ep);
	usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, ep);
	struct udc_pic32ck_fs_data *const priv = udc_get_private(dev);
	struct udc_ep_config *ep_cfg = udc_get_ep_cfg(dev, ep);
	struct net_buf *buf;
	uint32_t size;

	buf = udc_buf_peek(ep_cfg);
	if (buf == NULL) {
		LOG_ERR("No buffer for ep 0x%02x", ep);
		udc_submit_event(dev, UDC_EVT_ERROR, -ENOBUFS);
		return;
	}

	LOG_DBG("ISR ep 0x%02x byte_count %u room %u mps %u",
		ep, bd->bank0.byte_count, net_buf_tailroom(buf), udc_mps_ep_size(ep_cfg));

	size = MIN(bd->bank0.byte_count, net_buf_tailroom(buf));
	if (ep == USB_CONTROL_EP_OUT) {
		net_buf_add_mem(buf, priv->ctrl_out_buf, size);
	} else {
		net_buf_add(buf, size);
	}

	/*
	 * The remaining buffer size should actually be at least equal to MPS,
	 * if (net_buf_tailroom(buf) >= udc_mps_ep_size(ep_cfg) && ...,
	 * otherwise the controller may write outside the buffer, this must be
	 * fixed in the UDC buffer allocation.
	 */
	if (net_buf_tailroom(buf) && size == udc_mps_ep_size(ep_cfg)) {
		__maybe_unused int err;

		if (ep == USB_CONTROL_EP_OUT) {
			/* This is the same as pic32ck_prep_out() would do for the
			 * control OUT endpoint, but shorter.
			 */
			endpoint->USB_EPSTATUSCLR = USB_DEVICE_EPSTATUSCLR_BK0RDY_Msk;
		} else {
			err = pic32ck_prep_out(dev, buf, ep_cfg);
			__ASSERT(err == 0, "Failed to start new OUT transaction");
		}
	} else {
		atomic_set_bit(&priv->xfer_finished, udc_ep_to_bnum(ep));
		k_event_post(&priv->events, BIT(PIC32CK_EVT_XFER_FINISHED));
	}
}

static void pic32ck_handle_in_isr(const struct device *dev, const uint8_t ep)
{
	struct pic32ck_ep_buffer_desc *const bd = pic32ck_get_ebd(dev, ep);
	struct udc_pic32ck_fs_data *const priv = udc_get_private(dev);
	struct udc_ep_config *ep_cfg = udc_get_ep_cfg(dev, ep);
	__maybe_unused int err = 0;
	struct net_buf *buf;
	uint32_t len;

	buf = udc_buf_peek(ep_cfg);
	if (buf == NULL) {
		LOG_ERR("No buffer for ep 0x%02x", ep);
		udc_submit_event(dev, UDC_EVT_ERROR, -ENOBUFS);
		return;
	}

	len = bd->bank1.byte_count;
	LOG_DBG("ISR ep 0x%02x byte_count %u", ep, len);
	net_buf_pull(buf, len);

	if (buf->len) {
		err = pic32ck_prep_in(dev, buf, ep_cfg);
		__ASSERT(err == 0, "Failed to start new IN transaction");
	} else {
		if (udc_ep_buf_has_zlp(buf)) {
			err = pic32ck_prep_in(dev, buf, ep_cfg);
			__ASSERT(err == 0, "Failed to start new IN transaction");
			udc_ep_buf_clear_zlp(buf);
			return;
		}

		atomic_set_bit(&priv->xfer_finished, udc_ep_to_bnum(ep));
		k_event_post(&priv->events, BIT(PIC32CK_EVT_XFER_FINISHED));
	}
}

static void ALWAYS_INLINE handle_ep_isr(const struct device *dev, const uint8_t idx)
{
	usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, idx);
	uint32_t intflag;

	intflag = endpoint->USB_EPINTFLAG;
	/* Clear endpoint interrupt flags */
	endpoint->USB_EPINTFLAG = intflag;

	if (intflag & USB_DEVICE_EPINTFLAG_TRCPT1_Msk) {
		pic32ck_handle_in_isr(dev, idx | USB_EP_DIR_IN);
	}

	if (intflag & USB_DEVICE_EPINTFLAG_TRCPT0_Msk) {
		pic32ck_handle_out_isr(dev, idx);
	}

	if (intflag & USB_DEVICE_EPINTFLAG_RXSTP_Msk) {
		pic32ck_handle_setup_isr(dev);
	}
}

static void pic32ck_isr_handler(const struct device *dev)
{
	const struct udc_pic32ck_fs_config *config = dev->config;
	usb_device_registers_t *const base = config->base;
	uint32_t epintsmry = base->USB_EPINTSMRY;
	uint32_t intflag;

	/* Check endpoint interrupts bit-by-bit */
	for (uint8_t idx = 0U; epintsmry != 0U; epintsmry >>= 1) {
		if ((epintsmry & 1) != 0U) {
			handle_ep_isr(dev, idx);
		}

		idx++;
	}

	intflag = base->USB_INTFLAG;
	/* Clear interrupt flags */
	base->USB_INTFLAG = intflag;

	if (intflag & USB_DEVICE_INTFLAG_SOF_Msk) {
		udc_submit_sof_event(dev);
	}

	if (intflag & USB_DEVICE_INTFLAG_EORST_Msk) {
		usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, 0);

		/* Re-enable control endpoint interrupts */
		endpoint->USB_EPINTENSET = USB_DEVICE_EPINTENSET_TRCPT0_Msk |
					   USB_DEVICE_EPINTENSET_TRCPT1_Msk |
					   USB_DEVICE_EPINTENSET_RXSTP_Msk;

		udc_submit_event(dev, UDC_EVT_RESET, 0);
	}

	if (intflag & USB_DEVICE_INTFLAG_SUSPEND_Msk) {
		if (!udc_is_suspended(dev)) {
			udc_set_suspended(dev, true);
			udc_submit_event(dev, UDC_EVT_SUSPEND, 0);
		}
	}

	if (intflag & USB_DEVICE_INTFLAG_EORSM_Msk) {
		if (udc_is_suspended(dev)) {
			udc_set_suspended(dev, false);
			udc_submit_event(dev, UDC_EVT_RESUME, 0);
		}
	}

	/*
	 * This controller does not support VBUS status detection. To work
	 * smoothly, we should consider whether it would be possible to use the
	 * GPIO pin for VBUS state detection.
	 */

	if (intflag & USB_DEVICE_INTFLAG_RAMACER_Msk) {
		udc_submit_event(dev, UDC_EVT_ERROR, -EINVAL);
	}
}

static int udc_pic32ck_fs_ep_enqueue(const struct device *dev,
				     struct udc_ep_config *const ep_cfg, struct net_buf *buf)
{
	struct udc_pic32ck_fs_data *const priv = udc_get_private(dev);

	LOG_DBG("%s enqueue 0x%02x %p", dev->name, ep_cfg->addr, (void *)buf);
	udc_buf_put(ep_cfg, buf);

	if (!ep_cfg->stat.halted) {
		atomic_set_bit(&priv->xfer_new, udc_ep_to_bnum(ep_cfg->addr));
		k_event_post(&priv->events, BIT(PIC32CK_EVT_XFER_NEW));
	}

	return 0;
}

static int udc_pic32ck_fs_ep_dequeue(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, ep_cfg->addr);
	unsigned int lock_key;

	lock_key = irq_lock();

	if (USB_EP_DIR_IS_IN(ep_cfg->addr)) {
		endpoint->USB_EPSTATUSCLR = USB_DEVICE_EPSTATUSCLR_BK1RDY_Msk;
	} else {
		endpoint->USB_EPSTATUSSET = USB_DEVICE_EPSTATUSSET_BK0RDY_Msk;
	}

	udc_ep_cancel_queued(dev, ep_cfg);

	udc_ep_set_busy(ep_cfg, false);

	irq_unlock(lock_key);

	return 0;
}

static void setup_control_out_ep(const struct device *dev)
{
	struct pic32ck_ep_buffer_desc *const bd = pic32ck_get_ebd(dev, 0);
	struct udc_pic32ck_fs_data *const priv = udc_get_private(dev);

	/* It will never be reassigned to anything else during device runtime. */
	bd->bank0.addr = (uintptr_t)priv->ctrl_out_buf;
	bd->bank0.multi_packet_size = 0;
	bd->bank0.size = pic32ck_get_bd_size(64);
	bd->bank0.auto_zlp = 0;
}

static int udc_pic32ck_fs_ep_enable(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, ep_cfg->addr);
	uint8_t type;

	switch (ep_cfg->attributes & USB_EP_TRANSFER_TYPE_MASK) {
	case USB_EP_TYPE_CONTROL:
		type = 1;
		break;
	case USB_EP_TYPE_ISO:
		type = 2;
		break;
	case USB_EP_TYPE_BULK:
		type = 3;
		break;
	case USB_EP_TYPE_INTERRUPT:
		type = 4;
		break;
	default:
		return -EINVAL;
	}

	if (ep_cfg->addr == USB_CONTROL_EP_OUT) {
		setup_control_out_ep(dev);
		endpoint->USB_EPINTENSET = USB_DEVICE_EPINTENSET_RXSTP_Msk;
	}

	if (USB_EP_DIR_IS_IN(ep_cfg->addr)) {
		endpoint->USB_EPCFG = (endpoint->USB_EPCFG & ~USB_DEVICE_EPCFG_EPTYPE1_Msk) |
				      USB_DEVICE_EPCFG_EPTYPE1(type);
		endpoint->USB_EPSTATUSCLR = USB_DEVICE_EPSTATUSCLR_BK1RDY_Msk;
		endpoint->USB_EPINTENSET = USB_DEVICE_EPINTENSET_TRCPT1_Msk;
	} else {
		endpoint->USB_EPCFG = (endpoint->USB_EPCFG & ~USB_DEVICE_EPCFG_EPTYPE0_Msk) |
				      USB_DEVICE_EPCFG_EPTYPE0(type);
		endpoint->USB_EPSTATUSSET = USB_DEVICE_EPSTATUSSET_BK0RDY_Msk;
		endpoint->USB_EPINTENSET = USB_DEVICE_EPINTENSET_TRCPT0_Msk;
	}

	LOG_DBG("Enable ep 0x%02x", ep_cfg->addr);

	return 0;
}

static int udc_pic32ck_fs_ep_disable(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, ep_cfg->addr);

	if (ep_cfg->addr == USB_CONTROL_EP_OUT) {
		endpoint->USB_EPINTENCLR = USB_DEVICE_EPINTENCLR_RXSTP_Msk;
	}

	if (USB_EP_DIR_IS_IN(ep_cfg->addr)) {
		endpoint->USB_EPINTENCLR = USB_DEVICE_EPINTENCLR_TRCPT1_Msk;
		endpoint->USB_EPCFG = (endpoint->USB_EPCFG & ~USB_DEVICE_EPCFG_EPTYPE1_Msk) |
				      USB_DEVICE_EPCFG_EPTYPE1(0);
	} else {
		endpoint->USB_EPINTENCLR = USB_DEVICE_EPINTENCLR_TRCPT0_Msk;
		endpoint->USB_EPCFG = (endpoint->USB_EPCFG & ~USB_DEVICE_EPCFG_EPTYPE0_Msk) |
				      USB_DEVICE_EPCFG_EPTYPE0(0);
	}

	LOG_DBG("Disable ep 0x%02x", ep_cfg->addr);

	return 0;
}

static int udc_pic32ck_fs_ep_set_halt(const struct device *dev,
				      struct udc_ep_config *const ep_cfg)
{
	usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, ep_cfg->addr);

	if (USB_EP_DIR_IS_IN(ep_cfg->addr)) {
		endpoint->USB_EPSTATUSSET = USB_DEVICE_EPSTATUSSET_STALLRQ1_Msk;
	} else {
		endpoint->USB_EPSTATUSSET = USB_DEVICE_EPSTATUSSET_STALLRQ0_Msk;
	}

	LOG_DBG("Set halt ep 0x%02x", ep_cfg->addr);
	if (USB_EP_GET_IDX(ep_cfg->addr) != 0) {
		ep_cfg->stat.halted = true;
	}

	return 0;
}

static int udc_pic32ck_fs_ep_clear_halt(const struct device *dev,
					struct udc_ep_config *const ep_cfg)
{
	usb_device_endpoint_registers_t *const endpoint = pic32ck_get_ep_reg(dev, ep_cfg->addr);
	struct udc_pic32ck_fs_data *const priv = udc_get_private(dev);

	if (USB_EP_GET_IDX(ep_cfg->addr) == 0) {
		return 0;
	}

	if (USB_EP_DIR_IS_IN(ep_cfg->addr)) {
		endpoint->USB_EPSTATUSCLR = USB_DEVICE_EPSTATUSCLR_STALLRQ1_Msk;
		endpoint->USB_EPSTATUSCLR = USB_DEVICE_EPSTATUSCLR_DTGLIN_Msk;
	} else {
		endpoint->USB_EPSTATUSCLR = USB_DEVICE_EPSTATUSCLR_STALLRQ0_Msk;
		endpoint->USB_EPSTATUSCLR = USB_DEVICE_EPSTATUSCLR_DTGLOUT_Msk;
	}

	if (USB_EP_GET_IDX(ep_cfg->addr) != 0 && !udc_ep_is_busy(ep_cfg)) {
		if (udc_buf_peek(ep_cfg)) {
			atomic_set_bit(&priv->xfer_new, udc_ep_to_bnum(ep_cfg->addr));
			k_event_post(&priv->events, BIT(PIC32CK_EVT_XFER_NEW));
		}
	}

	LOG_DBG("Clear halt ep 0x%02x", ep_cfg->addr);
	ep_cfg->stat.halted = false;

	return 0;
}

static int udc_pic32ck_fs_set_address(const struct device *dev, const uint8_t addr)
{
	const struct udc_pic32ck_fs_config *config = dev->config;
	usb_device_registers_t *const base = config->base;

	LOG_DBG("Set new address %u for %s", addr, dev->name);
	if (addr != 0) {
		base->USB_DADD = addr | USB_DEVICE_DADD_ADDEN_Msk;
	} else {
		base->USB_DADD = 0;
	}

	return 0;
}

static int udc_pic32ck_fs_host_wakeup(const struct device *dev)
{
	const struct udc_pic32ck_fs_config *config = dev->config;
	usb_device_registers_t *const base = config->base;

	LOG_DBG("Remote wakeup from %s", dev->name);
	base->USB_CTRLB |= USB_DEVICE_CTRLB_UPRSM_Msk;

	return 0;
}

static enum udc_bus_speed udc_pic32ck_fs_device_speed(const struct device *dev)
{
	/* PIC32CK FS is always full-speed only */
	return UDC_BUS_SPEED_FS;
}

static int udc_pic32ck_fs_enable(const struct device *dev)
{
	const struct udc_pic32ck_fs_config *config = dev->config;
	const struct pinctrl_dev_config *const pcfg = config->pcfg;
	usb_device_registers_t *const base = config->base;
	int ret;

	/*
	 * Enable USB clocks directly via MCLK and GCLK registers.
	 * MCLK base = 0x44012000, GCLK base = 0x44010000
	 * USB_MCLK_ID_AHB = 19 → CLKMSK[0] bit 19
	 * USB_MCLK_ID_APB = 113 → CLKMSK[3] bit 17
	 * USB_GCLK_ID = 46 → PCHCTRL[46], source = GEN0 (48MHz DFLL)
	 */
	{
		volatile uint32_t *mclk_clkmsk0 = (volatile uint32_t *)(0x44012000 + 0x3C);
		volatile uint32_t *mclk_clkmsk3 = (volatile uint32_t *)(0x44012000 + 0x48);
		volatile uint32_t *gclk_pchctrl46 = (volatile uint32_t *)(0x44010000 + 0x80 + 46 * 4);

		/* Enable USB AHB clock: CLKMSK[0] bit 19 */
		*mclk_clkmsk0 |= (1U << 19);
		/* Enable USB APB clock: CLKMSK[3] bit 17 */
		*mclk_clkmsk3 |= (1U << 17);
		/* Enable GCLK for USB: GEN0 (48MHz) + CHEN */
		*gclk_pchctrl46 = (1U << 0) | (1U << 6);  /* GEN=1(48MHz DFLL), CHEN=1 */
	}

	/* Reset controller */
	base->USB_CTRLA = USB_CTRLA_SWRST_Msk;
	pic32ck_wait_syncbusy(dev);

	/*
	 * Change QOS values to have the best performance and correct USB
	 * behaviour.
	 */
	base->USB_QOSCTRL = (base->USB_QOSCTRL & ~USB_QOSCTRL_CQOS_Msk) | USB_QOSCTRL_CQOS(2);
	base->USB_QOSCTRL = (base->USB_QOSCTRL & ~USB_QOSCTRL_DQOS_Msk) | USB_QOSCTRL_DQOS(2);

	if (pcfg != NULL) {
		ret = pinctrl_apply_state(pcfg, PINCTRL_STATE_DEFAULT);
		if (ret) {
			LOG_ERR("Failed to apply default pinctrl state (%d)", ret);
			return ret;
		}
	}

	pic32ck_load_padcal(dev);

	base->USB_CTRLA = USB_CTRLA_MODE_DEVICE | USB_CTRLA_RUNSTDBY_Msk;
	base->USB_CTRLB = USB_DEVICE_CTRLB_SPDCONF_FS | USB_DEVICE_CTRLB_DETACH_Msk;

	base->USB_DESCADD = (uint32_t)(uintptr_t)config->bdt;

	if (udc_ep_enable_internal(dev, USB_CONTROL_EP_OUT,
				   USB_EP_TYPE_CONTROL, 64, 0)) {
		LOG_ERR("Failed to enable control endpoint");
		return -EIO;
	}

	if (udc_ep_enable_internal(dev, USB_CONTROL_EP_IN,
				   USB_EP_TYPE_CONTROL, 64, 0)) {
		LOG_ERR("Failed to enable control endpoint");
		return -EIO;
	}

	base->USB_INTENSET = USB_DEVICE_INTENSET_EORSM_Msk |
			     USB_DEVICE_INTENSET_EORST_Msk |
			     USB_DEVICE_INTENSET_SUSPEND_Msk;

	base->USB_CTRLA |= USB_CTRLA_ENABLE_Msk;
	pic32ck_wait_syncbusy(dev);
	base->USB_CTRLB &= ~USB_DEVICE_CTRLB_DETACH_Msk;

	config->irq_enable_func(dev);
	LOG_DBG("Enable device %s", dev->name);

	return 0;
}

static int udc_pic32ck_fs_disable(const struct device *dev)
{
	const struct udc_pic32ck_fs_config *config = dev->config;
	usb_device_registers_t *const base = config->base;

	config->irq_disable_func(dev);
	base->USB_CTRLB |= USB_DEVICE_CTRLB_DETACH_Msk;
	base->USB_CTRLA &= ~USB_CTRLA_ENABLE_Msk;
	pic32ck_wait_syncbusy(dev);

	if (udc_ep_disable_internal(dev, USB_CONTROL_EP_OUT)) {
		LOG_ERR("Failed to disable control endpoint");
		return -EIO;
	}

	if (udc_ep_disable_internal(dev, USB_CONTROL_EP_IN)) {
		LOG_ERR("Failed to disable control endpoint");
		return -EIO;
	}

	/* Clock disable is handled by the clock driver */

	LOG_DBG("Disable device %s", dev->name);

	return 0;
}

/*
 * Nothing to do here as the controller does not support VBUS state change
 * detection and there is nothing to initialize in the controller to do this.
 */
static int udc_pic32ck_fs_init(const struct device *dev)
{
	LOG_DBG("Init device %s", dev->name);

	return 0;
}

static int udc_pic32ck_fs_shutdown(const struct device *dev)
{
	LOG_DBG("Shutdown device %s", dev->name);

	return 0;
}

static int udc_pic32ck_fs_driver_preinit(const struct device *dev)
{
	const struct udc_pic32ck_fs_config *config = dev->config;
	struct udc_pic32ck_fs_data *priv = udc_get_private(dev);
	struct udc_data *data = dev->data;
	uint16_t mps = UDC_PIC32CK_FS_EP_MPS;
	int err;

	k_mutex_init(&data->mutex);
	k_event_init(&priv->events);
	atomic_clear(&priv->xfer_new);
	atomic_clear(&priv->xfer_finished);

	data->caps.rwup = true;
	data->caps.mps0 = UDC_MPS0_64;

	for (int i = 0; i < config->num_of_eps; i++) {
		config->ep_cfg_out[i].caps.out = 1;
		if (i == 0) {
			config->ep_cfg_out[i].caps.control = 1;
			config->ep_cfg_out[i].caps.mps = 64;
		} else {
			config->ep_cfg_out[i].caps.bulk = 1;
			config->ep_cfg_out[i].caps.interrupt = 1;
			config->ep_cfg_out[i].caps.iso = 1;
			config->ep_cfg_out[i].caps.mps = mps;
		}

		config->ep_cfg_out[i].addr = USB_EP_DIR_OUT | i;
		err = udc_register_ep(dev, &config->ep_cfg_out[i]);
		if (err != 0) {
			LOG_ERR("Failed to register endpoint");
			return err;
		}
	}

	for (int i = 0; i < config->num_of_eps; i++) {
		config->ep_cfg_in[i].caps.in = 1;
		if (i == 0) {
			config->ep_cfg_in[i].caps.control = 1;
			config->ep_cfg_in[i].caps.mps = 64;
		} else {
			config->ep_cfg_in[i].caps.bulk = 1;
			config->ep_cfg_in[i].caps.interrupt = 1;
			config->ep_cfg_in[i].caps.iso = 1;
			config->ep_cfg_in[i].caps.mps = mps;
		}

		config->ep_cfg_in[i].addr = USB_EP_DIR_IN | i;
		err = udc_register_ep(dev, &config->ep_cfg_in[i]);
		if (err != 0) {
			LOG_ERR("Failed to register endpoint");
			return err;
		}
	}

	config->make_thread(dev);

	return 0;
}

static void udc_pic32ck_fs_lock(const struct device *dev)
{
	k_sched_lock();
	udc_lock_internal(dev, K_FOREVER);
}

static void udc_pic32ck_fs_unlock(const struct device *dev)
{
	udc_unlock_internal(dev);
	k_sched_unlock();
}

static const struct udc_api udc_pic32ck_fs_api = {
	.lock = udc_pic32ck_fs_lock,
	.unlock = udc_pic32ck_fs_unlock,
	.device_speed = udc_pic32ck_fs_device_speed,
	.init = udc_pic32ck_fs_init,
	.enable = udc_pic32ck_fs_enable,
	.disable = udc_pic32ck_fs_disable,
	.shutdown = udc_pic32ck_fs_shutdown,
	.set_address = udc_pic32ck_fs_set_address,
	.host_wakeup = udc_pic32ck_fs_host_wakeup,
	.ep_enable = udc_pic32ck_fs_ep_enable,
	.ep_disable = udc_pic32ck_fs_ep_disable,
	.ep_set_halt = udc_pic32ck_fs_ep_set_halt,
	.ep_clear_halt = udc_pic32ck_fs_ep_clear_halt,
	.ep_enqueue = udc_pic32ck_fs_ep_enqueue,
	.ep_dequeue = udc_pic32ck_fs_ep_dequeue,
};

#define DT_DRV_COMPAT microchip_pic32ck_usb_fs

#define UDC_PIC32CK_FS_IRQ_ENABLE(i, n)					\
	IRQ_CONNECT(DT_INST_IRQ_BY_IDX(n, i, irq),				\
		    DT_INST_IRQ_BY_IDX(n, i, priority),				\
		    pic32ck_isr_handler, DEVICE_DT_INST_GET(n), 0);		\
	irq_enable(DT_INST_IRQ_BY_IDX(n, i, irq));

#define UDC_PIC32CK_FS_IRQ_DISABLE(i, n)					\
	irq_disable(DT_INST_IRQ_BY_IDX(n, i, irq));

#define UDC_PIC32CK_FS_IRQ_ENABLE_DEFINE(i, n)					\
static void udc_pic32ck_fs_irq_enable_func_##n(const struct device *dev)	\
{										\
	LISTIFY(DT_INST_NUM_IRQS(n), UDC_PIC32CK_FS_IRQ_ENABLE, (), n)		\
}

#define UDC_PIC32CK_FS_IRQ_DISABLE_DEFINE(i, n)					\
static void udc_pic32ck_fs_irq_disable_func_##n(const struct device *dev)	\
{										\
	LISTIFY(DT_INST_NUM_IRQS(n), UDC_PIC32CK_FS_IRQ_DISABLE, (), n)	\
}

#define UDC_PIC32CK_FS_PINCTRL_DT_INST_DEFINE(n)				\
	COND_CODE_1(DT_INST_PINCTRL_HAS_NAME(n, default),			\
		    (PINCTRL_DT_INST_DEFINE(n)), ())

#define UDC_PIC32CK_FS_PINCTRL_DT_INST_DEV_CONFIG_GET(n)			\
	COND_CODE_1(DT_INST_PINCTRL_HAS_NAME(n, default),			\
		    ((void *)PINCTRL_DT_INST_DEV_CONFIG_GET(n)), (NULL))

#define UDC_PIC32CK_FS_DEVICE_DEFINE(n)						\
	UDC_PIC32CK_FS_PINCTRL_DT_INST_DEFINE(n);				\
	UDC_PIC32CK_FS_IRQ_ENABLE_DEFINE(i, n);				\
	UDC_PIC32CK_FS_IRQ_DISABLE_DEFINE(i, n);				\
										\
	K_THREAD_STACK_DEFINE(udc_pic32ck_fs_stack_##n,				\
			      CONFIG_UDC_PIC32CK_FS_STACK_SIZE);		\
										\
	static __aligned(sizeof(void *)) struct pic32ck_ep_buffer_desc		\
		pic32ck_bdt_##n[DT_INST_PROP(n, num_bidir_endpoints)];		\
										\
	static void udc_pic32ck_fs_thread_##n(void *dev, void *arg1, void *arg2)\
	{									\
		while (true) {							\
			pic32ck_thread_handler(dev);				\
		}								\
	}									\
										\
	static void udc_pic32ck_fs_make_thread_##n(const struct device *dev)	\
	{									\
		struct udc_pic32ck_fs_data *priv = udc_get_private(dev);	\
										\
		k_thread_create(&priv->thread_data,				\
				udc_pic32ck_fs_stack_##n,			\
				K_THREAD_STACK_SIZEOF(udc_pic32ck_fs_stack_##n),\
				udc_pic32ck_fs_thread_##n,			\
				(void *)dev, NULL, NULL,			\
				K_PRIO_COOP(CONFIG_UDC_PIC32CK_FS_THREAD_PRIORITY),\
				K_ESSENTIAL,					\
				K_NO_WAIT);					\
		k_thread_name_set(&priv->thread_data, dev->name);		\
	}									\
										\
	static struct udc_ep_config						\
		ep_cfg_out[DT_INST_PROP(n, num_bidir_endpoints)];		\
	static struct udc_ep_config						\
		ep_cfg_in[DT_INST_PROP(n, num_bidir_endpoints)];		\
										\
	static const struct udc_pic32ck_fs_config udc_pic32ck_fs_config_##n = {	\
		.base = (usb_device_registers_t *)DT_INST_REG_ADDR(n),		\
		.bdt = pic32ck_bdt_##n,						\
		.num_of_eps = DT_INST_PROP(n, num_bidir_endpoints),		\
		.ep_cfg_in = ep_cfg_out,					\
		.ep_cfg_out = ep_cfg_in,					\
		.irq_enable_func = udc_pic32ck_fs_irq_enable_func_##n,		\
		.irq_disable_func = udc_pic32ck_fs_irq_disable_func_##n,	\
		.pcfg = UDC_PIC32CK_FS_PINCTRL_DT_INST_DEV_CONFIG_GET(n),	\
		.make_thread = udc_pic32ck_fs_make_thread_##n,			\
	};									\
										\
	static struct udc_pic32ck_fs_data udc_priv_##n = {			\
	};									\
										\
	static struct udc_data udc_data_##n = {					\
		.mutex = Z_MUTEX_INITIALIZER(udc_data_##n.mutex),		\
		.priv = &udc_priv_##n,						\
	};									\
										\
	DEVICE_DT_INST_DEFINE(n, udc_pic32ck_fs_driver_preinit, NULL,		\
			      &udc_data_##n, &udc_pic32ck_fs_config_##n,	\
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,	\
			      &udc_pic32ck_fs_api);

DT_INST_FOREACH_STATUS_OKAY(UDC_PIC32CK_FS_DEVICE_DEFINE)
