/*
 * Qualcomm pm8916 pmic gpio driver - part of Qualcomm PM8916 PMIC
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
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

/* Register offset for each gpio */
#define REG_OFFSET(x)          ((x) * 0x100)

/* Register maps */
#define REG_STATUS             0x08
#define REG_STATUS_VAL_MASK    0x1

/* MODE_CTL */
#define REG_CTL           0x40
#define REG_CTL_MODE_MASK       0x70
#define REG_CTL_MODE_INPUT      0x00
#define REG_CTL_MODE_INOUT      0x20
#define REG_CTL_MODE_OUTPUT     0x10
#define REG_CTL_OUTPUT_MASK     0x0F

#define REG_DIG_VIN_CTL        0x41
#define REG_DIG_VIN_VIN0       0

#define REG_DIG_PULL_CTL       0x42
#define REG_DIG_PULL_NO_PU     0x5

#define REG_DIG_OUT_CTL        0x45
#define REG_DIG_OUT_CTL_CMOS   (0x0 << 4)
#define REG_DIG_OUT_CTL_DRIVE_L (0x1)

#define REG_EN_CTL             0x46
#define REG_EN_CTL_ENABLE      (1 << 7)

struct pm8916_gpio_bank {
	uint16_t pid; /* Peripheral ID on SPMI bus */
};

static int pm8916_gpio_direction_input(struct udevice *dev, unsigned offset)
{
	struct pm8916_gpio_bank *priv = dev_get_priv(dev);
	uint32_t gpio_base = priv->pid + REG_OFFSET(offset);
	int val, ret;

	/* Disable the GPIO */
	val = pmic_reg_read(dev->parent, gpio_base + REG_EN_CTL);
	if (val < 0)
		return val;

	val &= ~REG_EN_CTL_ENABLE;
	ret = pmic_reg_write(dev->parent, gpio_base + REG_EN_CTL, val);
	if (ret < 0)
		return ret;

	/* Select the mode */
	val = REG_CTL_MODE_INPUT;
	ret = pmic_reg_write(dev->parent, gpio_base + REG_CTL, val);
	if (ret < 0)
		return ret;

	/* Set the right pull (no pull) */
	ret = pmic_reg_write(dev->parent, gpio_base + REG_DIG_PULL_CTL,
			     REG_DIG_PULL_NO_PU);
	if (ret < 0)
		return ret;

	/* Select the VIN - VIN0, pin is input so it doesn't matter */
	ret = pmic_reg_write(dev->parent, gpio_base + REG_DIG_VIN_CTL,
			     REG_DIG_VIN_VIN0);
	if (ret < 0)
		return ret;

	/* Enable the GPIO */
	val = pmic_reg_read(dev->parent, gpio_base + REG_EN_CTL);
	if (val < 0)
		return val;

	val |= REG_EN_CTL_ENABLE;

	ret = pmic_reg_write(dev->parent, gpio_base + REG_EN_CTL, val);
	return ret;
}

static int pm8916_gpio_direction_output(struct udevice *dev, unsigned offset,
					int value)
{
	struct pm8916_gpio_bank *priv = dev_get_priv(dev);
	uint32_t gpio_base = priv->pid + REG_OFFSET(offset);
	int val, ret;

	/* Disable the GPIO */
	val = pmic_reg_read(dev->parent, gpio_base + REG_EN_CTL);
	if (val < 0)
		return val;

	val &= REG_EN_CTL_ENABLE;
	ret = pmic_reg_write(dev->parent, gpio_base + REG_EN_CTL, val);
	if (ret < 0)
		return ret;

	/* Select the mode and output value*/
	val = REG_CTL_MODE_INOUT | (value ? 1 : 0);
	ret = pmic_reg_write(dev->parent, gpio_base + REG_CTL, val);
	if (ret < 0)
		return ret;

	/* Set the right pull */
	ret = pmic_reg_write(dev->parent, gpio_base + REG_DIG_PULL_CTL,
			     REG_DIG_PULL_NO_PU);
	if (ret < 0)
		return ret;

	/* Select the VIN - VIN0*/
	ret = pmic_reg_write(dev->parent, gpio_base + REG_DIG_VIN_CTL,
			     REG_DIG_VIN_VIN0);
	if (ret < 0)
		return ret;

	/* Set the right dig out control */
	val = REG_DIG_OUT_CTL_CMOS | REG_DIG_OUT_CTL_DRIVE_L;
	ret = pmic_reg_write(dev->parent, gpio_base + REG_DIG_OUT_CTL, val);
	if (ret < 0)
		return ret;

	/* Enable the GPIO */
	val = pmic_reg_read(dev->parent, gpio_base + REG_EN_CTL);
	if (val < 0)
		return val;

	val |= REG_EN_CTL_ENABLE;

	ret = pmic_reg_write(dev->parent, gpio_base + REG_EN_CTL, val);
	return ret;
}

static int pm8916_gpio_get_function(struct udevice *dev, unsigned offset)
{
	struct pm8916_gpio_bank *priv = dev_get_priv(dev);
	uint32_t gpio_base = priv->pid + REG_OFFSET(offset);
	int reg;

	/* Set the output value of the gpio */
	reg = pmic_reg_read(dev->parent, gpio_base + REG_CTL);
	if (reg < 0)
		return reg;

	if ((reg & REG_CTL_MODE_MASK) == REG_CTL_MODE_INPUT)
		return GPIOF_INPUT;
	if ((reg & REG_CTL_MODE_MASK) == REG_CTL_MODE_INOUT)
		return GPIOF_OUTPUT;
	if ((reg & REG_CTL_MODE_MASK) == REG_CTL_MODE_OUTPUT)
		return GPIOF_OUTPUT;
	return GPIOF_UNKNOWN;
}

static int pm8916_gpio_get_value(struct udevice *dev, unsigned offset)
{
	struct pm8916_gpio_bank *priv = dev_get_priv(dev);
	uint32_t gpio_base = priv->pid + REG_OFFSET(offset);
	int reg;

	reg = pmic_reg_read(dev->parent, gpio_base + REG_STATUS);
	if (reg < 0)
		return reg;

	return !!(reg & REG_STATUS_VAL_MASK);
}

static int pm8916_gpio_set_value(struct udevice *dev, unsigned offset,
				 int value)
{
	struct pm8916_gpio_bank *priv = dev_get_priv(dev);
	uint32_t gpio_base = priv->pid + REG_OFFSET(offset);
	int reg;

	/* Set the output value of the gpio */
	reg = pmic_reg_read(dev->parent, gpio_base + REG_CTL);
	if (reg < 0)
		return reg;

	reg = (reg & ~REG_CTL_OUTPUT_MASK) | value;

	return pmic_reg_write(dev->parent, gpio_base + REG_CTL, reg);
}

static const struct dm_gpio_ops pm8916_gpio_ops = {
	.direction_input	= pm8916_gpio_direction_input,
	.direction_output	= pm8916_gpio_direction_output,
	.get_value		= pm8916_gpio_get_value,
	.set_value		= pm8916_gpio_set_value,
	.get_function		= pm8916_gpio_get_function,
};

static int pm8916_gpio_probe(struct udevice *dev)
{
	struct pm8916_gpio_bank *priv = dev_get_priv(dev);
	priv->pid = dev_get_addr(dev);
	return 0;
}

static int pm8916_gpio_ofdata_to_platdata(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->gpio_count = fdtdec_get_int(gd->fdt_blob, dev->of_offset,
					     "gpio-count", 0);
	uc_priv->bank_name = fdt_getprop(gd->fdt_blob, dev->of_offset,
					 "gpio-bank-name", NULL);
	if (uc_priv->bank_name == NULL)
		uc_priv->bank_name = "pm8916";
	return 0;
}

static const struct udevice_id pm8916_gpio_ids[] = {
	{ .compatible = "qcom,pm8916-gpio" },
	{ }
};

U_BOOT_DRIVER(gpio_pm8916) = {
	.name	= "gpio_pm8916",
	.id	= UCLASS_GPIO,
	.of_match = pm8916_gpio_ids,
	.ofdata_to_platdata = pm8916_gpio_ofdata_to_platdata,
	.probe	= pm8916_gpio_probe,
	.ops	= &pm8916_gpio_ops,
	.priv_auto_alloc_size = sizeof(struct pm8916_gpio_bank),
};


/* Add pmic buttons as GPIO as well - there is no generic way for now */
#define PON_INT_RT_STS                        0x10
#define KPDPWR_ON_INT_BIT                     0
#define RESIN_ON_INT_BIT                      1

static int pm8941_pwrkey_get_function(struct udevice *dev, unsigned offset)
{
	return GPIOF_INPUT;
}

static int pm8941_pwrkey_get_value(struct udevice *dev, unsigned offset)
{
	struct pm8916_gpio_bank *priv = dev_get_priv(dev);

	int reg = pmic_reg_read(dev->parent, priv->pid + PON_INT_RT_STS);

	if (reg < 0)
		return 0;

	switch (offset) {
	case 0: /* Power button */
		return ((reg & BIT(KPDPWR_ON_INT_BIT)) != 0);
		break;
	case 1: /* Reset button */
	default:
		return ((reg & BIT(RESIN_ON_INT_BIT)) != 0);
		break;
	}
}

static const struct dm_gpio_ops pm8941_pwrkey_ops = {
	.get_value		= pm8941_pwrkey_get_value,
	.get_function		= pm8941_pwrkey_get_function,
};

static int pm8941_pwrkey_probe(struct udevice *dev)
{
	struct pm8916_gpio_bank *priv = dev_get_priv(dev);

	priv->pid = dev_get_addr(dev);
	return 0;
}

static int pm8941_pwrkey_ofdata_to_platdata(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	uc_priv->gpio_count = 2;
	uc_priv->bank_name = "pm8916_key";
	return 0;
}

static const struct udevice_id pm8941_pwrkey_ids[] = {
	{ .compatible = "qcom,pm8916-pwrkey" },
	{ }
};

U_BOOT_DRIVER(pwrkey_pm8941) = {
	.name	= "pwrkey_pm8916",
	.id	= UCLASS_GPIO,
	.of_match = pm8941_pwrkey_ids,
	.ofdata_to_platdata = pm8941_pwrkey_ofdata_to_platdata,
	.probe	= pm8941_pwrkey_probe,
	.ops	= &pm8941_pwrkey_ops,
	.priv_auto_alloc_size = sizeof(struct pm8916_gpio_bank),
};
