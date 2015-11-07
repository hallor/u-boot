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

#define GPIO_PERIPHERAL_BASE                  0xC000
#define GPIO_N_PERIPHERAL_BASE(x)            (GPIO_PERIPHERAL_BASE + ((x) - 1) * 0x100)

#define GPIO_STATUS                           0x08
#define GPIO_MODE_CTL                         0x40
#define GPIO_DIG_VIN_CTL                      0x41
#define GPIO_DIG_PULL_CTL                     0x42
#define GPIO_DIG_OUT_CTL                      0x45
#define GPIO_EN_CTL                           0x46

#define PERPH_EN_BIT                          7
#define GPIO_STATUS_VAL_BIT                   0
#define PM_GPIO_MODE_MASK       0x70
#define PM_GPIO_OUTPUT_MASK     0x0F

#define PON_INT_RT_STS                        0x810
#define KPDPWR_ON_INT_BIT                     0
#define RESIN_ON_INT_BIT                      1

uint8_t pmic_bus_read(uint32_t addr);
void pmic_bus_write(uint32_t addr, uint8_t val);

#define PM_GPIO_DIR_OUT         0x01
#define PM_GPIO_DIR_IN          0x00
#define PM_GPIO_DIR_BOTH        0x02

int pm8916_gpio_input(uint8_t gpio)
{
	uint8_t  val;
	uint32_t gpio_base = GPIO_N_PERIPHERAL_BASE(gpio);

	/* Disable the GPIO */
	val  = pmic_bus_read(gpio_base + GPIO_EN_CTL);
	val &= ~BIT(PERPH_EN_BIT);
	pmic_bus_write(gpio_base + GPIO_EN_CTL, val);

	/* Select the mode */
	val = 0 | (0x0 << 4); // assume function == 0, input
	pmic_bus_write(gpio_base + GPIO_MODE_CTL, val);

	/* Set the right pull */
	pmic_bus_write(gpio_base + GPIO_DIG_PULL_CTL, 0x5); // No pull

	/* Select the VIN */
	pmic_bus_write(gpio_base + GPIO_DIG_VIN_CTL, 0); // VIN0 -> device tree, but pin is output so it doesn't matter

	/* Enable the GPIO */
	val  = pmic_bus_read(gpio_base + GPIO_EN_CTL);
	val |= BIT(PERPH_EN_BIT);
	pmic_bus_write(gpio_base + GPIO_EN_CTL, val);

	return 0;
}

int pm8916_gpio_output(uint8_t gpio, int v)
{
	uint8_t  val;
	uint32_t gpio_base = GPIO_N_PERIPHERAL_BASE(gpio);

	/* Disable the GPIO */
	val  = pmic_bus_read(gpio_base + GPIO_EN_CTL);
	val &= ~BIT(PERPH_EN_BIT);
	pmic_bus_write(gpio_base + GPIO_EN_CTL, val);

	/* Select the mode */
	val = 0 | (0x1 << 4) | (v ? 1 : 0); // function == normal, output, value
	pmic_bus_write(gpio_base + GPIO_MODE_CTL, val);

	/* Set the right pull */
	pmic_bus_write(gpio_base + GPIO_DIG_PULL_CTL, 0x5); // No pull

	/* Select the VIN */
	pmic_bus_write(gpio_base + GPIO_DIG_VIN_CTL, 0); // VIN0

	/* Set the right dig out control */
	val = 0x1 | (0x0 << 4); // low drive strength, cmos
	pmic_bus_write(gpio_base + GPIO_DIG_OUT_CTL, val);

	/* Enable the GPIO */
	val  = pmic_bus_read(gpio_base + GPIO_EN_CTL);
	val |= BIT(PERPH_EN_BIT);
	pmic_bus_write(gpio_base + GPIO_EN_CTL, val);

	return 0;
}

/* Reads the status of requested gpio */
int pm8916_gpio_get(uint8_t gpio, uint8_t *status)
{
	uint32_t gpio_base = GPIO_N_PERIPHERAL_BASE(gpio);

	*status = pmic_bus_read(gpio_base + GPIO_STATUS);

	/* Return the value of the GPIO pin */
	*status &= BIT(GPIO_STATUS_VAL_BIT);

	printf("GPIO %d status is %d\n", gpio, *status);

	return 0;
}

/* Write the output value of the requested gpio */
int pm8916_gpio_set(uint8_t gpio, uint8_t value)
{
	uint32_t gpio_base = GPIO_N_PERIPHERAL_BASE(gpio);
	uint8_t val;

	/* Set the output value of the gpio */
	val = pmic_bus_read(gpio_base + GPIO_MODE_CTL);
	val = (val & ~PM_GPIO_OUTPUT_MASK) | value;
	pmic_bus_write(gpio_base + GPIO_MODE_CTL, val);

	return 0;
}

int pm8916_is_pwrkey_pressed(void)
{
	return !!(pmic_bus_read(PON_INT_RT_STS) & BIT(KPDPWR_ON_INT_BIT));
}

int pm8916_is_resin_pressed(void)
{
	return !!(pmic_bus_read(PON_INT_RT_STS) & BIT(RESIN_ON_INT_BIT));
}

int pm8916_init(void)
{
	pm8916_gpio_output(1, 1);
	return 0;
}
