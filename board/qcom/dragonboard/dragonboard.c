/*
 * Board init file for Dragonboard 410C
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>

 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <usb.h>

U_BOOT_DEVICES(msm_gpios) = {
	{ "gpio_msm", NULL },
	{ "msm_sdc", (void*)1 }, // sdc1 == emmc
	{ "msm_sdc", (void*)2 }, // sdc2 == SD
	{ "ehci_msm", NULL },
};

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;
	return 0;
}

void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
}

#define PM_GPIO_USB_HUB_RESET_N 3
#define PM_GPIO_USB_SW_SEL 4
int pm8916_gpio_output(uint8_t gpio, int v);
int pm8916_gpio_set(uint8_t gpio, uint8_t value);
void board_prepare_usb(enum usb_init_type type)
{
	if (type == USB_INIT_HOST) {
		/* Start USB Hub */
		pm8916_gpio_output(PM_GPIO_USB_HUB_RESET_N, 1);
		mdelay(100);
		/* Switch usb to host connectors */
		pm8916_gpio_output(PM_GPIO_USB_SW_SEL, 1);
		mdelay(100);
	} else { // Device
		/* Disable hub */
		pm8916_gpio_set(PM_GPIO_USB_HUB_RESET_N, 0);
		/* Switch back to device connector */
		pm8916_gpio_set(PM_GPIO_USB_SW_SEL, 0);
	}
}

int power_pmicc_init(void);
int pm8916_init(void);
int power_init_board(void)
{
	power_pmicc_init();
	pm8916_init();
	return 0;
}

int board_init(void)
{
	return 0;
}

uint32_t pm8916_is_resin_pressed(void);

int misc_init_r(void)
{
	if (pm8916_is_resin_pressed()) {
		setenv("bootdelay", "-1");
		printf("Power button pressed - dropping to console.\n");
	}
	return 0;
}
