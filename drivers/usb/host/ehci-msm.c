/*
 * Qualcomm EHCI driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Based on Linux driver
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/gpio.h>
#include <asm/arch/sysmap.h>
#include <asm-generic/errno.h>
#include <linux/compat.h>
#include <dm.h>
#include <asm/io.h>
#include <usb.h>
#include <usb/ulpi.h>
#include "ehci.h"

#ifndef CONFIG_USB_ULPI_VIEWPORT
#error Please enable CONFIG_USB_ULPI_VIEWPORT
#endif

/* PHY viewport && Regs */
#define USB_ULPI_VIEWPORT    (MSM_USB_BASE + 0x0170)
#define ULPI_MISC_A_READ         0x96
#define ULPI_MISC_A_SET          0x97
#define ULPI_MISC_A_CLEAR        0x98
#define ULPI_MISC_A_VBUSVLDEXTSEL    (1 << 1)
#define ULPI_MISC_A_VBUSVLDEXT       (1 << 0)

/* qcom specific registers (OTG) */
#define USB_GENCONFIG_2      (MSM_USB_BASE + 0x00A0)
#define GEN2_SESS_VLD_CTRL_EN (1 << 7)

/* Start of EHCI registers (CAP) - USB2_HSIC_USB_OTG_HS_BASE_USB_OTG_HS_CAPLENGTH */
#define USB_EHCI_BASE        (MSM_USB_BASE + 0x0100)

#define USB_USBCMD           (MSM_USB_BASE + 0x0140)
#define SESS_VLD_CTRL         (1 << 25)
#define USBCMD_RESET   2
#define USBCMD_ATTACH  1

#define USB_PORTSC           (MSM_USB_BASE + 0x0184) /* USB2_HSIC_USB_OTG_HS_BASE_USB_OTG_HS_PORTSC p5028*/
#define USB_SBUSCFG          (MSM_USB_BASE + 0x0090)
#define USB_AHB_MODE         (MSM_USB_BASE + 0x0098)

#define USB_USBMODE          (MSM_USB_BASE + 0x01A8)
#define USBMODE_DEVICE 2
#define USBMODE_HOST   3

void board_prepare_usb(enum usb_init_type type);

int ehci_hcd_init(int index, enum usb_init_type init,
		struct ehci_hccr **hccr, struct ehci_hcor **hcor)
{
	struct ulpi_viewport ulpi_vp = {.port_num=0, .viewport_addr=USB_ULPI_VIEWPORT};
	struct ehci_hccr *cr;
	struct ehci_hcor *or;
	uint32_t val;

	if (index != 0)
		return -EINVAL;
	if (init != USB_INIT_HOST)
		return -EINVAL;

	cr = (struct ehci_hccr*)USB_EHCI_BASE;
	or = (struct ehci_hcor*)(USB_EHCI_BASE + HC_LENGTH(readl(USB_EHCI_BASE)));

	board_prepare_usb(init);

	/* RESET */
	writel(0x00080002, USB_USBCMD);
	mdelay(20);

	while((readl(USB_USBCMD)&2));

	/* select ULPI phy */
	writel(0x80000000, USB_PORTSC);

	/* Select and enable external configuration with USB PHY */
	ulpi_write(&ulpi_vp, (u8*)ULPI_MISC_A_SET, ULPI_MISC_A_VBUSVLDEXTSEL | ULPI_MISC_A_VBUSVLDEXT);

	/* Enable sess_vld */
	val = readl(USB_GENCONFIG_2) | GEN2_SESS_VLD_CTRL_EN;
	writel(val, USB_GENCONFIG_2);

	/* Enable external vbus configuration in the LINK */
	val = readl(USB_USBCMD);
	val |= SESS_VLD_CTRL;
	writel(val, USB_USBCMD);

	/* USB_OTG_HS_AHB_BURST */
	writel(0x0, USB_SBUSCFG);

	/* USB_OTG_HS_AHB_MODE: HPROT_MODE */
	/* Bus access related config. */
	writel(0x08, USB_AHB_MODE);

	writel(USBMODE_HOST, USB_USBMODE); //set mode to host controller

	*hccr = cr;
	*hcor = or;

	return 0;
}

int ehci_hcd_stop(int index)
{
	struct ulpi_viewport ulpi_vp = {.port_num=0, .viewport_addr=USB_ULPI_VIEWPORT};
	uint32_t val;

	if (index != 0)
		return -EINVAL;

	/* Stop controller. */
	val = readl(USB_USBCMD);
	writel(val & ~USBCMD_ATTACH, USB_USBCMD);

	/* Disable VBUS mimicing in the controller. */
	ulpi_write(&ulpi_vp, (u8*)ULPI_MISC_A_CLEAR, ULPI_MISC_A_VBUSVLDEXTSEL | ULPI_MISC_A_VBUSVLDEXT);

	board_prepare_usb(USB_INIT_DEVICE);

	/* Reset controller */
	writel(0x00080002, USB_USBCMD); //reset usb
	mdelay(20);
	while((readl(USB_USBCMD)&2));

	return 0;
}
