/*
 * Qualcomm EHCI driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * It should cooperate with device controller
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <usb.h>
#include <asm/gpio.h>
#include <asm-generic/errno.h>
#include <linux/compat.h>
#include "ehci.h"

static int ehci_usb_probe(struct udevice *dev)
{
	return -1;
}

static int ehci_usb_remove(struct udevice *dev)
{
	return 0;
}

U_BOOT_DRIVER(usb_ehci) = {
	.name	= "msm_ehci",
	.id	= UCLASS_USB,
	.probe = ehci_usb_probe,
	.remove = ehci_usb_remove,
};
