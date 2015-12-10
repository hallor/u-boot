#include <config.h>

.global _fastboot_header
_fastboot_header:
	b _start
	add     x13, x18, #0x16
	// Image load offset from start of RAM, little-endian
	.quad   CONFIG_SYS_TEXT_BASE-PHYS_SDRAM_1
	// Effective size of kernel image, little-endian
	.quad   0 //0x60000
	// Informative flags, little-endian
	.quad   0
	.quad   0                               // reserved
	.quad   0                               // reserved
	.quad   0                               // reserved
	.byte   0x41                            // Magic number, "ARM\x64"
	.byte   0x52
	.byte   0x4d
	.byte   0x64
	.word   0                               // reserved
