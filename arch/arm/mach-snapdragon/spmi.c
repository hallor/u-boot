/*
 * Qualcomm APQ8016 spmi controller driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Copied from LK/spmi, TODO: cleanup
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <asm/io.h>
#include <asm/arch/sysmap.h>
#include <malloc.h>
#include <linux/bitops.h>

#define PMIC_ARB_CORE			((phys_addr_t)0x200F000)
#define PMIC_ARB_REG_CHNL(n)		(PMIC_ARB_CORE + 0x800 + 0x4 * (n))

#define SPMI_CORE_BASE(chnl)	(MSM_SPMI_BASE + 0x400000 + (chnl) * 0x8000)
#define SPMI_OBS_BASE(chnl)	(MSM_SPMI_BASE + 0xC00000 + (chnl) * 0x8000)

#define SPMI_REG_CMD0			(0x0)
#define SPMI_REG_CONFIG			(0x4)
#define SPMI_REG_STATUS			(0x8)
#define SPMI_REG_WDATA			(0x10)
#define SPMI_REG_RDATA			(0x18)

#define SPMI_CMD_OPCODE_SHIFT		27
#define SPMI_CMD_SLAVE_ID_SHIFT		20
#define SPMI_CMD_ADDR_SHIFT		12
#define SPMI_CMD_ADDR_OFFSET_SHIFT	4
#define SPMI_CMD_BYTE_CNT_SHIFT		0

#define SPMI_CMD_EXT_REG_WRITE_LONG	0x00
#define SPMI_CMD_EXT_REG_READ_LONG	0x01

#define SPMI_STATUS_DONE		0x1

#define CHNL_IDX(sid, pid)		((sid << 8) | pid)

#define SLAVE_ID(addr)			((addr) >> 16)
#define PERIPH_ID(addr)			(((addr) & 0xFF00) >> 8)
#define REG_OFFSET(addr)		((addr) & 0xFF)

#define MAX_PERIPH			128

static uint8_t *chnl_tbl;

static void spmi_scan_channels(void)
{
	int i;
	uint8_t slave_id;
	uint8_t ppid_address;
	/* We need a max of sid (4 bits) + pid (8bits) of uint8_t's */
	uint32_t chnl_tbl_sz = BIT(12) * sizeof(uint8_t);

	/* Allocate the channel table */
	chnl_tbl = (uint8_t *)malloc(chnl_tbl_sz);

	for (i = 0; i < MAX_PERIPH ; i++) {
		uint32_t periph = readl(PMIC_ARB_REG_CHNL(i));

		slave_id = (periph & 0xf0000) >> 16;
		ppid_address = (periph & 0xff00) >> 8;
		chnl_tbl[CHNL_IDX(slave_id, ppid_address)] = i;
	}
}

int pmic_bus_read(uint32_t addr, uint8_t *val)
{
	unsigned channel = chnl_tbl[CHNL_IDX(SLAVE_ID(addr), PERIPH_ID(addr))];
	uint32_t reg = 0;

	/* Disable IRQ mode for the current channel*/
	writel(0x0, SPMI_OBS_BASE(channel) + SPMI_REG_CONFIG);

	/* Prepare read command */
	reg |= SPMI_CMD_EXT_REG_READ_LONG << SPMI_CMD_OPCODE_SHIFT;
	reg |= (SLAVE_ID(addr) << SPMI_CMD_SLAVE_ID_SHIFT);
	reg |= (PERIPH_ID(addr) << SPMI_CMD_ADDR_SHIFT);
	reg |= (REG_OFFSET(addr) << SPMI_CMD_ADDR_OFFSET_SHIFT);
	reg |= 1; /* byte count */

	/* Request read */
	writel(reg, SPMI_OBS_BASE(channel) + SPMI_REG_CMD0);

	/* Wait till CMD DONE status */
	while (!(reg = readl(SPMI_OBS_BASE(channel) + SPMI_REG_STATUS)))
		;

	if (reg ^ SPMI_STATUS_DONE) {
		printf("SPMI read failure.\n");
		return -EIO;
	}

	/* Read the data */
	*val = readl(SPMI_OBS_BASE(channel) + SPMI_REG_RDATA);

	return 0;
}

int pmic_bus_write(uint32_t addr, uint8_t val)
{
	unsigned channel = chnl_tbl[CHNL_IDX(SLAVE_ID(addr), PERIPH_ID(addr))];
	uint32_t reg = 0;

	/* Disable IRQ mode for the current channel*/
	writel(0x0, SPMI_CORE_BASE(channel) + SPMI_REG_CONFIG);

	/* Write single byte */
	writel(val, SPMI_CORE_BASE(channel) + SPMI_REG_WDATA);

	/* Prepare write command */
	reg |= SPMI_CMD_EXT_REG_WRITE_LONG << SPMI_CMD_OPCODE_SHIFT;
	reg |= (SLAVE_ID(addr) << SPMI_CMD_SLAVE_ID_SHIFT);
	reg |= (PERIPH_ID(addr) << SPMI_CMD_ADDR_SHIFT);
	reg |= (REG_OFFSET(addr) << SPMI_CMD_ADDR_OFFSET_SHIFT);
	reg |= 1; /* byte count */

	/* Send write command */
	writel(reg, SPMI_CORE_BASE(channel) + SPMI_REG_CMD0);

	/* Wait till CMD DONE status */
	while (!(reg = readl(SPMI_CORE_BASE(channel) + SPMI_REG_STATUS)))
		;

	if (reg ^ SPMI_STATUS_DONE) {
		printf("SPMI write failure.\n");
		return -EIO;
	}

	return 0;
}

int mach_spmi_init(void)
{
	spmi_scan_channels();
	return 0;
}
