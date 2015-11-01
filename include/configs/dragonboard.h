/*
 * Board configuration file for Dragonboard 410C
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __DRAGONBOARD_H
#define __DRAGONBOARD_H

#include <linux/sizes.h>

#define CONFIG_REMAKE_ELF

#define CONFIG_IDENT_STRING		"\nQualcomm-DragonBoard 410C"

/* Flat Device Tree Definitions */
#define CONFIG_OF_LIBFDT

/* Physical Memory Map */

#define CONFIG_SYS_TEXT_BASE		0x80080000

#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM_1			0x80000000
#define PHYS_SDRAM_1_SIZE		0x3da00000 /* 1008 MB (the last ~30Mb are secured for TrustZone by ATF*/
#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1

/* Peripherals */

#define CONFIG_DM /* Use Device Model */

/* Generic Interrupt Controller Definitions */
#define GICD_BASE			0x0b000000
#define GICC_BASE			0x0a20c000

/* UART */
#define MSM_UART0_BASE			0x078B0000
#define CONFIG_BAUDRATE			115200

/* Generic Timer Definitions */
#define COUNTER_FREQUENCY		19000000

/* SDHCI */
#define CONFIG_SDHCI
#define CONFIG_MSM_SDHCI

/* This are needed to have proper mmc support */
#define CONFIG_MMC
#define CONFIG_DM_MMC
#define CONFIG_GENERIC_MMC

/* GPIO */
#define CONFIG_DM_GPIO
#define CONFIG_MSM_GPIO

/* PMIC */
#define CONFIG_POWER_PM8916

/* Status led */
#define CONFIG_GPIO_LED
#define CONFIG_STATUS_LED
#define CONFIG_BOARD_SPECIFIC_LED
#define STATUS_LED_BIT			21
/* Status LED polarity is inversed, so init it in the "off" state */
#define STATUS_LED_STATE		STATUS_LED_ON
#define STATUS_LED_PERIOD		(CONFIG_SYS_HZ / 2)
#define STATUS_LED_BOOT			0

/* Usual stuff */
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x7fff0)
#define CONFIG_SYS_LOAD_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x80000)

/* Extra Commands */
#define CONFIG_CMD_CACHE
#define CONFIG_CMD_ENV
#define CONFIG_CMD_FAT		/* FAT support			*/
#define CONFIG_CMD_GPIO
#define CONFIG_CMD_GPT
#define CONFIG_CMD_LED
#define CONFIG_CMD_MEMINFO	/* meminfo			*/
#define CONFIG_CMD_MMC
#define CONFIG_CMD_PART
#define CONFIG_CMD_REGINFO	/* Register dump		*/
#define CONFIG_CMD_TIMER
#define CONFIG_CMD_UNZIP

/* Command line configuration */
#define CONFIG_MENU
#define CONFIG_SYS_LONGHELP

/* Partition table support */
#define CONFIG_DOS_PARTITION
#define CONFIG_EFI_PARTITION
#define CONFIG_PARTITION_UUIDS
#define HAVE_BLOCK_DEVICE /* Needed for partition commands */

/* BOOTP options */
#define CONFIG_BOOTP_BOOTFILESIZE

/* Environment - Boot*/
#define CONFIG_BOOTDELAY		5	/* autoboot after 5 seconds */
#define CONFIG_BOOTCOMMAND "reset"
#define CONFIG_BOOTARGS ""

/* Environment */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"b=fatload mmc 1 0x80080000 u-boot.bin;go 0x80080000\0" \
	"ts=fatload mmc 1 0x90000000 Image.gz; fatload mmc 1 0x89000000 dragonboard.dtb\0" \
	"tm=fatload mmc 0 0x90000000 image/wcnss.b06\0"\
	"t=mmc dev 0; mmc info; mmc dev 1; mmc info; run ts; run tm;\0"\
	"tr=run t; reset\0"

#define CONFIG_ENV_IS_NOWHERE
#define CONFIG_ENV_SIZE		0x1000
#define CONFIG_SYS_NO_FLASH

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE + SZ_8M)

/* Monitor Command Prompt */
#define CONFIG_SYS_CBSIZE		512	/* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
#define CONFIG_SYS_LONGHELP
#define CONFIG_CMDLINE_EDITING
#define CONFIG_SYS_MAXARGS		64	/* max command args */


#endif /* __DRAGONBOARD_H */
