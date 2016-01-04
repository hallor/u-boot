/*
 * Sample SPMI bus driver
 *
 * It emulates bus with single pm8916-like pmic that has only GPIO reigsters.
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <spmi/spmi.h>
#include <asm/gpio.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

#define EMUL_GPIO_PID_START 0xC0
#define EMUL_GPIO_PID_END   0xC3

#define EMUL_GPIO_COUNT 4

#define EMUL_GPIO_REG_END 0x46 /* Last valid register */

#define EMUL_PERM_R 0x1
#define EMUL_PERM_W 0x2
#define EMUL_PERM_RW (EMUL_PERM_R | EMUL_PERM_W)

struct sandbox_emul_fake_regs {
	u8 value;
	u8 access_mask;
	u8 perms; /* Access permissions */
};

struct sandbox_emul_gpio {
	struct sandbox_emul_fake_regs r[EMUL_GPIO_REG_END]; /* Fake registers */
};

struct sandbox_spmi_priv {
	struct sandbox_emul_gpio gpios[EMUL_GPIO_COUNT];
};

/* Check if valid register was requested */
static bool check_address_valid(int usid, int pid, int off)
{
	if (usid != 0)
		return false;
	if (pid < EMUL_GPIO_PID_START || pid > EMUL_GPIO_PID_END)
		return false;
	if (off > EMUL_GPIO_REG_END)
		return false;
	return true;
}

static int sandbox_spmi_write(struct udevice *dev, int usid, int pid, int off,
			  uint8_t val)
{
	struct sandbox_spmi_priv *p = dev_get_priv(dev);
	struct sandbox_emul_fake_regs *r;

	if (!check_address_valid(usid, pid, off))
		return -EIO;

	r = p->gpios[pid & 0x3].r; /* Last 3 bits of pid are gpio # */

	switch(off)
	{
	case 0x40: /* Control */
		val &= r[off].access_mask;
		if (((val & 0x30) == 0x10) || ((val & 0x30) == 0x20)) {
			/* out/inout - set status register */
			r[0x8].value &= ~0x1;
			r[0x8].value |= val & 0x1;
		}
	default:
		if (r[off].perms & EMUL_PERM_W)
			r[off].value = val & r[off].access_mask;
	}
	return 0;
}

static int sandbox_spmi_read(struct udevice *dev, int usid, int pid, int off)
{
	struct sandbox_spmi_priv *p = dev_get_priv(dev);
	struct sandbox_emul_fake_regs *r;

	if (!check_address_valid(usid, pid, off))
		return -EIO;

	r = p->gpios[pid & 0x3].r; /* Last 3 bits of pid are gpio # */

	if (r[0x46].value == 0) /* Block disabled */
		return 0;

	switch(off)
	{
	case 0x8: /* Status */
		if (r[0x46].value == 0) /* Block disabled */
			return 0;
		return r[off].value;
	default:
		if (r[off].perms & EMUL_PERM_R)
			return r[off].value;
		else
			return 0;
	}
}

static struct dm_spmi_ops sandbox_spmi_ops = {
	.read = sandbox_spmi_read,
	.write = sandbox_spmi_write,
};

static int sandbox_spmi_probe(struct udevice *dev)
{
	struct sandbox_spmi_priv *p = dev_get_priv(dev);
	int i;

	for (i=0; i<EMUL_GPIO_COUNT; ++i) {
		struct sandbox_emul_fake_regs *r = p->gpios[i].r;
		r[4].perms = EMUL_PERM_R;
		r[4].value = 0x10;
		r[5].perms = EMUL_PERM_R;
		r[5].value = 0x5;
		r[8].access_mask = 0x81;
		r[8].perms = EMUL_PERM_RW;
		r[0x40].access_mask = 0x7F;
		r[0x40].perms = EMUL_PERM_RW;
		r[0x41].access_mask = 7;
		r[0x41].perms = EMUL_PERM_RW;
		r[0x42].access_mask = 7;
		r[0x42].perms = EMUL_PERM_RW;
		r[0x42].value = 0x4;
		r[0x45].access_mask = 0x3F;
		r[0x45].perms = EMUL_PERM_RW;
		r[0x45].value = 0x1;
		r[0x46].access_mask = 0x80;
		r[0x46].perms = EMUL_PERM_RW;
		r[0x46].value = 0x80;
	}
	return 0;
}

static const struct udevice_id sandbox_spmi_ids[] = {
	{ .compatible = "sandbox,spmi" },
	{ }
};

U_BOOT_DRIVER(msm_spmi) = {
	.name = "sandbox_spmi",
	.id = UCLASS_SPMI,
	.of_match = sandbox_spmi_ids,
	.ops = &sandbox_spmi_ops,
	.probe = sandbox_spmi_probe,
	.priv_auto_alloc_size = sizeof(struct sandbox_spmi_priv),
};
