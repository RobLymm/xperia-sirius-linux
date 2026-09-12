// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct novatek_jdi {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *supply;
	bool prepared;
};

static inline struct novatek_jdi *to_novatek_jdi(struct drm_panel *panel)
{
	return container_of(panel, struct novatek_jdi, panel);
}

#define dsi_generic_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_generic_write(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static int novatek_jdi_on(struct novatek_jdi *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_soft_reset(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to soft reset: %d\n", ret);
		return ret;
	}
	usleep_range(10000, 11000);

	dsi_generic_write_seq(dsi, 0xff, 0x01);
	dsi_generic_write_seq(dsi, 0x75, 0x00);
	dsi_generic_write_seq(dsi, 0x76, 0x30);
	dsi_generic_write_seq(dsi, 0x77, 0x00);
	dsi_generic_write_seq(dsi, 0x78, 0x43);
	dsi_generic_write_seq(dsi, 0x79, 0x00);
	dsi_generic_write_seq(dsi, 0x7a, 0x61);
	dsi_generic_write_seq(dsi, 0x7b, 0x00);
	dsi_generic_write_seq(dsi, 0x7c, 0x7d);
	dsi_generic_write_seq(dsi, 0x7d, 0x00);
	dsi_generic_write_seq(dsi, 0x7e, 0x98);
	dsi_generic_write_seq(dsi, 0x7f, 0x00);
	dsi_generic_write_seq(dsi, 0x80, 0xae);
	dsi_generic_write_seq(dsi, 0x81, 0x00);
	dsi_generic_write_seq(dsi, 0x82, 0xc0);
	dsi_generic_write_seq(dsi, 0x83, 0x00);
	dsi_generic_write_seq(dsi, 0x84, 0xcd);
	dsi_generic_write_seq(dsi, 0x85, 0x00);
	dsi_generic_write_seq(dsi, 0x86, 0xdb);
	dsi_generic_write_seq(dsi, 0x87, 0x01);
	dsi_generic_write_seq(dsi, 0x88, 0x0a);
	dsi_generic_write_seq(dsi, 0x89, 0x01);
	dsi_generic_write_seq(dsi, 0x8a, 0x2a);
	dsi_generic_write_seq(dsi, 0x8b, 0x01);
	dsi_generic_write_seq(dsi, 0x8c, 0x66);
	dsi_generic_write_seq(dsi, 0x8d, 0x01);
	dsi_generic_write_seq(dsi, 0x8e, 0x95);
	dsi_generic_write_seq(dsi, 0x8f, 0x01);
	dsi_generic_write_seq(dsi, 0x90, 0xd8);
	dsi_generic_write_seq(dsi, 0x91, 0x02);
	dsi_generic_write_seq(dsi, 0x92, 0x1a);
	dsi_generic_write_seq(dsi, 0x93, 0x02);
	dsi_generic_write_seq(dsi, 0x94, 0x1c);
	dsi_generic_write_seq(dsi, 0x95, 0x02);
	dsi_generic_write_seq(dsi, 0x96, 0x5e);
	dsi_generic_write_seq(dsi, 0x97, 0x02);
	dsi_generic_write_seq(dsi, 0x98, 0x9a);
	dsi_generic_write_seq(dsi, 0x99, 0x02);
	dsi_generic_write_seq(dsi, 0x9a, 0xbf);
	dsi_generic_write_seq(dsi, 0x9b, 0x02);
	dsi_generic_write_seq(dsi, 0x9c, 0xed);
	dsi_generic_write_seq(dsi, 0x9d, 0x03);
	dsi_generic_write_seq(dsi, 0x9e, 0x0b);
	dsi_generic_write_seq(dsi, 0x9f, 0x03);
	dsi_generic_write_seq(dsi, 0xa0, 0x36);
	dsi_generic_write_seq(dsi, 0xa2, 0x03);
	dsi_generic_write_seq(dsi, 0xa3, 0x3b);
	dsi_generic_write_seq(dsi, 0xa4, 0x03);
	dsi_generic_write_seq(dsi, 0xa5, 0x40);
	dsi_generic_write_seq(dsi, 0xa6, 0x03);
	dsi_generic_write_seq(dsi, 0xa7, 0x45);
	dsi_generic_write_seq(dsi, 0xa9, 0x03);
	dsi_generic_write_seq(dsi, 0xaa, 0x54);
	dsi_generic_write_seq(dsi, 0xab, 0x03);
	dsi_generic_write_seq(dsi, 0xac, 0x70);
	dsi_generic_write_seq(dsi, 0xad, 0x03);
	dsi_generic_write_seq(dsi, 0xae, 0x8e);
	dsi_generic_write_seq(dsi, 0xaf, 0x03);
	dsi_generic_write_seq(dsi, 0xb0, 0xb2);
	dsi_generic_write_seq(dsi, 0xb1, 0x03);
	dsi_generic_write_seq(dsi, 0xb2, 0xc9);
	dsi_generic_write_seq(dsi, 0xb3, 0x00);
	dsi_generic_write_seq(dsi, 0xb4, 0x30);
	dsi_generic_write_seq(dsi, 0xb5, 0x00);
	dsi_generic_write_seq(dsi, 0xb6, 0x43);
	dsi_generic_write_seq(dsi, 0xb7, 0x00);
	dsi_generic_write_seq(dsi, 0xb8, 0x61);
	dsi_generic_write_seq(dsi, 0xb9, 0x00);
	dsi_generic_write_seq(dsi, 0xba, 0x7d);
	dsi_generic_write_seq(dsi, 0xbb, 0x00);
	dsi_generic_write_seq(dsi, 0xbc, 0x98);
	dsi_generic_write_seq(dsi, 0xbd, 0x00);
	dsi_generic_write_seq(dsi, 0xbe, 0xae);
	dsi_generic_write_seq(dsi, 0xbf, 0x00);
	dsi_generic_write_seq(dsi, 0xc0, 0xc0);
	dsi_generic_write_seq(dsi, 0xc1, 0x00);
	dsi_generic_write_seq(dsi, 0xc2, 0xcd);
	dsi_generic_write_seq(dsi, 0xc3, 0x00);
	dsi_generic_write_seq(dsi, 0xc4, 0xdb);
	dsi_generic_write_seq(dsi, 0xc5, 0x01);
	dsi_generic_write_seq(dsi, 0xc6, 0x0a);
	dsi_generic_write_seq(dsi, 0xc7, 0x01);
	dsi_generic_write_seq(dsi, 0xc8, 0x2a);
	dsi_generic_write_seq(dsi, 0xc9, 0x01);
	dsi_generic_write_seq(dsi, 0xca, 0x66);
	dsi_generic_write_seq(dsi, 0xcb, 0x01);
	dsi_generic_write_seq(dsi, 0xcc, 0x95);
	dsi_generic_write_seq(dsi, 0xcd, 0x01);
	dsi_generic_write_seq(dsi, 0xce, 0xd8);
	dsi_generic_write_seq(dsi, 0xcf, 0x02);
	dsi_generic_write_seq(dsi, 0xd0, 0x1a);
	dsi_generic_write_seq(dsi, 0xd1, 0x02);
	dsi_generic_write_seq(dsi, 0xd2, 0x1c);
	dsi_generic_write_seq(dsi, 0xd3, 0x02);
	dsi_generic_write_seq(dsi, 0xd4, 0x5e);
	dsi_generic_write_seq(dsi, 0xd5, 0x02);
	dsi_generic_write_seq(dsi, 0xd6, 0x9a);
	dsi_generic_write_seq(dsi, 0xd7, 0x02);
	dsi_generic_write_seq(dsi, 0xd8, 0xbf);
	dsi_generic_write_seq(dsi, 0xd9, 0x02);
	dsi_generic_write_seq(dsi, 0xda, 0xed);
	dsi_generic_write_seq(dsi, 0xdb, 0x03);
	dsi_generic_write_seq(dsi, 0xdc, 0x0b);
	dsi_generic_write_seq(dsi, 0xdd, 0x03);
	dsi_generic_write_seq(dsi, 0xde, 0x36);
	dsi_generic_write_seq(dsi, 0xdf, 0x03);
	dsi_generic_write_seq(dsi, 0xe0, 0x3b);
	dsi_generic_write_seq(dsi, 0xe1, 0x03);
	dsi_generic_write_seq(dsi, 0xe2, 0x40);
	dsi_generic_write_seq(dsi, 0xe3, 0x03);
	dsi_generic_write_seq(dsi, 0xe4, 0x45);
	dsi_generic_write_seq(dsi, 0xe5, 0x03);
	dsi_generic_write_seq(dsi, 0xe6, 0x54);
	dsi_generic_write_seq(dsi, 0xe7, 0x03);
	dsi_generic_write_seq(dsi, 0xe8, 0x70);
	dsi_generic_write_seq(dsi, 0xe9, 0x03);
	dsi_generic_write_seq(dsi, 0xea, 0x8e);
	dsi_generic_write_seq(dsi, 0xeb, 0x03);
	dsi_generic_write_seq(dsi, 0xec, 0xb2);
	dsi_generic_write_seq(dsi, 0xed, 0x03);
	dsi_generic_write_seq(dsi, 0xee, 0xc9);
	dsi_generic_write_seq(dsi, 0xef, 0x00);
	dsi_generic_write_seq(dsi, 0xf0, 0x30);
	dsi_generic_write_seq(dsi, 0xf1, 0x00);
	dsi_generic_write_seq(dsi, 0xf2, 0x43);
	dsi_generic_write_seq(dsi, 0xf3, 0x00);
	dsi_generic_write_seq(dsi, 0xf4, 0x61);
	dsi_generic_write_seq(dsi, 0xf5, 0x00);
	dsi_generic_write_seq(dsi, 0xf6, 0x7d);
	dsi_generic_write_seq(dsi, 0xf7, 0x00);
	dsi_generic_write_seq(dsi, 0xf8, 0x98);
	dsi_generic_write_seq(dsi, 0xf9, 0x00);
	dsi_generic_write_seq(dsi, 0xfa, 0xae);
	dsi_generic_write_seq(dsi, 0xfb, 0x01);
	dsi_generic_write_seq(dsi, 0xff, 0x02);
	dsi_generic_write_seq(dsi, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0x01, 0xc0);
	dsi_generic_write_seq(dsi, 0x02, 0x00);
	dsi_generic_write_seq(dsi, 0x03, 0xcd);
	dsi_generic_write_seq(dsi, 0x04, 0x00);
	dsi_generic_write_seq(dsi, 0x05, 0xdb);
	dsi_generic_write_seq(dsi, 0x06, 0x01);
	dsi_generic_write_seq(dsi, 0x07, 0x0a);
	dsi_generic_write_seq(dsi, 0x08, 0x01);
	dsi_generic_write_seq(dsi, 0x09, 0x2a);
	dsi_generic_write_seq(dsi, 0x0a, 0x01);
	dsi_generic_write_seq(dsi, 0x0b, 0x69);
	dsi_generic_write_seq(dsi, 0x0c, 0x01);
	dsi_generic_write_seq(dsi, 0x0d, 0x99);
	dsi_generic_write_seq(dsi, 0x0e, 0x01);
	dsi_generic_write_seq(dsi, 0x0f, 0xde);
	dsi_generic_write_seq(dsi, 0x10, 0x02);
	dsi_generic_write_seq(dsi, 0x11, 0x20);
	dsi_generic_write_seq(dsi, 0x12, 0x02);
	dsi_generic_write_seq(dsi, 0x13, 0x22);
	dsi_generic_write_seq(dsi, 0x14, 0x02);
	dsi_generic_write_seq(dsi, 0x15, 0x64);
	dsi_generic_write_seq(dsi, 0x16, 0x02);
	dsi_generic_write_seq(dsi, 0x17, 0xa0);
	dsi_generic_write_seq(dsi, 0x18, 0x02);
	dsi_generic_write_seq(dsi, 0x19, 0xc4);
	dsi_generic_write_seq(dsi, 0x1a, 0x02);
	dsi_generic_write_seq(dsi, 0x1b, 0xf3);
	dsi_generic_write_seq(dsi, 0x1c, 0x03);
	dsi_generic_write_seq(dsi, 0x1d, 0x0f);
	dsi_generic_write_seq(dsi, 0x1e, 0x03);
	dsi_generic_write_seq(dsi, 0x1f, 0x36);
	dsi_generic_write_seq(dsi, 0x20, 0x03);
	dsi_generic_write_seq(dsi, 0x21, 0x3b);
	dsi_generic_write_seq(dsi, 0x22, 0x03);
	dsi_generic_write_seq(dsi, 0x23, 0x40);
	dsi_generic_write_seq(dsi, 0x24, 0x03);
	dsi_generic_write_seq(dsi, 0x25, 0x45);
	dsi_generic_write_seq(dsi, 0x26, 0x03);
	dsi_generic_write_seq(dsi, 0x27, 0x54);
	dsi_generic_write_seq(dsi, 0x28, 0x03);
	dsi_generic_write_seq(dsi, 0x29, 0x70);
	dsi_generic_write_seq(dsi, 0x2a, 0x03);
	dsi_generic_write_seq(dsi, 0x2b, 0x8e);
	dsi_generic_write_seq(dsi, 0x2d, 0x03);
	dsi_generic_write_seq(dsi, 0x2f, 0xb2);
	dsi_generic_write_seq(dsi, 0x30, 0x03);
	dsi_generic_write_seq(dsi, 0x31, 0xc9);
	dsi_generic_write_seq(dsi, 0x32, 0x00);
	dsi_generic_write_seq(dsi, 0x33, 0x30);
	dsi_generic_write_seq(dsi, 0x34, 0x00);
	dsi_generic_write_seq(dsi, 0x35, 0x43);
	dsi_generic_write_seq(dsi, 0x36, 0x00);
	dsi_generic_write_seq(dsi, 0x37, 0x61);
	dsi_generic_write_seq(dsi, 0x38, 0x00);
	dsi_generic_write_seq(dsi, 0x39, 0x7d);
	dsi_generic_write_seq(dsi, 0x3a, 0x00);
	dsi_generic_write_seq(dsi, 0x3b, 0x98);
	dsi_generic_write_seq(dsi, 0x3d, 0x00);
	dsi_generic_write_seq(dsi, 0x3f, 0xae);
	dsi_generic_write_seq(dsi, 0x40, 0x00);
	dsi_generic_write_seq(dsi, 0x41, 0xc0);
	dsi_generic_write_seq(dsi, 0x42, 0x00);
	dsi_generic_write_seq(dsi, 0x43, 0xcd);
	dsi_generic_write_seq(dsi, 0x44, 0x00);
	dsi_generic_write_seq(dsi, 0x45, 0xdb);
	dsi_generic_write_seq(dsi, 0x46, 0x01);
	dsi_generic_write_seq(dsi, 0x47, 0x0a);
	dsi_generic_write_seq(dsi, 0x48, 0x01);
	dsi_generic_write_seq(dsi, 0x49, 0x2a);
	dsi_generic_write_seq(dsi, 0x4a, 0x01);
	dsi_generic_write_seq(dsi, 0x4b, 0x69);
	dsi_generic_write_seq(dsi, 0x4c, 0x01);
	dsi_generic_write_seq(dsi, 0x4d, 0x99);
	dsi_generic_write_seq(dsi, 0x4e, 0x01);
	dsi_generic_write_seq(dsi, 0x4f, 0xde);
	dsi_generic_write_seq(dsi, 0x50, 0x02);
	dsi_generic_write_seq(dsi, 0x51, 0x20);
	dsi_generic_write_seq(dsi, 0x52, 0x02);
	dsi_generic_write_seq(dsi, 0x53, 0x22);
	dsi_generic_write_seq(dsi, 0x54, 0x02);
	dsi_generic_write_seq(dsi, 0x55, 0x64);
	dsi_generic_write_seq(dsi, 0x56, 0x02);
	dsi_generic_write_seq(dsi, 0x58, 0xa0);
	dsi_generic_write_seq(dsi, 0x59, 0x02);
	dsi_generic_write_seq(dsi, 0x5a, 0xc4);
	dsi_generic_write_seq(dsi, 0x5b, 0x02);
	dsi_generic_write_seq(dsi, 0x5c, 0xf3);
	dsi_generic_write_seq(dsi, 0x5d, 0x03);
	dsi_generic_write_seq(dsi, 0x5e, 0x0f);
	dsi_generic_write_seq(dsi, 0x5f, 0x03);
	dsi_generic_write_seq(dsi, 0x60, 0x36);
	dsi_generic_write_seq(dsi, 0x61, 0x03);
	dsi_generic_write_seq(dsi, 0x62, 0x3b);
	dsi_generic_write_seq(dsi, 0x63, 0x03);
	dsi_generic_write_seq(dsi, 0x64, 0x40);
	dsi_generic_write_seq(dsi, 0x65, 0x03);
	dsi_generic_write_seq(dsi, 0x66, 0x45);
	dsi_generic_write_seq(dsi, 0x67, 0x03);
	dsi_generic_write_seq(dsi, 0x68, 0x54);
	dsi_generic_write_seq(dsi, 0x69, 0x03);
	dsi_generic_write_seq(dsi, 0x6a, 0x70);
	dsi_generic_write_seq(dsi, 0x6b, 0x03);
	dsi_generic_write_seq(dsi, 0x6c, 0x8e);
	dsi_generic_write_seq(dsi, 0x6d, 0x03);
	dsi_generic_write_seq(dsi, 0x6e, 0xb2);
	dsi_generic_write_seq(dsi, 0x6f, 0x03);
	dsi_generic_write_seq(dsi, 0x70, 0xc9);
	dsi_generic_write_seq(dsi, 0x71, 0x00);
	dsi_generic_write_seq(dsi, 0x72, 0x30);
	dsi_generic_write_seq(dsi, 0x73, 0x00);
	dsi_generic_write_seq(dsi, 0x74, 0x43);
	dsi_generic_write_seq(dsi, 0x75, 0x00);
	dsi_generic_write_seq(dsi, 0x76, 0x61);
	dsi_generic_write_seq(dsi, 0x77, 0x00);
	dsi_generic_write_seq(dsi, 0x78, 0x7d);
	dsi_generic_write_seq(dsi, 0x79, 0x00);
	dsi_generic_write_seq(dsi, 0x7a, 0x98);
	dsi_generic_write_seq(dsi, 0x7b, 0x00);
	dsi_generic_write_seq(dsi, 0x7c, 0xae);
	dsi_generic_write_seq(dsi, 0x7d, 0x00);
	dsi_generic_write_seq(dsi, 0x7e, 0xc0);
	dsi_generic_write_seq(dsi, 0x7f, 0x00);
	dsi_generic_write_seq(dsi, 0x80, 0xcd);
	dsi_generic_write_seq(dsi, 0x81, 0x00);
	dsi_generic_write_seq(dsi, 0x82, 0xdb);
	dsi_generic_write_seq(dsi, 0x83, 0x01);
	dsi_generic_write_seq(dsi, 0x84, 0x0a);
	dsi_generic_write_seq(dsi, 0x85, 0x01);
	dsi_generic_write_seq(dsi, 0x86, 0x2a);
	dsi_generic_write_seq(dsi, 0x87, 0x01);
	dsi_generic_write_seq(dsi, 0x88, 0x63);
	dsi_generic_write_seq(dsi, 0x89, 0x01);
	dsi_generic_write_seq(dsi, 0x8a, 0x90);
	dsi_generic_write_seq(dsi, 0x8b, 0x01);
	dsi_generic_write_seq(dsi, 0x8c, 0xd2);
	dsi_generic_write_seq(dsi, 0x8d, 0x02);
	dsi_generic_write_seq(dsi, 0x8e, 0x14);
	dsi_generic_write_seq(dsi, 0x8f, 0x02);
	dsi_generic_write_seq(dsi, 0x90, 0x16);
	dsi_generic_write_seq(dsi, 0x91, 0x02);
	dsi_generic_write_seq(dsi, 0x92, 0x58);
	dsi_generic_write_seq(dsi, 0x93, 0x02);
	dsi_generic_write_seq(dsi, 0x94, 0x95);
	dsi_generic_write_seq(dsi, 0x95, 0x02);
	dsi_generic_write_seq(dsi, 0x96, 0xbc);
	dsi_generic_write_seq(dsi, 0x97, 0x02);
	dsi_generic_write_seq(dsi, 0x98, 0xed);
	dsi_generic_write_seq(dsi, 0x99, 0x03);
	dsi_generic_write_seq(dsi, 0x9a, 0x0b);
	dsi_generic_write_seq(dsi, 0x9b, 0x03);
	dsi_generic_write_seq(dsi, 0x9c, 0x36);
	dsi_generic_write_seq(dsi, 0x9d, 0x03);
	dsi_generic_write_seq(dsi, 0x9e, 0x3b);
	dsi_generic_write_seq(dsi, 0x9f, 0x03);
	dsi_generic_write_seq(dsi, 0xa0, 0x40);
	dsi_generic_write_seq(dsi, 0xa2, 0x03);
	dsi_generic_write_seq(dsi, 0xa3, 0x45);
	dsi_generic_write_seq(dsi, 0xa4, 0x03);
	dsi_generic_write_seq(dsi, 0xa5, 0x54);
	dsi_generic_write_seq(dsi, 0xa6, 0x03);
	dsi_generic_write_seq(dsi, 0xa7, 0x70);
	dsi_generic_write_seq(dsi, 0xa9, 0x03);
	dsi_generic_write_seq(dsi, 0xaa, 0x8e);
	dsi_generic_write_seq(dsi, 0xab, 0x03);
	dsi_generic_write_seq(dsi, 0xac, 0xb2);
	dsi_generic_write_seq(dsi, 0xad, 0x03);
	dsi_generic_write_seq(dsi, 0xae, 0xc9);
	dsi_generic_write_seq(dsi, 0xaf, 0x00);
	dsi_generic_write_seq(dsi, 0xb0, 0x30);
	dsi_generic_write_seq(dsi, 0xb1, 0x00);
	dsi_generic_write_seq(dsi, 0xb2, 0x43);
	dsi_generic_write_seq(dsi, 0xb3, 0x00);
	dsi_generic_write_seq(dsi, 0xb4, 0x61);
	dsi_generic_write_seq(dsi, 0xb5, 0x00);
	dsi_generic_write_seq(dsi, 0xb6, 0x7d);
	dsi_generic_write_seq(dsi, 0xb7, 0x00);
	dsi_generic_write_seq(dsi, 0xb8, 0x98);
	dsi_generic_write_seq(dsi, 0xb9, 0x00);
	dsi_generic_write_seq(dsi, 0xba, 0xae);
	dsi_generic_write_seq(dsi, 0xbb, 0x00);
	dsi_generic_write_seq(dsi, 0xbc, 0xc0);
	dsi_generic_write_seq(dsi, 0xbd, 0x00);
	dsi_generic_write_seq(dsi, 0xbe, 0xcd);
	dsi_generic_write_seq(dsi, 0xbf, 0x00);
	dsi_generic_write_seq(dsi, 0xc0, 0xdb);
	dsi_generic_write_seq(dsi, 0xc1, 0x01);
	dsi_generic_write_seq(dsi, 0xc2, 0x0a);
	dsi_generic_write_seq(dsi, 0xc3, 0x01);
	dsi_generic_write_seq(dsi, 0xc4, 0x2a);
	dsi_generic_write_seq(dsi, 0xc5, 0x01);
	dsi_generic_write_seq(dsi, 0xc6, 0x63);
	dsi_generic_write_seq(dsi, 0xc7, 0x01);
	dsi_generic_write_seq(dsi, 0xc8, 0x90);
	dsi_generic_write_seq(dsi, 0xc9, 0x01);
	dsi_generic_write_seq(dsi, 0xca, 0xd2);
	dsi_generic_write_seq(dsi, 0xcb, 0x02);
	dsi_generic_write_seq(dsi, 0xcc, 0x14);
	dsi_generic_write_seq(dsi, 0xcd, 0x02);
	dsi_generic_write_seq(dsi, 0xce, 0x16);
	dsi_generic_write_seq(dsi, 0xcf, 0x02);
	dsi_generic_write_seq(dsi, 0xd0, 0x58);
	dsi_generic_write_seq(dsi, 0xd1, 0x02);
	dsi_generic_write_seq(dsi, 0xd2, 0x95);
	dsi_generic_write_seq(dsi, 0xd3, 0x02);
	dsi_generic_write_seq(dsi, 0xd4, 0xbc);
	dsi_generic_write_seq(dsi, 0xd5, 0x02);
	dsi_generic_write_seq(dsi, 0xd6, 0xed);
	dsi_generic_write_seq(dsi, 0xd7, 0x03);
	dsi_generic_write_seq(dsi, 0xd8, 0x0b);
	dsi_generic_write_seq(dsi, 0xd9, 0x03);
	dsi_generic_write_seq(dsi, 0xda, 0x36);
	dsi_generic_write_seq(dsi, 0xdb, 0x03);
	dsi_generic_write_seq(dsi, 0xdc, 0x3b);
	dsi_generic_write_seq(dsi, 0xdd, 0x03);
	dsi_generic_write_seq(dsi, 0xde, 0x40);
	dsi_generic_write_seq(dsi, 0xdf, 0x03);
	dsi_generic_write_seq(dsi, 0xe0, 0x45);
	dsi_generic_write_seq(dsi, 0xe1, 0x03);
	dsi_generic_write_seq(dsi, 0xe2, 0x54);
	dsi_generic_write_seq(dsi, 0xe3, 0x03);
	dsi_generic_write_seq(dsi, 0xe4, 0x70);
	dsi_generic_write_seq(dsi, 0xe5, 0x03);
	dsi_generic_write_seq(dsi, 0xe6, 0x8e);
	dsi_generic_write_seq(dsi, 0xe7, 0x03);
	dsi_generic_write_seq(dsi, 0xe8, 0xb2);
	dsi_generic_write_seq(dsi, 0xe9, 0x03);
	dsi_generic_write_seq(dsi, 0xea, 0xc9);
	dsi_generic_write_seq(dsi, 0xfb, 0x01);
	dsi_generic_write_seq(dsi, 0xff, 0x01);
	dsi_generic_write_seq(dsi, 0x0b, 0x4b);
	dsi_generic_write_seq(dsi, 0x0c, 0x4b);
	dsi_generic_write_seq(dsi, 0x0e, 0xa1);
	dsi_generic_write_seq(dsi, 0x15, 0x0b);
	dsi_generic_write_seq(dsi, 0x16, 0x0b);
	dsi_generic_write_seq(dsi, 0x1b, 0x1b);
	dsi_generic_write_seq(dsi, 0x1c, 0xf5);
	dsi_generic_write_seq(dsi, 0x01, 0x44);
	dsi_generic_write_seq(dsi, 0x5c, 0x82);
	dsi_generic_write_seq(dsi, 0x5e, 0x02);
	dsi_generic_write_seq(dsi, 0x60, 0x0f);
	dsi_generic_write_seq(dsi, 0x66, 0x01);
	dsi_generic_write_seq(dsi, 0x69, 0x99);
	dsi_generic_write_seq(dsi, 0x6d, 0x33);
	dsi_generic_write_seq(dsi, 0xfb, 0x01);
	dsi_generic_write_seq(dsi, 0xff, 0x05);
	dsi_generic_write_seq(dsi, 0x35, 0x6b);
	dsi_generic_write_seq(dsi, 0x7e, 0x02);
	dsi_generic_write_seq(dsi, 0x7f, 0x18);
	dsi_generic_write_seq(dsi, 0x81, 0x05);
	dsi_generic_write_seq(dsi, 0x82, 0x05);
	dsi_generic_write_seq(dsi, 0xa6, 0x04);
	dsi_generic_write_seq(dsi, 0x84, 0x03);
	dsi_generic_write_seq(dsi, 0x85, 0x04);
	dsi_dcs_write_seq(dsi, 0xc6, 0x00);
	dsi_generic_write_seq(dsi, 0xfb, 0x01);
	dsi_generic_write_seq(dsi, 0xff, 0xff);
	dsi_generic_write_seq(dsi, 0x4f, 0x03);
	dsi_generic_write_seq(dsi, 0xfb, 0x01);
	dsi_generic_write_seq(dsi, 0xff, 0x00);
	dsi_dcs_write_seq(dsi, 0xd3, 0x08);
	dsi_dcs_write_seq(dsi, 0xd4, 0x1b);
	dsi_dcs_write_seq(dsi, 0xd5, 0x50);
	dsi_dcs_write_seq(dsi, 0xd6, 0x70);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}

	return 0;
}

static int novatek_jdi_off(struct novatek_jdi *ctx)
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
	msleep(80);

	return 0;
}

static int novatek_jdi_prepare(struct drm_panel *panel)
{
	struct novatek_jdi *ctx = to_novatek_jdi(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->supply);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulator: %d\n", ret);
		return ret;
	}

	ret = novatek_jdi_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		regulator_disable(ctx->supply);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int novatek_jdi_unprepare(struct drm_panel *panel)
{
	struct novatek_jdi *ctx = to_novatek_jdi(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = novatek_jdi_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	regulator_disable(ctx->supply);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode novatek_jdi_mode = {
	.clock = (1080 + 112 + 4 + 76) * (1920 + 27 + 4 + 4) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 112,
	.hsync_end = 1080 + 112 + 4,
	.htotal = 1080 + 112 + 4 + 76,
	.vdisplay = 1920,
	.vsync_start = 1920 + 27,
	.vsync_end = 1920 + 27 + 4,
	.vtotal = 1920 + 27 + 4 + 4,
	.width_mm = 64,
	.height_mm = 114,
};

static int novatek_jdi_get_modes(struct drm_panel *panel,
				 struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &novatek_jdi_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs novatek_jdi_panel_funcs = {
	.prepare = novatek_jdi_prepare,
	.unprepare = novatek_jdi_unprepare,
	.get_modes = novatek_jdi_get_modes,
};

static int novatek_jdi_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct novatek_jdi *ctx;
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

	drm_panel_init(&ctx->panel, dev, &novatek_jdi_panel_funcs,
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

static int novatek_jdi_remove(struct mipi_dsi_device *dsi)
{
	struct novatek_jdi *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id novatek_jdi_of_match[] = {
	{ .compatible = "novatek,jdi" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, novatek_jdi_of_match);

static struct mipi_dsi_driver novatek_jdi_driver = {
	.probe = novatek_jdi_probe,
	.remove = novatek_jdi_remove,
	.driver = {
		.name = "panel-novatek-jdi",
		.of_match_table = novatek_jdi_of_match,
	},
};
module_mipi_dsi_driver(novatek_jdi_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for jdi novatek 1080p video");
MODULE_LICENSE("GPL v2");
