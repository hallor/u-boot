/*
 * Qualcomm UART driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * UART will work in Data Mover mode.
 * Based on Linux driver.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <asm/arch/sysmap.h>
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <linux/compiler.h>
#include <serial.h>
#include <watchdog.h>

/* Serial registers - this driver works in uartdm mode*/

#define UARTDM_DMRX             0x34 /* Max RX transfer length */
#define UARTDM_NCF_TX           0x40 /* Number of chars to TX */

#define UARTDM_RXFS             0x50 /* RX channel status register */
#define UARTDM_RXFS_BUF_SHIFT   0x7  /* Number of bytes in the packing buffer */
#define UARTDM_RXFS_BUF_MASK    0x7

#define UARTDM_SR                0xA4 /* Status register */
#define UARTDM_SR_RX_READY       (1 << 0) /* Word is the receiver FIFO */
#define UARTDM_SR_TX_EMPTY       (1 << 3) /* Transmitter underrun */
#define UARTDM_SR_UART_OVERRUN   (1 << 4) /* Receive overrun */

#define UARTDM_CR                         0xA8 /* Command register */
#define UARTDM_CR_CMD_RESET_ERR           (3 << 4) /* Clear overrun error */
#define UARTDM_CR_CMD_RESET_STALE_INT     (8 << 4) /* Clears stale irq */
#define UARTDM_CR_CMD_RESET_TX_READY      (3 << 8) /* Clears TX Ready irq*/
#define UARTDM_CR_CMD_FORCE_STALE         (4 << 8) /* Causes stale event */
#define UARTDM_CR_CMD_STALE_EVENT_DISABLE (6 << 8) /* Disable stale event */

#define UARTDM_IMR                0xB0 /* Interrupt mask register */
#define UARTDM_ISR                0xB4 /* Interrupt status register */
#define UARTDM_ISR_TX_READY       0x80 /* TX FIFO empty */

#define UARTDM_TF               0x100 /* UART Transmit FIFO register */
#define UARTDM_RF               0x140 /* UART Receive FIFO register */


DECLARE_GLOBAL_DATA_PTR;

struct msm_serial_data {
	phys_addr_t base;
	unsigned chars_cnt; /* number of buffered chars */
	uint32_t chars_buf; /* buffered chars */
};

static int msm_serial_getc(struct udevice *dev)
{
	struct msm_serial_data *p = dev_get_priv(dev);
	unsigned sr;
	char c;

	/* There was something buffered */
	if (p->chars_cnt) {
		c = p->chars_buf & 0xFF;
		p->chars_buf >>= 8;
		p->chars_cnt--;
		return c;
	}

	/* Clear error in case of buffer overrun */
	if (readl(p->base + UARTDM_SR) & UARTDM_SR_UART_OVERRUN)
		writel(UARTDM_CR_CMD_RESET_ERR, p->base + UARTDM_CR);

	/* We need to fetch new character */
	sr = readl(p->base + UARTDM_SR);

	/* There are at least 4 bytes in fifo */
	if (sr & UARTDM_SR_RX_READY) {
		p->chars_buf = readl(p->base + UARTDM_RF);
		c = p->chars_buf & 0xFF;
		p->chars_cnt = 3; /* 4 - one read */
		p->chars_buf >>= 8;
		return c;
	}

	/* Check if there is anything in fifo */
	p->chars_cnt = readl(p->base + UARTDM_RXFS);
	/* Extract number of characters in UART packing buffer */
	p->chars_cnt = (p->chars_cnt >> UARTDM_RXFS_BUF_SHIFT) &
		       UARTDM_RXFS_BUF_MASK;
	if (!p->chars_cnt)
		return -EAGAIN;

	/* There is at least one charcter, move it to fifo */
	writel(UARTDM_CR_CMD_FORCE_STALE, p->base + UARTDM_CR);

	p->chars_buf = readl(p->base + UARTDM_RF);
	writel(UARTDM_CR_CMD_RESET_STALE_INT, p->base + UARTDM_CR);
	writel(0xFFFFFF, p->base + UARTDM_DMRX);

	c = p->chars_buf & 0xFF;
	p->chars_buf >>= 8;
	p->chars_cnt--;

	return c;
}

static int msm_serial_putc(struct udevice *dev, const char ch)
{
	struct msm_serial_data *p = dev_get_priv(dev);

	if (!(readl(p->base + UARTDM_SR) & UARTDM_SR_TX_EMPTY) &&
	    !(readl(p->base + UARTDM_ISR) & UARTDM_ISR_TX_READY))
		return -EAGAIN;

	writel(UARTDM_CR_CMD_RESET_TX_READY, p->base + UARTDM_CR);

	writel(1, p->base + UARTDM_NCF_TX);
	writel(ch, p->base + UARTDM_TF);
	return 0;
}

static int msm_serial_pending(struct udevice *dev, bool input)
{
	struct msm_serial_data *p = dev_get_priv(dev);

	if (input) {
		if (p->chars_cnt)
			return 1;
		if (readl(p->base + UARTDM_SR) & UARTDM_SR_RX_READY)
			return 1;
		if (readl(p->base + UARTDM_RXFS))
			return 1;
	}
	return 0;
}

static const struct dm_serial_ops msm_serial_ops = {
	.putc = msm_serial_putc,
	.pending = msm_serial_pending,
	.getc = msm_serial_getc,
};

int clk_init_uart(void);
static int msm_serial_probe(struct udevice *dev)
{
	struct msm_serial_data *p = dev_get_priv(dev);
	clk_init_uart();

	if (readl(p->base + UARTDM_SR) & UARTDM_SR_UART_OVERRUN)
		writel(UARTDM_CR_CMD_RESET_ERR, p->base + UARTDM_CR);

	writel(0, p->base + UARTDM_IMR);
	writel(UARTDM_CR_CMD_STALE_EVENT_DISABLE, p->base + UARTDM_CR);
	writel(0xFFFFFF, p->base + UARTDM_DMRX);

	p->chars_buf = 0;
	p->chars_cnt = 0;
	return 0;
}

static int msm_serial_ofdata_to_platdata(struct udevice *dev)
{
	struct msm_serial_data *p = dev_get_priv(dev);

	p->base = dev_get_addr(dev);
	if (p->base == FDT_ADDR_T_NONE)
		return -EINVAL;
	return 0;
}

static const struct udevice_id msm_serial_ids[] = {
	{ .compatible = "qcom,msm-uartdm-v1.4" },
	{ }
};

U_BOOT_DRIVER(serial_msm) = {
	.name	= "serial_msm",
	.id	= UCLASS_SERIAL,
	.of_match = msm_serial_ids,
	.ofdata_to_platdata = msm_serial_ofdata_to_platdata,
	.priv_auto_alloc_size = sizeof(struct msm_serial_data),
	.probe = msm_serial_probe,
	.ops	= &msm_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
