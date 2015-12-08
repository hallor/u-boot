/*
 * Board init file for Dragonboard 410C
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>

 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <usb.h>
#include <asm/gpio.h>

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

void board_prepare_usb(enum usb_init_type type)
{
	int ret;
	struct gpio_desc hub_reset, usb_sel;

	ret = dm_gpio_lookup_name("pmic2", &hub_reset);
	if (ret < 0) {
		printf("Failed to lookup pmic2 gpio\n");
		return;
	}

	ret = dm_gpio_lookup_name("pmic3", &usb_sel);
	if (ret < 0) {
		printf("Failed to lookup pmic3 gpio\n");
		return;
	}

	ret = dm_gpio_request(&hub_reset, "hub_reset");
	if (ret < 0) {
		printf("Failed to request hub_reset gpio\n");
		return;
	}

	ret = dm_gpio_request(&usb_sel, "usb_sel");
	if (ret < 0) {
		printf("Failed to request usb_sel gpio\n");
		return;
	}

	if (type == USB_INIT_HOST) {
		/* Start USB Hub */
		dm_gpio_set_dir_flags(&hub_reset, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
		mdelay(100);
		/* Switch usb to host connectors */
		dm_gpio_set_dir_flags(&usb_sel, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
		mdelay(100);
	} else { /* Device */
		/* Disable hub */
		dm_gpio_set_dir_flags(&hub_reset, GPIOD_IS_OUT);
		/* Switch back to device connector */
		dm_gpio_set_dir_flags(&hub_reset, GPIOD_IS_OUT);
	}

#warning TODO: free gpio somehow
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
