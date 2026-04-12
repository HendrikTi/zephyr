/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT waveshare_ed2208

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/sys/byteorder.h>

#include "ed2208_regs.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ed2208, CONFIG_DISPLAY_LOG_LEVEL);

struct ed2208_config {
	const struct device *mipi_dev;
	struct mipi_dbi_config dbi_config;
	struct gpio_dt_spec busy_gpio;
	uint16_t height;
	uint16_t width;
};

struct ed2208_data {
	bool blanking_on;
};

/* --- Low-level I/O ------------------------------------------------------- */

static inline void ed2208_busy_wait(const struct device *dev)
{
	const struct ed2208_config *config = dev->config;
	int pin = gpio_pin_get_dt(&config->busy_gpio);

	while (pin > 0) {
		__ASSERT(pin >= 0, "Failed to get pin level");
		k_sleep(K_MSEC(ED2208_BUSY_DELAY));
		pin = gpio_pin_get_dt(&config->busy_gpio);
	}
}

static inline int ed2208_write_cmd(const struct device *dev, uint8_t cmd,
				   const uint8_t *data, size_t len)
{
	const struct ed2208_config *config = dev->config;
	int err;

	ed2208_busy_wait(dev);

	err = mipi_dbi_command_write(config->mipi_dev, &config->dbi_config,
				     cmd, data, len);
	mipi_dbi_release(config->mipi_dev, &config->dbi_config);
	return err;
}

static inline int ed2208_write_cmd_uint8(const struct device *dev, uint8_t cmd,
					 uint8_t data)
{
	return ed2208_write_cmd(dev, cmd, &data, 1);
}

/* --- Init sequence -------------------------------------------------------- */

static int ed2208_controller_init(const struct device *dev)
{
	const struct ed2208_config *config = dev->config;
	struct ed2208_data *data = dev->data;
	uint16_t w_be = sys_cpu_to_be16(config->width);
	uint16_t h_be = sys_cpu_to_be16(config->height);
	int err;

	if (mipi_dbi_reset(config->mipi_dev, ED2208_RESET_DELAY) < 0) {
		return -EIO;
	}
	k_sleep(K_MSEC(ED2208_RESET_DELAY));

	/* CMDH — magic handshake */
	{
		static const uint8_t cmdh[] = {0x49, 0x55, 0x20, 0x08, 0x09, 0x18};

		err = ed2208_write_cmd(dev, ED2208_CMD_CMDH, cmdh, sizeof(cmdh));
		if (err) {
			return err;
		}
	}

	/* Power setting */
	{
		static const uint8_t pwr[] = {0x3F, 0x00, 0x32, 0x2A, 0x0E, 0x2A};

		err = ed2208_write_cmd(dev, ED2208_CMD_PWRR, pwr, sizeof(pwr));
		if (err) {
			return err;
		}
	}

	/* Panel setting */
	{
		static const uint8_t psr[] = {0x5F, 0x69};

		err = ed2208_write_cmd(dev, ED2208_CMD_PSR, psr, sizeof(psr));
		if (err) {
			return err;
		}
	}

	/* Power-off sequence */
	{
		static const uint8_t pofs[] = {0x00, 0x54, 0x00, 0x44};

		err = ed2208_write_cmd(dev, ED2208_CMD_POFS, pofs, sizeof(pofs));
		if (err) {
			return err;
		}
	}

	/* Booster soft-start 1 */
	{
		static const uint8_t btst1[] = {0x40, 0x1F, 0x1F, 0x2C};

		err = ed2208_write_cmd(dev, ED2208_CMD_BTST1, btst1, sizeof(btst1));
		if (err) {
			return err;
		}
	}

	/* Booster soft-start 2 */
	{
		static const uint8_t btst2[] = {0x6F, 0x1F, 0x16, 0x25};

		err = ed2208_write_cmd(dev, ED2208_CMD_BTST2, btst2, sizeof(btst2));
		if (err) {
			return err;
		}
	}

	/* Booster soft-start 3 */
	{
		static const uint8_t btst3[] = {0x6F, 0x1F, 0x1F, 0x22};

		err = ed2208_write_cmd(dev, ED2208_CMD_BTST3, btst3, sizeof(btst3));
		if (err) {
			return err;
		}
	}

	/* IPC */
	{
		static const uint8_t ipc[] = {0x00, 0x04};

		err = ed2208_write_cmd(dev, ED2208_CMD_IPC, ipc, sizeof(ipc));
		if (err) {
			return err;
		}
	}

	/* PLL */
	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_PLL, 0x02);
	if (err) {
		return err;
	}

	/* TSE */
	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_TSE, 0x00);
	if (err) {
		return err;
	}

	/* CDI */
	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_CDI, 0x3F);
	if (err) {
		return err;
	}

	/* TCON */
	{
		static const uint8_t tcon[] = {0x02, 0x00};

		err = ed2208_write_cmd(dev, ED2208_CMD_TCON, tcon, sizeof(tcon));
		if (err) {
			return err;
		}
	}

	/* TRES — resolution */
	{
		const uint8_t tres[] = {
			(config->width >> 8) & 0xFF,
			config->width & 0xFF,
			(config->height >> 8) & 0xFF,
			config->height & 0xFF,
		};

		err = ed2208_write_cmd(dev, ED2208_CMD_TRES, tres, sizeof(tres));
		if (err) {
			return err;
		}
	}

	/* VDCS */
	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_VDCS, 0x1E);
	if (err) {
		return err;
	}

	/* T_VDCS */
	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_T_VDCS, 0x01);
	if (err) {
		return err;
	}

	/* AGID */
	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_AGID, 0x00);
	if (err) {
		return err;
	}

	/* PWS */
	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_PWS, 0x2F);
	if (err) {
		return err;
	}

	/* CCSET */
	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_CCSET, 0x00);
	if (err) {
		return err;
	}

	/* TSSET */
	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_TSSET, 0x00);
	if (err) {
		return err;
	}

	/* Power on */
	err = ed2208_write_cmd(dev, ED2208_CMD_PON, NULL, 0);
	if (err) {
		return err;
	}
	ed2208_busy_wait(dev);

	data->blanking_on = true;

	return 0;
}

static int ed2208_init(const struct device *dev)
{
	const struct ed2208_config *config = dev->config;

	LOG_DBG("");

	if (!device_is_ready(config->mipi_dev)) {
		LOG_ERR("MIPI device not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&config->busy_gpio)) {
		LOG_ERR("Busy GPIO device not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&config->busy_gpio, GPIO_INPUT);

	return ed2208_controller_init(dev);
}

/* --- Display update / refresh -------------------------------------------- */

static int ed2208_update_display(const struct device *dev)
{
	int err;

	LOG_DBG("Trigger display refresh");

	err = ed2208_write_cmd_uint8(dev, ED2208_CMD_DRF, 0x00);
	if (err) {
		return err;
	}

	k_sleep(K_MSEC(1));
	ed2208_busy_wait(dev);

	return 0;
}

/* --- Display write (mono -> 4bpp expansion) ------------------------------ */

/*
 * Expand one byte of MONO10 data (8 pixels, MSB first) into 4 bytes of 4bpp
 * data suitable for the ED2208.
 *
 * MONO10: bit=1 means white, bit=0 means black.
 * ED2208: nibble 0x00 = white, 0x01 = black.
 */
static void ed2208_expand_mono_byte(uint8_t mono, uint8_t out[4])
{
	for (int i = 0; i < 4; i++) {
		uint8_t hi = (mono & 0x80) ? ED2208_COLOR_WHITE : ED2208_COLOR_BLACK;
		uint8_t lo = (mono & 0x40) ? ED2208_COLOR_WHITE : ED2208_COLOR_BLACK;

		out[i] = (hi << 4) | lo;
		mono <<= 2;
	}
}

static int ed2208_write(const struct device *dev, const uint16_t x,
			const uint16_t y,
			const struct display_buffer_descriptor *desc,
			const void *buf)
{
	const struct ed2208_config *config = dev->config;
	struct ed2208_data *data = dev->data;
	const uint8_t *src = buf;
	struct display_buffer_descriptor mipi_desc;
	uint16_t x_end_idx = x + desc->width - 1;
	uint16_t y_end_idx = y + desc->height - 1;
	size_t mono_bytes_per_row;
	int err;

	LOG_DBG("x %u, y %u, height %u, width %u, pitch %u",
		x, y, desc->height, desc->width, desc->pitch);

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller than width");
	__ASSERT(buf != NULL, "Buffer is not available");
	__ASSERT(!(desc->width % ED2208_MONO_PIXELS_PER_BYTE),
		 "Buffer width not multiple of %d", ED2208_MONO_PIXELS_PER_BYTE);

	if ((y_end_idx > (config->height - 1)) ||
	    (x_end_idx > (config->width - 1))) {
		LOG_ERR("Position out of bounds");
		return -EINVAL;
	}

	mono_bytes_per_row = desc->width / ED2208_MONO_PIXELS_PER_BYTE;

	/* Start data transfer — DTM command */
	ed2208_busy_wait(dev);
	err = mipi_dbi_command_write(config->mipi_dev, &config->dbi_config,
				     ED2208_CMD_DTM, NULL, 0);
	if (err) {
		goto out;
	}

	/*
	 * Expand mono data row by row into 4bpp and send.
	 * Each mono byte (8 pixels) becomes 4 output bytes (8 nibbles).
	 */
	mipi_desc.height = 1;

	for (uint16_t row = 0; row < desc->height; row++) {
		const uint8_t *row_src = src + row * (desc->pitch / ED2208_MONO_PIXELS_PER_BYTE);

		for (uint16_t col = 0; col < mono_bytes_per_row; col++) {
			uint8_t expanded[4];

			ed2208_expand_mono_byte(row_src[col], expanded);

			mipi_desc.buf_size = sizeof(expanded);
			mipi_desc.width = sizeof(expanded);
			mipi_desc.pitch = sizeof(expanded);

			err = mipi_dbi_write_display(config->mipi_dev,
						     &config->dbi_config,
						     expanded, &mipi_desc,
						     PIXEL_FORMAT_MONO10);
			if (err) {
				goto out;
			}
		}
	}

out:
	mipi_dbi_release(config->mipi_dev, &config->dbi_config);

	if (err) {
		return err;
	}

	if (!data->blanking_on) {
		return ed2208_update_display(dev);
	}

	return 0;
}

/* --- Blanking ------------------------------------------------------------- */

static int ed2208_blanking_off(const struct device *dev)
{
	struct ed2208_data *data = dev->data;

	if (data->blanking_on) {
		if (ed2208_update_display(dev)) {
			return -EIO;
		}
	}

	data->blanking_on = false;
	return 0;
}

static int ed2208_blanking_on(const struct device *dev)
{
	struct ed2208_data *data = dev->data;

	data->blanking_on = true;
	return 0;
}

/* --- Capabilities --------------------------------------------------------- */

static void ed2208_get_capabilities(const struct device *dev,
				    struct display_capabilities *caps)
{
	const struct ed2208_config *config = dev->config;

	memset(caps, 0, sizeof(struct display_capabilities));
	caps->x_resolution = config->width;
	caps->y_resolution = config->height;
	caps->supported_pixel_formats = PIXEL_FORMAT_MONO10;
	caps->current_pixel_format = PIXEL_FORMAT_MONO10;
	caps->screen_info = SCREEN_INFO_MONO_MSB_FIRST | SCREEN_INFO_EPD;
}

static int ed2208_set_pixel_format(const struct device *dev,
				   const enum display_pixel_format pf)
{
	if (pf == PIXEL_FORMAT_MONO10) {
		return 0;
	}

	LOG_ERR("Pixel format not supported");
	return -ENOTSUP;
}

/* --- Driver API ----------------------------------------------------------- */

static DEVICE_API(display, ed2208_driver_api) = {
	.blanking_on = ed2208_blanking_on,
	.blanking_off = ed2208_blanking_off,
	.write = ed2208_write,
	.get_capabilities = ed2208_get_capabilities,
	.set_pixel_format = ed2208_set_pixel_format,
};

/* --- Device instantiation ------------------------------------------------- */

#define ED2208_DEFINE(n)						\
	static const struct ed2208_config ed2208_cfg_##n = {		\
		.mipi_dev = DEVICE_DT_GET(DT_PARENT(n)),		\
		.dbi_config = {						\
			.mode = MIPI_DBI_MODE_SPI_4WIRE,		\
			.config = MIPI_DBI_SPI_CONFIG_DT(n,		\
					SPI_OP_MODE_MASTER |		\
					SPI_LOCK_ON | SPI_WORD_SET(8),	\
					0),				\
		},							\
		.busy_gpio = GPIO_DT_SPEC_GET(n, busy_gpios),		\
		.height = DT_PROP(n, height),				\
		.width = DT_PROP(n, width),				\
	};								\
									\
	static struct ed2208_data ed2208_data_##n;			\
									\
	DEVICE_DT_DEFINE(n, ed2208_init, NULL,				\
			 &ed2208_data_##n,				\
			 &ed2208_cfg_##n,				\
			 POST_KERNEL,					\
			 CONFIG_DISPLAY_INIT_PRIORITY,			\
			 &ed2208_driver_api);

DT_FOREACH_STATUS_OKAY(waveshare_ed2208, ED2208_DEFINE)
