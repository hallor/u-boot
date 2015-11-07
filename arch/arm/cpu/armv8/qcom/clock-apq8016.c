/*
 * Clock drivers for Qualcomm APQ8016
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Based on Linux/Little-Kernel driver, simplified
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <asm/gpio.h>
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <linux/bitops.h>
#include <asm/arch/sysmap.h>


/* GPLL0 clock control registers */
#define GPLL0_STATUS        (CLK_CTL_BASE + 0x2101C)
#define GPLL0_STATUS_ACTIVE BIT(17)

#define APCS_GPLL_ENA_VOTE  (CLK_CTL_BASE + 0x45000)
#define APCS_GPLL_ENA_VOTE_GPLL0 BIT(0)

// vote reg for blsp1 clock
#define APCS_CLOCK_BRANCH_ENA_VOTE  (CLK_CTL_BASE + 0x45004)
#define APCS_CLOCK_BRANCH_ENA_VOTE_BLSP1 BIT(10)

/* SDC(n) clock control registers; n=1,2 */
#define SDCC_BCR(n)                   (CLK_CTL_BASE + ((n*0x1000)) + 0x41000) /* block reset*/
#define SDCC_CMD_RCGR(n)              (CLK_CTL_BASE + ((n*0x1000)) + 0x41004) /* cmd */
#define SDCC_CFG_RCGR(n)              (CLK_CTL_BASE + ((n*0x1000)) + 0x41008) /* cfg */
#define SDCC_M(n)                     (CLK_CTL_BASE + ((n*0x1000)) + 0x4100C) /* m */
#define SDCC_N(n)                     (CLK_CTL_BASE + ((n*0x1000)) + 0x41010) /* n */
#define SDCC_D(n)                     (CLK_CTL_BASE + ((n*0x1000)) + 0x41014) /* d */
#define SDCC_APPS_CBCR(n)             (CLK_CTL_BASE + ((n*0x1000)) + 0x41018) /* branch control */
#define SDCC_AHB_CBCR(n)              (CLK_CTL_BASE + ((n*0x1000)) + 0x4101C)

/* BLSP1 AHB clock (root clock for BLSP) */
#define BLSP1_AHB_CBCR              (CLK_CTL_BASE + 0x1008)

/* Uart clock control registers */
#define BLSP1_UART2_BCR             (CLK_CTL_BASE + 0x3028)
#define BLSP1_UART2_APPS_CBCR       (CLK_CTL_BASE + 0x302C)
#define BLSP1_UART2_APPS_CMD_RCGR   (CLK_CTL_BASE + 0x3034)
#define BLSP1_UART2_APPS_CFG_RCGR   (CLK_CTL_BASE + 0x3038)
#define BLSP1_UART2_APPS_M          (CLK_CTL_BASE + 0x303C)
#define BLSP1_UART2_APPS_N          (CLK_CTL_BASE + 0x3040)
#define BLSP1_UART2_APPS_D          (CLK_CTL_BASE + 0x3044)

/* CBCR register fields */
#define CBCR_BRANCH_ENABLE_BIT  BIT(0)
#define CBCR_BRANCH_OFF_BIT     BIT(31)

/* Enable clock controlled by CBC soft macro */
static void clk_enable_cbc(uintptr_t cbcr)
{
	uint32_t val = readl(cbcr);
	val |= CBCR_BRANCH_ENABLE_BIT;
	writel(val, cbcr);

	while(readl(cbcr) & CBCR_BRANCH_OFF_BIT);
}

// clock has 800MHz
static void clk_enable_gpll0(void)
{
	uint32_t ena;

	if (readl(GPLL0_STATUS) & GPLL0_STATUS_ACTIVE)  // clock already enabled
		return;

	ena = readl(APCS_GPLL_ENA_VOTE);
	ena |= APCS_GPLL_ENA_VOTE_GPLL0;
	writel(ena, APCS_GPLL_ENA_VOTE);

	while ((readl(GPLL0_STATUS) & GPLL0_STATUS_ACTIVE) == 0);
}

#define APPS_CMD_RGCR_UPDATE BIT(0)

/* Update clock command via CMD_RGCR */
static void clk_bcr_update(uintptr_t apps_cmd_rgcr)
{
	uint32_t cmd;

	cmd  = readl(apps_cmd_rgcr);
	cmd |= APPS_CMD_RGCR_UPDATE;
	writel(cmd, apps_cmd_rgcr);

	/* Wait for frequency to be updated. */
	while(readl(apps_cmd_rgcr) & APPS_CMD_RGCR_UPDATE);
}

struct bcr_regs
{
	uintptr_t cfg_rcgr;
	uintptr_t cmd_rcgr;
	uintptr_t M;
	uintptr_t N;
	uintptr_t D;
};

/* RCGR_CFG register fields */
#define CFG_MODE_DUAL_EDGE (0x2 << 12) // Counter mode

// sources
#define CFG_CLK_SRC_CXO   (0 << 8)
#define CFG_CLK_SRC_GPLL0 (1 << 8)
#define CFG_CLK_SRC_MASK  (7 << 8)

// Mask for supported fields
#define CFG_MASK (0x3FFF)

#define BM(msb, lsb)        (((((uint32_t)-1) << (31-msb)) >> (31-msb+lsb)) << lsb)
#define BVAL(msb, lsb, val) (((val) << lsb) & BM(msb, lsb))

/* root set rate for clocks with half integer and MND divider */
static void clk_rcg_set_rate_mnd(struct bcr_regs *regs, int div, int m, int n, int source)
{
	uint32_t cfg;
	/* Clock dividers/multipliers have to be tweaked for SoC */
	uint32_t m_val = m; // This register houses the M value for MND divider.
	uint32_t n_val = ~((n)-(m)) * !!(n); // This register houses the NOT(N-M) value for MND divider.
	uint32_t d_val = ~(n); // This register houses the NOT 2D value for MND divider.

	/* Program MND values */
	writel(m_val, regs->M); // M
	writel(n_val, regs->N); // N
	writel(d_val, regs->D); // D

	/* setup src select and divider */
	cfg  = readl(regs->cfg_rcgr);
	cfg &= ~CFG_MASK;
	cfg |= source & CFG_CLK_SRC_MASK; // Select clock source
	cfg |= BVAL(4, 0, (int)(2*(div) - 1))  | BVAL(10, 8, source);
	if (n_val)
		cfg |= CFG_MODE_DUAL_EDGE;

	writel(cfg, regs->cfg_rcgr); // Write new clock configuration

	/* Inform h/w to start using the new config. */
	clk_bcr_update(regs->cmd_rcgr);
}

struct bcr_regs sdc_regs[] =
{
	{
	.cfg_rcgr = SDCC_CFG_RCGR(1),
	.cmd_rcgr = SDCC_CMD_RCGR(1),
	.M = SDCC_M(1),
	.N = SDCC_N(1),
	.D = SDCC_D(1),
	},
	{
	.cfg_rcgr = SDCC_CFG_RCGR(2),
	.cmd_rcgr = SDCC_CMD_RCGR(2),
	.M = SDCC_M(2),
	.N = SDCC_N(2),
	.D = SDCC_D(2),
	}
};

/* Init clock for SDHCI controller */
int clk_init_sdc(int slot)
{
	int div;

	if (slot < 1 || slot > 2)
		return -1;

	if (slot == 1)
		div = 8; // 100Mhz for eMMC
	else
		div = 4; // 200Mhz for SD

	clk_enable_cbc(SDCC_AHB_CBCR(slot));
//	clk_rcg_set_rate_mnd(&sdc_regs[slot], 12, 1, 4, CFG_CLK_SRC_CXO); // 400kHz, cxo
#warning TODO: here, gpll0 source should be set - not cxo...
	clk_rcg_set_rate_mnd(&sdc_regs[slot - 1], div, 0, 0, CFG_CLK_SRC_CXO); // 800Mhz/div, gpll0
	clk_enable_gpll0();
	clk_enable_cbc(SDCC_APPS_CBCR(slot));
	return 0;
}

struct bcr_regs uart2_regs =
{
	.cfg_rcgr = BLSP1_UART2_APPS_CFG_RCGR,
	.cmd_rcgr = BLSP1_UART2_APPS_CMD_RCGR,
	.M = BLSP1_UART2_APPS_M,
	.N = BLSP1_UART2_APPS_N,
	.D = BLSP1_UART2_APPS_D,
};

/* Init UART clock, 115200 */
int clk_init_uart(void)
{
	clk_enable_cbc(BLSP1_AHB_CBCR); // Enable iface clk //
	clk_rcg_set_rate_mnd(&uart2_regs, 1, 144, 15625, CFG_CLK_SRC_GPLL0); // 7372800 uart block clock @ GPLL0
	clk_enable_gpll0();
	clk_enable_cbc(BLSP1_UART2_APPS_CBCR); // Enable core clk
	return 0;
}
