#!/bin/sh
mkbootimg --kernel=u-boot.bin --output=u-boot.img --dt=fake_dt  --pagesize 2048 --base 0x80000000 --ramdisk=fake_ramdisk --cmdline=""
