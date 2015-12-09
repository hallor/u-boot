#
# (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
#
# SPDX-License-Identifier:	GPL-2.0+
#

Build & Run instructions:

1) Install mkbootimg from git://codeaurora.org/quic/kernel/skales (15ece94f09 worked for me)
2) Setup CROSS_COMPILE to aarch64 compiler
3) make dragonboard_config
4) make
5) generate fake, empty ramdisk (can have 0 bytes)
$ touch rd

6) generate qualcomm device tree, use dtbTool to generate it
$ dtbTool -o dt.img arch/arm/dts

7) generate image with mkbootimg:
$ mkbootimg --kernel=u-boot-dtb.bin --output=u-boot.img --dt=dt.img  --pagesize 2048 --base 0x80000000 --ramdisk=rd --cmdline=""

Boot it with fastboot:
fastboot boot u-boot.img
or flash as kernel:
fastboot flash boot u-boot.img
fastboot reboot


What is working:
- UART
- GPIO (SoC)
- SD
- eMMC
- Reset
- USB in EHCI mode (usb starts does switch device->host, usb stop does the opposite)
- PMIC GPIOS (but not in generic subsystem)
- PMIC "special" buttons (power, vol-)

What is not working / known bugs:
- SDHCI is slow (~2.5MiB/s for SD and eMMC)
