// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct novatek_sharp {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *supply;
	bool prepared;
};

static inline struct novatek_sharp *to_novatek_sharp(struct drm_panel *panel)
{
	return container_of(panel, struct novatek_sharp, panel);
}

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static int novatek_sharp_on(struct novatek_sharp *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	dsi_dcs_write_seq(dsi, 0xff, 0x01);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0x15, 0x14);
	dsi_dcs_write_seq(dsi, 0x16, 0x12);
	dsi_dcs_write_seq(dsi, 0x58, 0x82);
	dsi_dcs_write_seq(dsi, 0x59, 0x00);
	dsi_dcs_write_seq(dsi, 0x5a, 0x02);
	dsi_dcs_write_seq(dsi, 0x5b, 0x00);
	dsi_dcs_write_seq(dsi, 0x5c, 0x82);
	dsi_dcs_write_seq(dsi, 0x5d, 0x00);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_CABC_MIN_BRIGHTNESS, 0x02);
	dsi_dcs_write_seq(dsi, 0x5f, 0x00);
	dsi_dcs_write_seq(dsi, 0x60, 0x0f);
	dsi_dcs_write_seq(dsi, 0x6d, 0x22);
	dsi_dcs_write_seq(dsi, 0xff, 0x05);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0xa6, 0x04);
	dsi_dcs_write_seq(dsi, 0xc4, 0x04);
	dsi_dcs_write_seq(dsi, 0xff, 0x00);
	dsi_dcs_write_seq(dsi, 0xff, 0x01);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0x75, 0x00);
	dsi_dcs_write_seq(dsi, 0x76, 0x21);
	dsi_dcs_write_seq(dsi, 0x77, 0x00);
	dsi_dcs_write_seq(dsi, 0x78, 0x57);
	dsi_dcs_write_seq(dsi, 0x79, 0x00);
	dsi_dcs_write_seq(dsi, 0x7a, 0x8a);
	dsi_dcs_write_seq(dsi, 0x7b, 0x00);
	dsi_dcs_write_seq(dsi, 0x7c, 0xa9);
	dsi_dcs_write_seq(dsi, 0x7d, 0x00);
	dsi_dcs_write_seq(dsi, 0x7e, 0xc1);
	dsi_dcs_write_seq(dsi, 0x7f, 0x00);
	dsi_dcs_write_seq(dsi, 0x80, 0xd6);
	dsi_dcs_write_seq(dsi, 0x81, 0x00);
	dsi_dcs_write_seq(dsi, 0x82, 0xe4);
	dsi_dcs_write_seq(dsi, 0x83, 0x00);
	dsi_dcs_write_seq(dsi, 0x84, 0xf6);
	dsi_dcs_write_seq(dsi, 0x85, 0x01);
	dsi_dcs_write_seq(dsi, 0x86, 0x06);
	dsi_dcs_write_seq(dsi, 0x87, 0x01);
	dsi_dcs_write_seq(dsi, 0x88, 0x33);
	dsi_dcs_write_seq(dsi, 0x89, 0x01);
	dsi_dcs_write_seq(dsi, 0x8a, 0x59);
	dsi_dcs_write_seq(dsi, 0x8b, 0x01);
	dsi_dcs_write_seq(dsi, 0x8c, 0x95);
	dsi_dcs_write_seq(dsi, 0x8d, 0x01);
	dsi_dcs_write_seq(dsi, 0x8e, 0xc5);
	dsi_dcs_write_seq(dsi, 0x8f, 0x02);
	dsi_dcs_write_seq(dsi, 0x90, 0x11);
	dsi_dcs_write_seq(dsi, 0x91, 0x02);
	dsi_dcs_write_seq(dsi, 0x92, 0x4d);
	dsi_dcs_write_seq(dsi, 0x93, 0x02);
	dsi_dcs_write_seq(dsi, 0x94, 0x4f);
	dsi_dcs_write_seq(dsi, 0x95, 0x02);
	dsi_dcs_write_seq(dsi, 0x96, 0x77);
	dsi_dcs_write_seq(dsi, 0x97, 0x02);
	dsi_dcs_write_seq(dsi, 0x98, 0xa2);
	dsi_dcs_write_seq(dsi, 0x99, 0x02);
	dsi_dcs_write_seq(dsi, 0x9a, 0xbe);
	dsi_dcs_write_seq(dsi, 0x9b, 0x02);
	dsi_dcs_write_seq(dsi, 0x9c, 0xe5);
	dsi_dcs_write_seq(dsi, 0x9d, 0x03);
	dsi_dcs_write_seq(dsi, 0x9e, 0x04);
	dsi_dcs_write_seq(dsi, 0x9f, 0x03);
	dsi_dcs_write_seq(dsi, 0xa0, 0x2c);
	dsi_dcs_write_seq(dsi, MIPI_DCS_READ_PPS_START, 0x03);
	dsi_dcs_write_seq(dsi, 0xa3, 0x39);
	dsi_dcs_write_seq(dsi, 0xa4, 0x03);
	dsi_dcs_write_seq(dsi, 0xa5, 0x46);
	dsi_dcs_write_seq(dsi, 0xa6, 0x03);
	dsi_dcs_write_seq(dsi, 0xa7, 0x55);
	dsi_dcs_write_seq(dsi, MIPI_DCS_READ_PPS_CONTINUE, 0x03);
	dsi_dcs_write_seq(dsi, 0xaa, 0x66);
	dsi_dcs_write_seq(dsi, 0xab, 0x03);
	dsi_dcs_write_seq(dsi, 0xac, 0x7b);
	dsi_dcs_write_seq(dsi, 0xad, 0x03);
	dsi_dcs_write_seq(dsi, 0xae, 0x95);
	dsi_dcs_write_seq(dsi, 0xaf, 0x03);
	dsi_dcs_write_seq(dsi, 0xb0, 0xb4);
	dsi_dcs_write_seq(dsi, 0xb1, 0x03);
	dsi_dcs_write_seq(dsi, 0xb2, 0xbb);
	dsi_dcs_write_seq(dsi, 0xb3, 0x00);
	dsi_dcs_write_seq(dsi, 0xb4, 0x5b);
	dsi_dcs_write_seq(dsi, 0xb5, 0x00);
	dsi_dcs_write_seq(dsi, 0xb6, 0x94);
	dsi_dcs_write_seq(dsi, 0xb7, 0x00);
	dsi_dcs_write_seq(dsi, 0xb8, 0xc2);
	dsi_dcs_write_seq(dsi, 0xb9, 0x00);
	dsi_dcs_write_seq(dsi, 0xba, 0xde);
	dsi_dcs_write_seq(dsi, 0xbb, 0x00);
	dsi_dcs_write_seq(dsi, 0xbc, 0xf5);
	dsi_dcs_write_seq(dsi, 0xbd, 0x01);
	dsi_dcs_write_seq(dsi, 0xbe, 0x09);
	dsi_dcs_write_seq(dsi, 0xbf, 0x01);
	dsi_dcs_write_seq(dsi, 0xc0, 0x15);
	dsi_dcs_write_seq(dsi, 0xc1, 0x01);
	dsi_dcs_write_seq(dsi, 0xc2, 0x26);
	dsi_dcs_write_seq(dsi, 0xc3, 0x01);
	dsi_dcs_write_seq(dsi, 0xc4, 0x34);
	dsi_dcs_write_seq(dsi, 0xc5, 0x01);
	dsi_dcs_write_seq(dsi, 0xc6, 0x5c);
	dsi_dcs_write_seq(dsi, 0xc7, 0x01);
	dsi_dcs_write_seq(dsi, 0xc8, 0x7c);
	dsi_dcs_write_seq(dsi, 0xc9, 0x01);
	dsi_dcs_write_seq(dsi, 0xca, 0xac);
	dsi_dcs_write_seq(dsi, 0xcb, 0x01);
	dsi_dcs_write_seq(dsi, 0xcc, 0xd2);
	dsi_dcs_write_seq(dsi, 0xcd, 0x02);
	dsi_dcs_write_seq(dsi, 0xce, 0x0d);
	dsi_dcs_write_seq(dsi, 0xcf, 0x02);
	dsi_dcs_write_seq(dsi, 0xd0, 0x3b);
	dsi_dcs_write_seq(dsi, 0xd1, 0x02);
	dsi_dcs_write_seq(dsi, 0xd2, 0x3d);
	dsi_dcs_write_seq(dsi, 0xd3, 0x02);
	dsi_dcs_write_seq(dsi, 0xd4, 0x73);
	dsi_dcs_write_seq(dsi, 0xd5, 0x02);
	dsi_dcs_write_seq(dsi, 0xd6, 0xad);
	dsi_dcs_write_seq(dsi, 0xd7, 0x02);
	dsi_dcs_write_seq(dsi, 0xd8, 0xcf);
	dsi_dcs_write_seq(dsi, 0xd9, 0x02);
	dsi_dcs_write_seq(dsi, 0xda, 0xfe);
	dsi_dcs_write_seq(dsi, 0xdb, 0x03);
	dsi_dcs_write_seq(dsi, 0xdc, 0x1c);
	dsi_dcs_write_seq(dsi, 0xdd, 0x03);
	dsi_dcs_write_seq(dsi, 0xde, 0x44);
	dsi_dcs_write_seq(dsi, 0xdf, 0x03);
	dsi_dcs_write_seq(dsi, 0xe0, 0x51);
	dsi_dcs_write_seq(dsi, 0xe1, 0x03);
	dsi_dcs_write_seq(dsi, 0xe2, 0x5e);
	dsi_dcs_write_seq(dsi, 0xe3, 0x03);
	dsi_dcs_write_seq(dsi, 0xe4, 0x6e);
	dsi_dcs_write_seq(dsi, 0xe5, 0x03);
	dsi_dcs_write_seq(dsi, 0xe6, 0x7f);
	dsi_dcs_write_seq(dsi, 0xe7, 0x03);
	dsi_dcs_write_seq(dsi, 0xe8, 0x93);
	dsi_dcs_write_seq(dsi, 0xe9, 0x03);
	dsi_dcs_write_seq(dsi, 0xea, 0xac);
	dsi_dcs_write_seq(dsi, 0xeb, 0x03);
	dsi_dcs_write_seq(dsi, 0xec, 0xca);
	dsi_dcs_write_seq(dsi, 0xed, 0x03);
	dsi_dcs_write_seq(dsi, 0xee, 0xd1);
	dsi_dcs_write_seq(dsi, 0xef, 0x00);
	dsi_dcs_write_seq(dsi, 0xf0, 0x14);
	dsi_dcs_write_seq(dsi, 0xf1, 0x00);
	dsi_dcs_write_seq(dsi, 0xf2, 0x50);
	dsi_dcs_write_seq(dsi, 0xf3, 0x00);
	dsi_dcs_write_seq(dsi, 0xf4, 0x84);
	dsi_dcs_write_seq(dsi, 0xf5, 0x00);
	dsi_dcs_write_seq(dsi, 0xf6, 0xa4);
	dsi_dcs_write_seq(dsi, 0xf7, 0x00);
	dsi_dcs_write_seq(dsi, 0xf8, 0xb9);
	dsi_dcs_write_seq(dsi, 0xf9, 0x00);
	dsi_dcs_write_seq(dsi, 0xfa, 0xd2);
	dsi_dcs_write_seq(dsi, 0xff, 0x02);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0x01, 0xe1);
	dsi_dcs_write_seq(dsi, 0x02, 0x00);
	dsi_dcs_write_seq(dsi, 0x03, 0xf1);
	dsi_dcs_write_seq(dsi, 0x04, 0x01);
	dsi_dcs_write_seq(dsi, 0x05, 0x02);
	dsi_dcs_write_seq(dsi, 0x06, 0x01);
	dsi_dcs_write_seq(dsi, 0x07, 0x31);
	dsi_dcs_write_seq(dsi, 0x08, 0x01);
	dsi_dcs_write_seq(dsi, 0x09, 0x56);
	dsi_dcs_write_seq(dsi, 0x0a, 0x01);
	dsi_dcs_write_seq(dsi, 0x0b, 0x93);
	dsi_dcs_write_seq(dsi, 0x0c, 0x01);
	dsi_dcs_write_seq(dsi, 0x0d, 0xc4);
	dsi_dcs_write_seq(dsi, 0x0e, 0x02);
	dsi_dcs_write_seq(dsi, 0x0f, 0x0f);
	dsi_dcs_write_seq(dsi, 0x10, 0x02);
	dsi_dcs_write_seq(dsi, 0x11, 0x4b);
	dsi_dcs_write_seq(dsi, 0x12, 0x02);
	dsi_dcs_write_seq(dsi, 0x13, 0x4c);
	dsi_dcs_write_seq(dsi, 0x14, 0x02);
	dsi_dcs_write_seq(dsi, 0x15, 0x75);
	dsi_dcs_write_seq(dsi, 0x16, 0x02);
	dsi_dcs_write_seq(dsi, 0x17, 0xa0);
	dsi_dcs_write_seq(dsi, 0x18, 0x02);
	dsi_dcs_write_seq(dsi, 0x19, 0xbb);
	dsi_dcs_write_seq(dsi, 0x1a, 0x02);
	dsi_dcs_write_seq(dsi, 0x1b, 0xe0);
	dsi_dcs_write_seq(dsi, 0x1c, 0x02);
	dsi_dcs_write_seq(dsi, 0x1d, 0xff);
	dsi_dcs_write_seq(dsi, 0x1e, 0x03);
	dsi_dcs_write_seq(dsi, 0x1f, 0x27);
	dsi_dcs_write_seq(dsi, 0x20, 0x03);
	dsi_dcs_write_seq(dsi, 0x21, 0x33);
	dsi_dcs_write_seq(dsi, 0x22, 0x03);
	dsi_dcs_write_seq(dsi, 0x23, 0x41);
	dsi_dcs_write_seq(dsi, 0x24, 0x03);
	dsi_dcs_write_seq(dsi, 0x25, 0x50);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_GAMMA_CURVE, 0x03);
	dsi_dcs_write_seq(dsi, 0x27, 0x61);
	dsi_dcs_write_seq(dsi, 0x28, 0x03);
	dsi_dcs_write_seq(dsi, 0x29, 0x76);
	dsi_dcs_write_seq(dsi, 0x2a, 0x03);
	dsi_dcs_write_seq(dsi, 0x2b, 0x91);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_LUT, 0x03);
	dsi_dcs_write_seq(dsi, 0x2f, 0xb3);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_ROWS, 0x03);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_COLUMNS, 0xbb);
	dsi_dcs_write_seq(dsi, 0x32, 0x00);
	dsi_dcs_write_seq(dsi, 0x33, 0x4e);
	dsi_dcs_write_seq(dsi, 0x34, 0x00);
	dsi_dcs_write_seq(dsi, 0x35, 0x8f);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_ADDRESS_MODE, 0x00);
	dsi_dcs_write_seq(dsi, 0x37, 0xbe);
	dsi_dcs_write_seq(dsi, 0x38, 0x00);
	dsi_dcs_write_seq(dsi, 0x39, 0xdb);

	ret = mipi_dsi_dcs_set_pixel_format(dsi, 0x00);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, 0x3b, 0xef);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_3D_CONTROL, 0x01);
	dsi_dcs_write_seq(dsi, 0x3f, 0x06);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_VSYNC_TIMING, 0x01);
	dsi_dcs_write_seq(dsi, 0x41, 0x13);
	dsi_dcs_write_seq(dsi, 0x42, 0x01);
	dsi_dcs_write_seq(dsi, 0x43, 0x22);
	dsi_dcs_write_seq(dsi, 0x44, 0x01);
	dsi_dcs_write_seq(dsi, MIPI_DCS_GET_SCANLINE, 0x31);
	dsi_dcs_write_seq(dsi, 0x46, 0x01);
	dsi_dcs_write_seq(dsi, 0x47, 0x5b);
	dsi_dcs_write_seq(dsi, 0x48, 0x01);
	dsi_dcs_write_seq(dsi, 0x49, 0x7b);
	dsi_dcs_write_seq(dsi, 0x4a, 0x01);
	dsi_dcs_write_seq(dsi, 0x4b, 0xab);
	dsi_dcs_write_seq(dsi, 0x4c, 0x01);
	dsi_dcs_write_seq(dsi, 0x4d, 0xd1);
	dsi_dcs_write_seq(dsi, 0x4e, 0x02);
	dsi_dcs_write_seq(dsi, 0x4f, 0x0b);
	dsi_dcs_write_seq(dsi, 0x50, 0x02);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x0039);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, 0x52, 0x02);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x3a);
	dsi_dcs_write_seq(dsi, 0x54, 0x02);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x71);
	dsi_dcs_write_seq(dsi, 0x56, 0x02);
	dsi_dcs_write_seq(dsi, 0x58, 0xaa);
	dsi_dcs_write_seq(dsi, 0x59, 0x02);
	dsi_dcs_write_seq(dsi, 0x5a, 0xcc);
	dsi_dcs_write_seq(dsi, 0x5b, 0x02);
	dsi_dcs_write_seq(dsi, 0x5c, 0xfa);
	dsi_dcs_write_seq(dsi, 0x5d, 0x03);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_CABC_MIN_BRIGHTNESS, 0x17);
	dsi_dcs_write_seq(dsi, 0x5f, 0x03);
	dsi_dcs_write_seq(dsi, 0x60, 0x3f);
	dsi_dcs_write_seq(dsi, 0x61, 0x03);
	dsi_dcs_write_seq(dsi, 0x62, 0x4b);
	dsi_dcs_write_seq(dsi, 0x63, 0x03);
	dsi_dcs_write_seq(dsi, 0x64, 0x59);
	dsi_dcs_write_seq(dsi, 0x65, 0x03);
	dsi_dcs_write_seq(dsi, 0x66, 0x68);
	dsi_dcs_write_seq(dsi, 0x67, 0x03);
	dsi_dcs_write_seq(dsi, 0x68, 0x7a);
	dsi_dcs_write_seq(dsi, 0x69, 0x03);
	dsi_dcs_write_seq(dsi, 0x6a, 0x8e);
	dsi_dcs_write_seq(dsi, 0x6b, 0x03);
	dsi_dcs_write_seq(dsi, 0x6c, 0xa8);
	dsi_dcs_write_seq(dsi, 0x6d, 0x03);
	dsi_dcs_write_seq(dsi, 0x6e, 0xc9);
	dsi_dcs_write_seq(dsi, 0x6f, 0x03);
	dsi_dcs_write_seq(dsi, 0x70, 0xd1);
	dsi_dcs_write_seq(dsi, 0x71, 0x00);
	dsi_dcs_write_seq(dsi, 0x72, 0x00);
	dsi_dcs_write_seq(dsi, 0x73, 0x00);
	dsi_dcs_write_seq(dsi, 0x74, 0x38);
	dsi_dcs_write_seq(dsi, 0x75, 0x00);
	dsi_dcs_write_seq(dsi, 0x76, 0x6c);
	dsi_dcs_write_seq(dsi, 0x77, 0x00);
	dsi_dcs_write_seq(dsi, 0x78, 0x8f);
	dsi_dcs_write_seq(dsi, 0x79, 0x00);
	dsi_dcs_write_seq(dsi, 0x7a, 0xaa);
	dsi_dcs_write_seq(dsi, 0x7b, 0x00);
	dsi_dcs_write_seq(dsi, 0x7c, 0xbf);
	dsi_dcs_write_seq(dsi, 0x7d, 0x00);
	dsi_dcs_write_seq(dsi, 0x7e, 0xd5);
	dsi_dcs_write_seq(dsi, 0x7f, 0x00);
	dsi_dcs_write_seq(dsi, 0x80, 0xe2);
	dsi_dcs_write_seq(dsi, 0x81, 0x00);
	dsi_dcs_write_seq(dsi, 0x82, 0xf3);
	dsi_dcs_write_seq(dsi, 0x83, 0x01);
	dsi_dcs_write_seq(dsi, 0x84, 0x27);
	dsi_dcs_write_seq(dsi, 0x85, 0x01);
	dsi_dcs_write_seq(dsi, 0x86, 0x4e);
	dsi_dcs_write_seq(dsi, 0x87, 0x01);
	dsi_dcs_write_seq(dsi, 0x88, 0x8d);
	dsi_dcs_write_seq(dsi, 0x89, 0x01);
	dsi_dcs_write_seq(dsi, 0x8a, 0xc0);
	dsi_dcs_write_seq(dsi, 0x8b, 0x02);
	dsi_dcs_write_seq(dsi, 0x8c, 0x0d);
	dsi_dcs_write_seq(dsi, 0x8d, 0x02);
	dsi_dcs_write_seq(dsi, 0x8e, 0x4b);
	dsi_dcs_write_seq(dsi, 0x8f, 0x02);
	dsi_dcs_write_seq(dsi, 0x90, 0x4c);
	dsi_dcs_write_seq(dsi, 0x91, 0x02);
	dsi_dcs_write_seq(dsi, 0x92, 0x76);
	dsi_dcs_write_seq(dsi, 0x93, 0x02);
	dsi_dcs_write_seq(dsi, 0x94, 0xa1);
	dsi_dcs_write_seq(dsi, 0x95, 0x02);
	dsi_dcs_write_seq(dsi, 0x96, 0xbd);
	dsi_dcs_write_seq(dsi, 0x97, 0x02);
	dsi_dcs_write_seq(dsi, 0x98, 0xe4);
	dsi_dcs_write_seq(dsi, 0x99, 0x03);
	dsi_dcs_write_seq(dsi, 0x9a, 0x03);
	dsi_dcs_write_seq(dsi, 0x9b, 0x03);
	dsi_dcs_write_seq(dsi, 0x9c, 0x2d);
	dsi_dcs_write_seq(dsi, 0x9d, 0x03);
	dsi_dcs_write_seq(dsi, 0x9e, 0x39);
	dsi_dcs_write_seq(dsi, 0x9f, 0x03);
	dsi_dcs_write_seq(dsi, 0xa0, 0x46);
	dsi_dcs_write_seq(dsi, MIPI_DCS_READ_PPS_START, 0x03);
	dsi_dcs_write_seq(dsi, 0xa3, 0x56);
	dsi_dcs_write_seq(dsi, 0xa4, 0x03);
	dsi_dcs_write_seq(dsi, 0xa5, 0x68);
	dsi_dcs_write_seq(dsi, 0xa6, 0x03);
	dsi_dcs_write_seq(dsi, 0xa7, 0x7f);
	dsi_dcs_write_seq(dsi, MIPI_DCS_READ_PPS_CONTINUE, 0x03);
	dsi_dcs_write_seq(dsi, 0xaa, 0x97);
	dsi_dcs_write_seq(dsi, 0xab, 0x03);
	dsi_dcs_write_seq(dsi, 0xac, 0xb5);
	dsi_dcs_write_seq(dsi, 0xad, 0x03);
	dsi_dcs_write_seq(dsi, 0xae, 0xbb);
	dsi_dcs_write_seq(dsi, 0xaf, 0x00);
	dsi_dcs_write_seq(dsi, 0xb0, 0x39);
	dsi_dcs_write_seq(dsi, 0xb1, 0x00);
	dsi_dcs_write_seq(dsi, 0xb2, 0x76);
	dsi_dcs_write_seq(dsi, 0xb3, 0x00);
	dsi_dcs_write_seq(dsi, 0xb4, 0xa6);
	dsi_dcs_write_seq(dsi, 0xb5, 0x00);
	dsi_dcs_write_seq(dsi, 0xb6, 0xc6);
	dsi_dcs_write_seq(dsi, 0xb7, 0x00);
	dsi_dcs_write_seq(dsi, 0xb8, 0xdf);
	dsi_dcs_write_seq(dsi, 0xb9, 0x00);
	dsi_dcs_write_seq(dsi, 0xba, 0xf2);
	dsi_dcs_write_seq(dsi, 0xbb, 0x01);
	dsi_dcs_write_seq(dsi, 0xbc, 0x07);
	dsi_dcs_write_seq(dsi, 0xbd, 0x01);
	dsi_dcs_write_seq(dsi, 0xbe, 0x13);
	dsi_dcs_write_seq(dsi, 0xbf, 0x01);
	dsi_dcs_write_seq(dsi, 0xc0, 0x22);
	dsi_dcs_write_seq(dsi, 0xc1, 0x01);
	dsi_dcs_write_seq(dsi, 0xc2, 0x50);
	dsi_dcs_write_seq(dsi, 0xc3, 0x01);
	dsi_dcs_write_seq(dsi, 0xc4, 0x72);
	dsi_dcs_write_seq(dsi, 0xc5, 0x01);
	dsi_dcs_write_seq(dsi, 0xc6, 0xa5);
	dsi_dcs_write_seq(dsi, 0xc7, 0x01);
	dsi_dcs_write_seq(dsi, 0xc8, 0xcd);
	dsi_dcs_write_seq(dsi, 0xc9, 0x02);
	dsi_dcs_write_seq(dsi, 0xca, 0x09);
	dsi_dcs_write_seq(dsi, 0xcb, 0x02);
	dsi_dcs_write_seq(dsi, 0xcc, 0x39);
	dsi_dcs_write_seq(dsi, 0xcd, 0x02);
	dsi_dcs_write_seq(dsi, 0xce, 0x3a);
	dsi_dcs_write_seq(dsi, 0xcf, 0x02);
	dsi_dcs_write_seq(dsi, 0xd0, 0x72);
	dsi_dcs_write_seq(dsi, 0xd1, 0x02);
	dsi_dcs_write_seq(dsi, 0xd2, 0xac);
	dsi_dcs_write_seq(dsi, 0xd3, 0x02);
	dsi_dcs_write_seq(dsi, 0xd4, 0xcf);
	dsi_dcs_write_seq(dsi, 0xd5, 0x02);
	dsi_dcs_write_seq(dsi, 0xd6, 0xfe);
	dsi_dcs_write_seq(dsi, 0xd7, 0x03);
	dsi_dcs_write_seq(dsi, 0xd8, 0x1c);
	dsi_dcs_write_seq(dsi, 0xd9, 0x03);
	dsi_dcs_write_seq(dsi, 0xda, 0x45);
	dsi_dcs_write_seq(dsi, 0xdb, 0x03);
	dsi_dcs_write_seq(dsi, 0xdc, 0x51);
	dsi_dcs_write_seq(dsi, 0xdd, 0x03);
	dsi_dcs_write_seq(dsi, 0xde, 0x5f);
	dsi_dcs_write_seq(dsi, 0xdf, 0x03);
	dsi_dcs_write_seq(dsi, 0xe0, 0x6f);
	dsi_dcs_write_seq(dsi, 0xe1, 0x03);
	dsi_dcs_write_seq(dsi, 0xe2, 0x81);
	dsi_dcs_write_seq(dsi, 0xe3, 0x03);
	dsi_dcs_write_seq(dsi, 0xe4, 0x97);
	dsi_dcs_write_seq(dsi, 0xe5, 0x03);
	dsi_dcs_write_seq(dsi, 0xe6, 0xae);
	dsi_dcs_write_seq(dsi, 0xe7, 0x03);
	dsi_dcs_write_seq(dsi, 0xe8, 0xcb);
	dsi_dcs_write_seq(dsi, 0xe9, 0x03);
	dsi_dcs_write_seq(dsi, 0xea, 0xd1);
	dsi_dcs_write_seq(dsi, 0xff, 0x00);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(150);

	dsi_dcs_write_seq(dsi, 0xd3, 0x07);
	dsi_dcs_write_seq(dsi, 0xd4, 0x02);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}

	return 0;
}

static int novatek_sharp_off(struct novatek_sharp *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(100);

	return 0;
}

static int novatek_sharp_prepare(struct drm_panel *panel)
{
	struct novatek_sharp *ctx = to_novatek_sharp(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->supply);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulator: %d\n", ret);
		return ret;
	}

	ret = novatek_sharp_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		regulator_disable(ctx->supply);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int novatek_sharp_unprepare(struct drm_panel *panel)
{
	struct novatek_sharp *ctx = to_novatek_sharp(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = novatek_sharp_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	regulator_disable(ctx->supply);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode novatek_sharp_mode = {
	.clock = (1080 + 128 + 4 + 76) * (1920 + 2 + 2 + 5) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 128,
	.hsync_end = 1080 + 128 + 4,
	.htotal = 1080 + 128 + 4 + 76,
	.vdisplay = 1920,
	.vsync_start = 1920 + 2,
	.vsync_end = 1920 + 2 + 2,
	.vtotal = 1920 + 2 + 2 + 5,
	.width_mm = 64,
	.height_mm = 114,
};

static int novatek_sharp_get_modes(struct drm_panel *panel,
				   struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &novatek_sharp_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs novatek_sharp_panel_funcs = {
	.prepare = novatek_sharp_prepare,
	.unprepare = novatek_sharp_unprepare,
	.get_modes = novatek_sharp_get_modes,
};

static int novatek_sharp_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct novatek_sharp *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->supply = devm_regulator_get(dev, "vddio");
	if (IS_ERR(ctx->supply))
		return dev_err_probe(dev, PTR_ERR(ctx->supply),
				     "Failed to get vddio regulator\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_HSE |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &novatek_sharp_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static int novatek_sharp_remove(struct mipi_dsi_device *dsi)
{
	struct novatek_sharp *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id novatek_sharp_of_match[] = {
	{ .compatible = "novatek,sharp" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, novatek_sharp_of_match);

static struct mipi_dsi_driver novatek_sharp_driver = {
	.probe = novatek_sharp_probe,
	.remove = novatek_sharp_remove,
	.driver = {
		.name = "panel-novatek-sharp",
		.of_match_table = novatek_sharp_of_match,
	},
};
module_mipi_dsi_driver(novatek_sharp_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for sharp novatek 1080p video");
MODULE_LICENSE("GPL v2");
