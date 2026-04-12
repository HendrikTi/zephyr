/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_DISPLAY_ED2208_REGS_H_
#define ZEPHYR_DRIVERS_DISPLAY_ED2208_REGS_H_

/* ED2208 command registers */
#define ED2208_CMD_PSR			0x00
#define ED2208_CMD_PWRR			0x01
#define ED2208_CMD_POF			0x02
#define ED2208_CMD_POFS			0x03
#define ED2208_CMD_PON			0x04
#define ED2208_CMD_BTST1		0x05
#define ED2208_CMD_BTST2		0x06
#define ED2208_CMD_DSLP			0x07
#define ED2208_CMD_BTST3		0x08
#define ED2208_CMD_DTM			0x10
#define ED2208_CMD_DRF			0x12
#define ED2208_CMD_IPC			0x13
#define ED2208_CMD_PLL			0x30
#define ED2208_CMD_TSE			0x41
#define ED2208_CMD_CDI			0x50
#define ED2208_CMD_TCON			0x60
#define ED2208_CMD_TRES			0x61
#define ED2208_CMD_REV			0x70
#define ED2208_CMD_VDCS			0x82
#define ED2208_CMD_T_VDCS		0x84
#define ED2208_CMD_AGID			0x86
#define ED2208_CMD_CMDH			0xAA
#define ED2208_CMD_CCSET		0xE0
#define ED2208_CMD_PWS			0xE3
#define ED2208_CMD_TSSET		0xE6

/* ED2208 native color indices (4bpp) — for future color support */
#define ED2208_COLOR_WHITE		0x00
#define ED2208_COLOR_BLACK		0x01
#define ED2208_COLOR_GREEN		0x02
#define ED2208_COLOR_BLUE		0x03
#define ED2208_COLOR_RED		0x05
#define ED2208_COLOR_YELLOW		0x06

/* Mono mode: 8 pixels per input byte, expanded to 4 output bytes (4bpp) */
#define ED2208_MONO_PIXELS_PER_BYTE	8U

/* Time constants in ms */
#define ED2208_RESET_DELAY		20U
#define ED2208_BUSY_DELAY		10U

#endif /* ZEPHYR_DRIVERS_DISPLAY_ED2208_REGS_H_ */
