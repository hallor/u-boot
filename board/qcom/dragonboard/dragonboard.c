/*
 * Board init file for Dragonboard 410C
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>

 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>

U_BOOT_DEVICES(msm_gpios) = {
	{ "gpio_msm", NULL },
	{ "msm_sdc", (void*)1 }, // sdc1 == emmc
	{ "msm_sdc", (void*)2 }, // sdc2 == SD
	{ "msm_ehci", NULL },
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

int power_pm8916_init(void);
int power_init_board(void)
{
	return power_pm8916_init();
}

int board_init(void)
{
	return 0;
}
