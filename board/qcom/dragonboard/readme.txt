Build & Run instructions:

1) Install mkbootimg from git://codeaurora.org/quic/kernel/skales (15ece94f09 worked for me)
2) Setup CROSS_COMPILE to aarch64 compiler
3) make dragonboard_config
4) make
5) run img.sh to wrap u-boot into fastboot compatible img

Boot it with fastboot:
fastboot boot u-boot.img
or flash as kernel:
fastboot flash boot u-boot.img
fastboot reboot


What is working (more-or-less):
- UART
- GPIO (SoC)
- SD
- eMMC (only in 4-bit mode)
- Reset
- USB in EHCI mode (usb starts does switch device->host, usb stop does the opposite)
- PMIC GPIOS (but not in generic subsystem)
- PMIC "special" buttons (power, vol-)

What is not working / known bugs:
- SDHCI is slow (~2MiB/s for SD, ~1MiB/s for eMMC)
- Writes to eMMC don't work (reads work though)
