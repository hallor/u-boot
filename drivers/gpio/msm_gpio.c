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

struct msm_gpio_bank {
	void * base; // base reg (TLMM_BASE_ADDR)
};

static int msm_gpio_direction_input(struct udevice *dev, unsigned int gpio)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);

	void *reg = priv->base + GPIO_CONFIG_OFF(gpio);
	// Disable OE bit
	writel( (readl(reg) & ~GPIO_OE_MASK) | GPIO_OE_DISABLE , reg);
	return 0;
}

static int msm_gpio_set_value(struct udevice *dev, unsigned gpio, int value)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);

	value = !!value;
	//set value
	writel(value << GPIO_OUT, priv->base + GPIO_IN_OUT_OFF(gpio));
	return 0;
}

static int msm_gpio_direction_output(struct udevice *dev, unsigned gpio, int value)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);
	void * reg = priv->base + GPIO_CONFIG_OFF(gpio);

	value = !!value;
	// set value
	writel(value << GPIO_OUT, priv->base + GPIO_IN_OUT_OFF(gpio));
	// switch direction
	writel( (readl(reg) & ~GPIO_OE_MASK) | GPIO_OE_ENABLE , reg);
	return 0;
}

static int msm_gpio_get_value(struct udevice *dev, unsigned gpio)
{
	struct msm_gpio_bank *priv = dev_get_priv(dev);

	return !!(readl(priv->base + GPIO_IN_OUT_OFF(gpio)) >> GPIO_IN);
}

static const struct dm_gpio_ops gpio_msm_ops = {
	.direction_input	= msm_gpio_direction_input,
	.direction_output	= msm_gpio_direction_output,
	.get_value		= msm_gpio_get_value,
	.set_value		= msm_gpio_set_value,
};

static int msm_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct msm_gpio_bank *priv = dev_get_priv(dev);

	uc_priv->bank_name = strdup("msm");
	if (!uc_priv->bank_name)
		return -ENOMEM;
	uc_priv->gpio_count = MSM_GPIO_COUNT;
	priv->base = (void*)TLMM_BASE_ADDR;

	return 0;
}

U_BOOT_DRIVER(gpio_msm) = {
	.name	= "gpio_msm",
	.id	= UCLASS_GPIO,
	.ops	= &gpio_msm_ops,
	.probe	= msm_gpio_probe,
	.priv_auto_alloc_size = sizeof(struct msm_gpio_bank),
};


