/*
 * Qualcomm GPIO driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Based on Linux/Little-Kernel driver
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <asm/arch/sysmap.h>
#include <asm/io.h>
#include <errno.h>

DECLARE_GLOBAL_DATA_PTR;

struct msm_gpio_bank {
	phys_addr_t base;
};

static int msm_gpio_direction_input(struct udevice *dev, unsigned int gpio)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);
	phys_addr_t reg = priv->base + GPIO_CONFIG_OFF(gpio);

	/* Disable OE bit */
	writel((readl(reg) & ~GPIO_OE_MASK) | GPIO_OE_DISABLE , reg);
	return 0;
}

static int msm_gpio_set_value(struct udevice *dev, unsigned gpio, int value)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);

	value = !!value;
	/* set value */
	writel(value << GPIO_OUT, priv->base + GPIO_IN_OUT_OFF(gpio));
	return 0;
}

static int msm_gpio_direction_output(struct udevice *dev, unsigned gpio,
				     int value)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);
	phys_addr_t reg = priv->base + GPIO_CONFIG_OFF(gpio);

	value = !!value;
	/* set value */
	writel(value << GPIO_OUT, priv->base + GPIO_IN_OUT_OFF(gpio));
	/* switch direction */
	writel((readl(reg) & ~GPIO_OE_MASK) | GPIO_OE_ENABLE , reg);
	return 0;
}

static int msm_gpio_get_value(struct udevice *dev, unsigned gpio)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);

	return !!(readl(priv->base + GPIO_IN_OUT_OFF(gpio)) >> GPIO_IN);
}

static int msm_gpio_get_function(struct udevice *dev, unsigned offset)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);

	if (readl(priv->base + GPIO_CONFIG_OFF(offset)) & GPIO_OE_ENABLE)
		return GPIOF_OUTPUT;
	return GPIOF_INPUT;
}

static const struct dm_gpio_ops gpio_msm_ops = {
	.direction_input	= msm_gpio_direction_input,
	.direction_output	= msm_gpio_direction_output,
	.get_value		= msm_gpio_get_value,
	.set_value		= msm_gpio_set_value,
	.get_function		= msm_gpio_get_function,
};

static int msm_gpio_probe(struct udevice *dev)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);

	priv->base = dev_get_addr(dev);
	return priv->base == FDT_ADDR_T_NONE;
}

static int msm_gpio_ofdata_to_platdata(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->gpio_count = fdtdec_get_int(gd->fdt_blob, dev->of_offset,
					     "gpio-count", 0);
	uc_priv->bank_name = fdt_getprop(gd->fdt_blob, dev->of_offset,
					 "gpio-bank-name", NULL);
	if (uc_priv->bank_name == NULL)
		uc_priv->bank_name = "soc";
	return 0;
}

static const struct udevice_id msm_gpio_ids[] = {
	{ .compatible = "qcom,msm8916-pinctrl" },
	{ }
};

U_BOOT_DRIVER(gpio_msm) = {
	.name	= "gpio_msm",
	.id	= UCLASS_GPIO,
	.of_match = msm_gpio_ids,
	.ofdata_to_platdata = msm_gpio_ofdata_to_platdata,
	.probe	= msm_gpio_probe,
	.ops	= &gpio_msm_ops,
	.priv_auto_alloc_size = sizeof(struct msm_gpio_bank),
};


