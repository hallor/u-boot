/*
 * Qualcomm APQ8016 reset controller driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>

#define QCOM_PSHOLD 0x4ab000 /* Power-Supply hold?? */

void reset_cpu(ulong addr)
{
	writel(0, QCOM_PSHOLD);
	while(1);
}

