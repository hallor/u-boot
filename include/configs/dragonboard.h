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
#include <asm/arch/sysmap.h>

#define CONFIG_IDENT_STRING		"\nQualcomm-DragonBoard 410C"

#define CONFIG_MISC_INIT_R /* To stop autoboot */

/* Flat Device Tree Definitions */
#define CONFIG_OF_LIBFDT

/* Physical Memory Map */
#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM_1			0x80000000
#define PHYS_SDRAM_1_SIZE		0x3da00000 /* 1008 MB (the last ~30Mb are secured for TrustZone by ATF*/
#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1
#define CONFIG_SYS_TEXT_BASE		0x80080000
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x7fff0)
#define CONFIG_SYS_LOAD_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x80000)
#define CONFIG_SYS_BOOTM_LEN		0x1000000 /* 16MB max kernel size */

/* Peripherals */
#define CONFIG_DM /* Use Device Model */

/* UART */
#define CONFIG_BAUDRATE			115200

/* Generic Timer Definitions */
#define COUNTER_FREQUENCY		19000000

/* SD/eMMC controller - SDHCI */
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

/* USB */
/* #define CONFIG_DM_USB */ /* Requires fdt binding */
#define CONFIG_USB_EHCI /* Host controller is EHCI */
#define CONFIG_USB_EHCI_MSM /* MSM flavour */
#define CONFIG_USB_ULPI_VIEWPORT /* ULPI is used to talk with USB phy */
/* Fixup - in init code we switch from device to host mode,
 * it has to be done after each HCD reset */
#define CONFIG_EHCI_HCD_INIT_AFTER_RESET

#define CONFIG_USB_STORAGE /* Enable USB Storage */
#define CONFIG_USB_HOST_ETHER /* Enable USB Networking */

/* Support all possible USB ethernet dongles */
#define CONFIG_USB_ETHER_DM9601
#define CONFIG_USB_ETHER_ASIX
#define CONFIG_USB_ETHER_ASIX88179
#define CONFIG_USB_ETHER_MCS7830
#define CONFIG_USB_ETHER_SMSC95XX

/* Libraries  */
#define CONFIG_MD5

/* Extra Commands */
#define CONFIG_CMD_CACHE
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_ENV
#define CONFIG_CMD_FAT		/* FAT support			*/
#define CONFIG_CMD_GPIO
#define CONFIG_CMD_GPT
#define CONFIG_CMD_LED
#define CONFIG_CMD_MD5SUM
#define CONFIG_CMD_MEMINFO	/* meminfo			*/
#define CONFIG_CMD_MMC
#define CONFIG_CMD_PART
#define CONFIG_CMD_PING
#define CONFIG_CMD_REGINFO	/* Register dump		*/
#define CONFIG_CMD_TFTP
#define CONFIG_CMD_TIMER
#define CONFIG_CMD_UNZIP
#define CONFIG_CMD_USB
#define CONFIG_CMD_BOOTZ
#define CONFIG_CMD_BOOTI

/* Command line configuration */
#define CONFIG_MENU
#define CONFIG_SYS_LONGHELP

/* Partition table support */
#define HAVE_BLOCK_DEVICE /* Needed for partition commands */
#define CONFIG_DOS_PARTITION
#define CONFIG_EFI_PARTITION
#define CONFIG_PARTITION_UUIDS

/* Support FIT images */
#define CONFIG_FIT

/* BOOTP options */
#define CONFIG_BOOTP_BOOTFILESIZE

/* Environment - Boot*/
#define CONFIG_BOOTDELAY		-1	/* autoboot after 5 seconds */

#define CONFIG_SERVERIP	10.0.0.1
#define CONFIG_IPADDR	10.0.0.2
#define CONFIG_NETMASK	255.255.255.0
#define CONFIG_NFSBOOTCOMMAND ""
#define CONFIG_ROOTPATH "/home/nfs/dragonboard"
#define CONFIG_BOOTFILE "dragonboard/linux.itb"
#define CONFIG_BOOTCOMMAND "usb start && tftp && usb stop && bootm"

/* Does what recovery does */
#define REFLASH(file, part) \
"part start mmc 0 "#part" start && "\
"part size mmc 0 "#part" size && "\
"tftp $loadaddr "#file" &&" \
"mmc write $loadaddr $start $size &&"


#define CONFIG_ENV_REFLASH \
"mmc dev 0 &&"\
"usb start &&"\
"tftp $loadaddr dragonboard/rescue/gpt_both0.bin && mmc write $loadaddr 0 64 &&"\
REFLASH(dragonboard/rescue/NON-HLOS.bin,1)\
REFLASH(dragonboard/rescue/sbl1.mbn,2)\
REFLASH(dragonboard/rescue/rpm.mbn,3)\
REFLASH(dragonboard/rescue/tz.mbn,4)\
REFLASH(dragonboard/rescue/hyp.mbn,5)\
REFLASH(dragonboard/rescue/sec.dat,6)\
REFLASH(dragonboard/rescue/emmc_appsboot.mbn,7)\
REFLASH(dragonboard/u-boot.img,8)\
"usb stop &&"\
"echo Reflash completed"

/* Environment */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"reflash="CONFIG_ENV_REFLASH"\0"\
	"loadaddr=0x81000000\0" \
	"linux_image=dragonboard/Image\0" \
	"linux_addr=0x81000000\0"\
	"fdt_image=dragonboard/apq8016-sbc.dtb\0" \
	"fdt_addr=0x83000000\0"\
	"ramdisk_addr=0x84000000\0"\
	"ramdisk_image=dragonboard/initrd.img\0" \
	"dl_uboot=tftp $loadaddr dragonboard/u-boot.img\0"\
	"dl_kernel=tftp $linux_addr $linux_image " \
		"&& tftp $fdt_addr $fdt_image\0"\
	"dl_ramdisk=tftp $ramdisk_addr $ramdisk_image\0"\
	"nboot_nord=usb start && run dl_kernel && usb stop && booti $linux_addr - $fdt_addr\0"\
	"nboot_rd=usb start && run dl_kernel && run dl_ramdisk && booti $linux_addr $ramdisk_addr $fdt_addr\0"\
	"test_network=usb start && dhcp; usb stop\0" \
	"test_mmc=mmc dev 0 && mmc erase 71020 1 && mmc write 0xBD956000 71020 1"\
		"&& mmc read $fdt_addr 71020 1 && cmp.b 0xBD956000 $fdt_addr 200\0"\
	"test_sd=mmc dev 1 && mmc erase 61460 1 && mmc write 0xBD956000 61460 1"\
		"&& mmc read $fdt_addr 61460 1 && cmp.b 0xBD956000 $fdt_addr 200\0" \
	"test_sdm=mmc dev 1 && mmc erase 61460 8 && mmc write 0xBD956000 61460 8"\
		"&& mmc read $fdt_addr 61460 8 && cmp.b 0xBD956000 $fdt_addr 1000\0"\
	"test=run test_mmc && run test_sd && run test_sdm && run test_network && reset\0" \
	"time_mmc=mmc dev 0; timer start; mmc read $loadaddr 0 5000; timer get\0"\
	"time_sd=mmc dev 1; timer start; mmc read $loadaddr 0 5000; timer get\0"\

#define CONFIG_ENV_IS_NOWHERE
#define CONFIG_ENV_SIZE		0x1000
#define CONFIG_ENV_VARS_UBOOT_CONFIG
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
