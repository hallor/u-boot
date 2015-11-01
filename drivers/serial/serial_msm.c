/*
 * Qualcomm UART driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Based on Linux/Little-Kernel driver
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <linux/compiler.h>
#include <serial.h>
#include <watchdog.h>

/* Serial registers */
#define UARTDM_CR                         0x00a8
#define UARTDM_CR_CMD_FORCE_STALE         (4 << 8)
#define UARTDM_CR_CMD_RESET_TX_READY      (3 << 8)
#define UARTDM_CR_CMD_STALE_EVENT_ENABLE  (5 << 8)
#define UARTDM_CR_CMD_STALE_EVENT_DISABLE (6 << 8)
#define UARTDM_CR_CMD_RESET_STALE_INT     (8 << 4)
#define UARTDM_CR_CMD_RESET_ERR (3 << 4)

#define UARTDM_DMRX             0x34
#define UARTDM_NCF_TX           0x40
#define UARTDM_RF       0x0070
#define UARTDM_RXFS             0x50
#define UARTDM_RXFS_BUF_MASK    0x7
#define UARTDM_RXFS_BUF_SHIFT   0x7
#define UARTDM_TF       0x0070

#define UARTDM_IMR                0x00b0
#define UARTDM_ISR                0x00b4
#define UARTDM_ISR_TX_READY       0x80

#define UARTDM_SR	0xA4
#define UARTDM_SR_RX_READY       (1 << 0)
#define UARTDM_SR_TX_READY       (1 << 2)
#define UARTDM_SR_TX_EMPTY       (1 << 3)
#define UARTDM_SR_UART_OVERRUN   (1 << 4)

static void *uart_base = (void*)MSM_UART0_BASE;

static void uwr(unsigned v, unsigned a)
{
	writel(v, uart_base + a);
}

static unsigned urd(unsigned a)
{
	return readl(uart_base + a);
}

static inline void msm_wait_for_xmitr(void)
{
	while (!(urd(UARTDM_SR) & UARTDM_SR_TX_EMPTY)) {
		if (urd(UARTDM_ISR) & UARTDM_ISR_TX_READY)
			break;
		WATCHDOG_RESET();
	}
	uwr(UARTDM_CR_CMD_RESET_TX_READY, UARTDM_CR);
}

static void msm_reset_dm_count(int count)
{
	msm_wait_for_xmitr();
	uwr(count, UARTDM_NCF_TX);
	urd(UARTDM_NCF_TX);
}

static void msm_do_puts(const char *s)
{
	int i;
	int num_newlines = 0;
	int replaced = 0;
	unsigned count = 0;
	volatile void *tf = uart_base + UARTDM_TF;

	{
		const char * c = s;
		while (*c) {
			count++;
			c++;
		}
	}

	for (i = 0; i < count; i++)
		if (s[i] == '\n')
			num_newlines++;
	count += num_newlines;

	msm_reset_dm_count(count);
	i = 0;
	while (i < count) {
		int j;
		unsigned int num_chars;
		uint32_t buf = 0;

		num_chars = min(count - i, (unsigned)sizeof(buf));

		for (j = 0; j < num_chars; j++) {
			char c = *s;
			if (c == '\n' && !replaced) {
				buf |= (uint32_t)'\r' << (j * 8);
				j++;
				replaced = 1;
			}
			if (j < num_chars) {
				buf |= (uint32_t)c << (j * 8);
				s++;
				replaced = 0;
			}
		}

		while (!(urd(UARTDM_SR) & UARTDM_SR_TX_READY)) {
		}

		writel(buf, tf);
		i += num_chars;
	}
}

static unsigned chars_cnt = 0; // number of buffered chars
static uint32_t chars_buf = 0; // buffered chars
static int msm_serial_getc(void)
{
	unsigned sr;
	char c;

	// There was something buffered
	if (chars_cnt) {
		c = chars_buf & 0xFF;
		chars_buf >>= 8;
		chars_cnt--;
		return c;
	}

	// First - overrun fixup - TODO: how to avoid this?
	if (urd(UARTDM_SR) & UARTDM_SR_UART_OVERRUN) {
		uwr(UARTDM_CR_CMD_RESET_ERR, UARTDM_CR);
	}

	// We need to fetch new character

	sr = urd(UARTDM_SR);
	// There are at least 4 bytes in fifo
	if (sr & UARTDM_SR_RX_READY) {
		chars_buf = urd(UARTDM_RF);
		c = chars_buf & 0xFF;
		chars_cnt = 3; // 4 - one read
		chars_buf >>= 8;
		return c;
	}

	while (!chars_cnt) {
		chars_cnt = urd(UARTDM_RXFS);
		chars_cnt = (chars_cnt >> UARTDM_RXFS_BUF_SHIFT) & UARTDM_RXFS_BUF_MASK;
	}

	// There is at least one charcter in fifo
	uwr(UARTDM_CR_CMD_FORCE_STALE, UARTDM_CR);
	chars_buf = urd(UARTDM_RF);
	uwr(UARTDM_CR_CMD_RESET_STALE_INT, UARTDM_CR);
	uwr(0xFFFFFF, UARTDM_DMRX);
//	uwr(UARTDM_CR_CMD_STALE_EVENT_ENABLE, UARTDM_CR);
	c = chars_buf & 0xFF;
	chars_buf >>= 8;
	chars_cnt--;
	return c;
}


static int msm_serial_tstc(void)
{
	if (chars_cnt)
		return 1;
	if (urd(UARTDM_SR) & UARTDM_SR_RX_READY)
		return 1;
	if (urd(UARTDM_RXFS))
		return 1;
	return 0;
}

static void msm_serial_setbrg(void)
{
	msm_do_puts("setbrg()\n");
#warning TODO
}

static void msm_serial_putc(const char c)
{
	char a[2] = {c, 0};

	msm_do_puts(a);
}

int clk_init_uart(void);

int msm_serial_init(void)
{
	clk_init_uart();
#warning TODO: RX & TX GPIO CONFIG, port configuration

	if (urd(UARTDM_SR) & UARTDM_SR_UART_OVERRUN) {
		uwr(UARTDM_CR_CMD_RESET_ERR, UARTDM_CR);
	}
	uwr(0, UARTDM_IMR);
	uwr(UARTDM_CR_CMD_STALE_EVENT_DISABLE, UARTDM_CR);
	uwr(0xFFFFFF, UARTDM_DMRX);
	chars_cnt = 0;
	chars_buf = 0;
	return 0;
}

static struct serial_device msm_serial_drv = {
	.name	= "msm_serial",
	.start	= msm_serial_init,
	.stop	= NULL,
	.setbrg	= msm_serial_setbrg,
	.putc	= msm_serial_putc,
	.puts	= msm_do_puts,
	.getc	= msm_serial_getc,
	.tstc	= msm_serial_tstc,
};

void msm_serial_initialize(void)
{
	serial_register(&msm_serial_drv);
}

__weak struct serial_device *default_serial_console(void)
{
	return &msm_serial_drv;
}
