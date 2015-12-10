/*
 * Qualcomm GPIO definitions
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef _MACH_GPIO_H
#define _MACH_GPIO_H

/* Register offsets */
#define GPIO_CONFIG_OFF(no)         ((no)*0x1000)
#define GPIO_IN_OUT_OFF(no)         ((no)*0x1000 + 0x4)

/* OE */
#define GPIO_OE_DISABLE	(0x0 << 9)
#define GPIO_OE_ENABLE	(0x1 << 9)
#define GPIO_OE_MASK	(0x1 << 9)

/* GPIO_IN_OUT register shifts. */
#define GPIO_IN         0
#define GPIO_OUT        1

#endif /* _ASM_ARCH_GPIO_H */

