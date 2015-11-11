/*
 * Qualcomm SDHCI driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Based on Linux/Little-Kernel driver
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <asm/arch/sysmap.h>
#include <common.h>
#include <dm.h>
#include <linux/bitops.h>
#include <sdhci.h>

/* Non-standard registers needed for SDHCI startup */
#define SDCC_MCI_POWER   0x0
#define SDCC_MCI_POWER_SW_RST BIT(7)

/* This is undocumented register */
#define SDCC_MCI_VERSION             0x50
#define SDCC_MCI_VERSION_MAJOR_SHIFT 28
#define SDCC_MCI_VERSION_MAJOR_MASK  (0xf << SDCC_MCI_VERSION_MAJOR_SHIFT)
#define SDCC_MCI_VERSION_MINOR_MASK  0xff

#define SDCC_MCI_HC_MODE 0x78

/* Offset to SDHCI registers */
#define SDCC_SDHCI_OFFSET 0x900

/* Non standard (?) SDHCI register */
#define SDHCI_VENDOR_SPEC_CAPABILITIES0  0x11c

struct msm_sdhc
{
	unsigned no;
	void * base;
	struct sdhci_host host;
};

int clk_init_sdc(int slot);

static int msm_sdc_probe(struct udevice *dev)
{
	struct msm_sdhc * prv = dev_get_priv(dev);
	struct sdhci_host *host = &prv->host;
	u32 core_version, core_minor, core_major;

	prv->no = (unsigned) dev->platdata; //controller #

	if (prv->no < 1 || prv->no > 2)
		return -1;

	if (prv->no == 1) // EMMC
		prv->base = (void*)MSM_SDC1_BASE;
	else
		prv->base = (void*)MSM_SDC2_BASE; // sdc2

	host->name = "msm_sdhci";
	host->ioaddr = prv->base + SDCC_SDHCI_OFFSET;
	host->quirks = SDHCI_QUIRK_WAIT_SEND_CMD | SDHCI_QUIRK_BROKEN_R1B;
	host->index = prv->no - 1;

	if (clk_init_sdc(prv->no))
		return -1;

	/* Reset the core and Enable SDHC mode */
	writel(readl(prv->base + SDCC_MCI_POWER) | SDCC_MCI_POWER_SW_RST, prv->base + SDCC_MCI_POWER);

	/* SW reset can take upto 10HCLK + 15MCLK cycles. (min 40us) */
	mdelay(2);

	if (readl(prv->base + SDCC_MCI_POWER) & SDCC_MCI_POWER_SW_RST) {
		printf("Stuck in reset\n");
		return -1;
	}

	writel(1, prv->base + SDCC_MCI_HC_MODE);   // Enable host-controller mode

	core_version = readl(prv->base + SDCC_MCI_VERSION);
	core_major = (core_version & SDCC_MCI_VERSION_MAJOR_MASK) >> SDCC_MCI_VERSION_MAJOR_SHIFT;
	core_minor = core_version & SDCC_MCI_VERSION_MINOR_MASK;

	/*
	 * Support for some capabilities is not advertised by newer
	 * controller versions and must be explicitly enabled.
	 */
	if (core_major >= 1 && core_minor != 0x11 && core_minor != 0x12) {
		u32 caps = readl(host->ioaddr + SDHCI_CAPABILITIES);
		caps |= SDHCI_CAN_VDD_300 | SDHCI_CAN_DO_8BIT;
		writel(caps, host->ioaddr + SDHCI_VENDOR_SPEC_CAPABILITIES0);
	}

	/* Set host controller version */
	host->version = sdhci_readw(host, SDHCI_HOST_VERSION);

	// automatically detect max speed, min speed is because of using gpll0
	return add_sdhci(host, 0, 0);
}

static int msm_sdc_remove(struct udevice *dev)
{
	struct msm_sdhc * priv = dev_get_priv(dev);
	writel(0, priv->base + SDCC_MCI_HC_MODE); // Disable host-controller mode
	return 0;
}

U_BOOT_DRIVER(msm_sdc_drv) = {
	.name		= "msm_sdc",
	.id		= UCLASS_MMC,
	.probe		= msm_sdc_probe,
	.remove		= msm_sdc_remove,
	.priv_auto_alloc_size = sizeof(struct msm_sdhc),
};
