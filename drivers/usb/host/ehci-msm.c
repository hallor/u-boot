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
#include <asm-generic/errno.h>
#include <linux/compat.h>
#include <dm.h>
#include <fdtdec.h>
#include <libfdt.h>
#include <asm/io.h>
#include <usb.h>
#include <usb/ulpi.h>
#include "ehci.h"

#ifndef CONFIG_USB_ULPI_VIEWPORT
#error Please enable CONFIG_USB_ULPI_VIEWPORT
#endif

#define MSM_USB_ULPI_OFFSET 0x170 /* ULPI viewport (PHY) */
#define MSM_USB_EHCI_OFFSET 0x100 /* Start of EHCI registers */

/* PHY viewport regs */
#define ULPI_MISC_A_READ         0x96
#define ULPI_MISC_A_SET          0x97
#define ULPI_MISC_A_CLEAR        0x98
#define ULPI_MISC_A_VBUSVLDEXTSEL    (1 << 1)
#define ULPI_MISC_A_VBUSVLDEXT       (1 << 0)

/* qcom specific registers (OTG) */
#define USB_GENCONFIG_2      (0x00A0)
#define GEN2_SESS_VLD_CTRL_EN (1 << 7)

#define USB_USBCMD           (0x0140)
#define SESS_VLD_CTRL         (1 << 25)
#define USBCMD_RESET   2
#define USBCMD_ATTACH  1

/* USB2_HSIC_USB_OTG_HS_BASE_USB_OTG_HS_PORTSC */
#define USB_PORTSC           (0x0184)
#define USB_SBUSCFG          (0x0090)
#define USB_AHB_MODE         (0x0098)

#define USB_USBMODE          (0x01A8)
#define USBMODE_DEVICE 2
#define USBMODE_HOST   3

struct msm_ehci_priv {
	struct ehci_ctrl ctrl; /* Needed by EHCI */
	phys_addr_t base;
	phys_addr_t ehci_base;
	u32 ulpi_base;
	u32 ulpi_port;
};

void __weak board_prepare_usb(enum usb_init_type type)
{
}

static void setup_usb_phy(struct msm_ehci_priv *priv)
{
	struct ulpi_viewport ulpi_vp = {.port_num = priv->ulpi_port,
					.viewport_addr = priv->ulpi_base};

	/* Select and enable external configuration with USB PHY */
	ulpi_write(&ulpi_vp, (u8 *)ULPI_MISC_A_SET,
		   ULPI_MISC_A_VBUSVLDEXTSEL | ULPI_MISC_A_VBUSVLDEXT);
}

static void reset_usb_phy(struct msm_ehci_priv *priv)
{
	struct ulpi_viewport ulpi_vp = {.port_num = priv->ulpi_port,
					.viewport_addr = priv->ulpi_base};

	/* Disable VBUS mimicing in the controller. */
	ulpi_write(&ulpi_vp, (u8 *)ULPI_MISC_A_CLEAR,
		   ULPI_MISC_A_VBUSVLDEXTSEL | ULPI_MISC_A_VBUSVLDEXT);
}


static int msm_init_after_reset(struct ehci_ctrl *dev)
{
	struct msm_ehci_priv *p = container_of(dev, struct msm_ehci_priv, ctrl);
	uint32_t val;

	/* select ULPI phy */
	writel(0x80000000, p->base + USB_PORTSC);
	setup_usb_phy(p);

	/* Enable sess_vld */
	val = readl(p->base + USB_GENCONFIG_2) | GEN2_SESS_VLD_CTRL_EN;
	writel(val, p->base + USB_GENCONFIG_2);

	/* Enable external vbus configuration in the LINK */
	val = readl(p->base + USB_USBCMD);
	val |= SESS_VLD_CTRL;
	writel(val, p->base + USB_USBCMD);

	/* USB_OTG_HS_AHB_BURST */
	writel(0x0, p->base + USB_SBUSCFG);

	/* USB_OTG_HS_AHB_MODE: HPROT_MODE */
	/* Bus access related config. */
	writel(0x08, p->base + USB_AHB_MODE);

	/* set mode to host controller */
	writel(USBMODE_HOST, p->base + USB_USBMODE);

	return 0;
}

static const struct ehci_ops msm_ehci_ops = {
	.init_after_reset = msm_init_after_reset
};

static int ehci_usb_probe(struct udevice *dev)
{
	struct msm_ehci_priv *p = dev_get_priv(dev);
	struct ehci_hccr *cr;
	struct ehci_hcor *or;

	cr = (struct ehci_hccr *)p->ehci_base;
	or = (struct ehci_hcor *)(p->ehci_base +
				  HC_LENGTH(readl(p->ehci_base)));

	board_prepare_usb(USB_INIT_HOST);

	return ehci_register(dev, cr, or, &msm_ehci_ops, 0, USB_INIT_HOST);
}

static int ehci_usb_remove(struct udevice *dev)
{
	struct msm_ehci_priv *p = dev_get_priv(dev);
	phys_addr_t reg = p->base + USB_USBCMD;
	int ret;

	ret = ehci_deregister(dev);
	if (ret)
		return ret;

	/* Stop controller. */
	writel(readl(reg) & ~USBCMD_ATTACH, reg);

	reset_usb_phy(p);

	board_prepare_usb(USB_INIT_DEVICE); /* Board specific hook */

	/* Reset controller */
	writel(0x00080002, reg); /* reset usb */
	mdelay(20);
	/* Wait for completion */
	while (readl(reg) & 2)
		;

	return 0;
}

static int ehci_usb_ofdata_to_platdata(struct udevice *dev)
{
	struct msm_ehci_priv *priv = dev_get_priv(dev);

	priv->base = dev_get_addr(dev);
	priv->ehci_base = priv->base + MSM_USB_EHCI_OFFSET;
	priv->ulpi_base = priv->base + MSM_USB_ULPI_OFFSET;
	priv->ulpi_port = 0;
	return 0;
}

static const struct udevice_id ehci_usb_ids[] = {
	{ .compatible = "qcom,ehci-host", },
	{ }
};

U_BOOT_DRIVER(usb_ehci) = {
	.name	= "ehci_msm",
	.id	= UCLASS_USB,
	.of_match = ehci_usb_ids,
	.ofdata_to_platdata = ehci_usb_ofdata_to_platdata,
	.probe = ehci_usb_probe,
	.remove = ehci_usb_remove,
	.ops	= &ehci_usb_ops,
	.priv_auto_alloc_size = sizeof(struct msm_ehci_priv),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};
