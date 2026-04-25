/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <lvgl.h>
#include "lvgl_display.h"
#include <zephyr/logging/log.h>

/* I4 palette: 16 entries × 4 bytes (lv_color32_t / RGB656) */
#define I4_PALETTE_HEADER_SIZE (16U * 2U)
#define BUFFER_SIZE (800*480/2)
LOG_MODULE_REGISTER(lvgl_indexed, CONFIG_LV_Z_LOG_LEVEL);

static uint8_t buf0[BUFFER_SIZE];		

/* Spectra 6 (E6) panel CLUT slots. Slots 4 and 7 are unused by the
 * controller — verify against the reTerminal E1002 driver if the
 * output looks scrambled.
 */
#define EPD_BLACK  0x0U
#define EPD_WHITE  0x1U
#define EPD_YELLOW 0x2U
#define EPD_RED    0x3U
#define EPD_BLUE   0x5U
#define EPD_GREEN  0x6U

static uint8_t map_to_palette(uint16_t px)
{
	switch (px) {
	case 0x0000: return EPD_BLACK;  /* R=0,  G=0,  B=0  */
	case 0xFFFF: return EPD_WHITE;  /* R=31, G=63, B=31 */
	case 0xFFE0: return EPD_YELLOW; /* R=31, G=63, B=0  */
	case 0xF800: return EPD_RED;    /* R=31, G=0,  B=0  */
	case 0x001F: return EPD_BLUE;   /* R=0,  G=0,  B=31 */
	case 0x07E0: return EPD_GREEN;  /* R=0,  G=63, B=0  */
	default:     return EPD_WHITE;
	}
}

void lvgl_flush_cb_indexed(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{

	// We need to convert RGB565 to I4
	struct lvgl_disp_data *data = (struct lvgl_disp_data *)lv_display_get_user_data(display);
	const struct device *display_dev = data->display_dev;
	const bool is_epd = data->cap.screen_info & SCREEN_INFO_EPD;
	const bool is_last = lv_display_flush_is_last(display);
	int w = 800/2; 
	uint16_t w_in = area->x2 - area->x1 + 1;
	uint16_t h_in = area->y2 - area->y1 + 1;
	uint16_t * buf16 = (uint16_t *)px_map;
	for(int y = area->y1; y <= area->y2; y++) {
		for(int x = area->x1; x <= area->x2; x++) {
			uint16_t in_px;
			int x_in = x-area->x1;
			int y_in = y-area->y1;
			memcpy(&in_px, &buf16[x_in+(y_in*w_in)], sizeof(uint16_t));
			uint8_t px = map_to_palette(in_px); 
			if ((area->x1-x & 0x1)) {
				// odd
				buf0[y*w+(x/2)] = (buf0[y*w+(x/2)] & 0xF0) | (px);    
			} else {
				// even
				buf0[y*w+(x/2)] = (buf0[y*w+(x/2)] & 0x0F) | (px << 4);
			} 
			LOG_ERR("out: %x" , buf0[y*(x/2)]);
		}
	}
	if(is_last) {
		struct display_buffer_descriptor desc = {
			.buf_size = DIV_ROUND_UP(800 * 480, 2),
			.width = 800,
			.pitch = ROUND_UP(DIV_ROUND_UP(800, 2), LV_DRAW_BUF_STRIDE_ALIGN) * 2,
			.height = 480,
			.frame_incomplete = !is_last,
		};
		display_write(display_dev, 0, 0, &desc, (void *)buf0);
		if (is_epd && is_last && data->blanking_on) {
			/*
			* The entire screen has now been rendered. Update the
			* display by disabling blanking.
			*/
			display_blanking_off(display_dev);
			data->blanking_on = false;
		}
	}
	lv_display_flush_ready(display);
}