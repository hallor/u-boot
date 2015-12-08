/*
 * Qualcomm pm8916 pmic driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Based on Little Kernel code
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <power/pmic.h>
#include <spmi/spmi.h>
#include <asm/gpio.h>

DECLARE_GLOBAL_DATA_PTR;

#define PON_INT_RT_STS                        0x810
#define KPDPWR_ON_INT_BIT                     0
#define RESIN_ON_INT_BIT                      1

int pm8916_is_pwrkey_pressed(void)
{
#if 0
	int ret;
	uint8_t val;

	ret = pmic_bus_read(PON_INT_RT_STS, &val);

	return ret == 0 && ((val & BIT(KPDPWR_ON_INT_BIT)) != 0);
#endif
#warning TODO: rewrite for new pmic
	return 0;
}

int pm8916_is_resin_pressed(void)
{
#if 0
	int ret;
	uint8_t val;

	ret = pmic_bus_read(PON_INT_RT_STS, &val);

	return ret == 0 && ((val & BIT(RESIN_ON_INT_BIT)) != 0);
#endif
#warning TODO: rewrite for new pmic
	return 0;
}

/////////////////////////////////////////// NEW

#define EXTRACT_PID(x) (((x) >> 8) & 0xFF)
#define EXTRACT_REG(x) ((x) & 0xFF)

struct spmi_pmic_priv {
	uint16_t usid; /* Slave ID on SPMI bus */
};

static int spmi_pmic_reg_count(struct udevice *dev)
{
	return 500; // TODO: what return here?
}

static int spmi_pmic_write(struct udevice *dev, uint reg, const uint8_t *buff,
			   int len)
{
	struct spmi_pmic_priv *priv = dev_get_priv(dev);

	if (len != 1)
		return -EINVAL;

	return spmi_reg_write(dev->parent, priv->usid, EXTRACT_PID(reg),
			      EXTRACT_REG(reg), *buff);
}

static int spmi_pmic_read(struct udevice *dev, uint reg, uint8_t *buff, int len)
{
	struct spmi_pmic_priv *priv = dev_get_priv(dev);
	int val;

	if (len != 1)
		return -EINVAL;

	val = spmi_reg_read(dev->parent, priv->usid, EXTRACT_PID(reg),
			    EXTRACT_REG(reg));

	if (val < 0)
		return val;
	*buff = val;
	return 0;
}

static struct dm_pmic_ops spmi_pmic_ops = {
	.reg_count = spmi_pmic_reg_count,
	.read = spmi_pmic_read,
	.write = spmi_pmic_write,
};

static const struct udevice_id spmi_pmic_ids[] = {
	{ .compatible = "qcom,spmi-pmic" },
	{ }
};

static const struct pmic_child_info pmic_children_info[] = {
	{ .prefix = "pwrkey", .driver = "pm8941-pwrkey" },
	{ .prefix = "gpios", .driver = "gpio_pm8916" },
	{ },
};

static int spmi_pmic_probe(struct udevice *dev)
{
	struct spmi_pmic_priv *priv = dev_get_priv(dev);
	priv->usid = dev_get_addr(dev);
	return 0;
}

static int spmi_pmic_bind(struct udevice *dev)
{
	pmic_bind_children(dev, dev->of_offset, pmic_children_info);
	return 0;
}

U_BOOT_DRIVER(pmic_spmi) = {
	.name = "spmi_pmic",
	.id = UCLASS_PMIC,
	.of_match = spmi_pmic_ids,
	.bind = spmi_pmic_bind,
	.probe = spmi_pmic_probe,
	.ops = &spmi_pmic_ops,
	.priv_auto_alloc_size = sizeof(struct spmi_pmic_priv),
};
