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

struct novatek_auo {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *supply;
	bool prepared;
};

static inline struct novatek_auo *to_novatek_auo(struct drm_panel *panel)
{
	return container_of(panel, struct novatek_auo, panel);
}

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static int novatek_auo_on(struct novatek_auo *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	dsi_dcs_write_seq(dsi, 0xff, 0x05);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0x93, 0x06);
	dsi_dcs_write_seq(dsi, 0x94, 0x30);
	dsi_dcs_write_seq(dsi, 0xff, 0x00);
	dsi_dcs_write_seq(dsi, 0xff, 0x01);
	dsi_dcs_write_seq(dsi, 0x01, 0x55);
	dsi_dcs_write_seq(dsi, 0x15, 0x0f);
	dsi_dcs_write_seq(dsi, 0x16, 0x0f);
	dsi_dcs_write_seq(dsi, 0x1b, 0x1b);
	dsi_dcs_write_seq(dsi, 0x1c, 0xf7);
	dsi_dcs_write_seq(dsi, 0x60, 0x0f);
	dsi_dcs_write_seq(dsi, 0x58, 0x82);
	dsi_dcs_write_seq(dsi, 0x59, 0x00);
	dsi_dcs_write_seq(dsi, 0x5a, 0x02);
	dsi_dcs_write_seq(dsi, 0x5b, 0x00);
	dsi_dcs_write_seq(dsi, 0x5c, 0x82);
	dsi_dcs_write_seq(dsi, 0x5d, 0x80);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_CABC_MIN_BRIGHTNESS, 0x02);
	dsi_dcs_write_seq(dsi, 0x5f, 0x00);
	dsi_dcs_write_seq(dsi, 0x66, 0x01);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0xff, 0x05);
	dsi_dcs_write_seq(dsi, 0x54, 0x75);
	dsi_dcs_write_seq(dsi, 0x85, 0x05);
	dsi_dcs_write_seq(dsi, 0xa6, 0x04);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0xff, 0xff);
	dsi_dcs_write_seq(dsi, 0x4f, 0x03);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0xff, 0x00);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VHBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, 0xd3, 0x30);
	dsi_dcs_write_seq(dsi, 0xd4, 0x06);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(100);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	msleep(100);

	return 0;
}

static int novatek_auo_off(struct novatek_auo *ctx)
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

static int novatek_auo_prepare(struct drm_panel *panel)
{
	struct novatek_auo *ctx = to_novatek_auo(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->supply);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulator: %d\n", ret);
		return ret;
	}

	ret = novatek_auo_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		regulator_disable(ctx->supply);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int novatek_auo_unprepare(struct drm_panel *panel)
{
	struct novatek_auo *ctx = to_novatek_auo(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = novatek_auo_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	regulator_disable(ctx->supply);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode novatek_auo_mode = {
	.clock = (1080 + 104 + 20 + 56) * (1920 + 6 + 2 + 46) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 104,
	.hsync_end = 1080 + 104 + 20,
	.htotal = 1080 + 104 + 20 + 56,
	.vdisplay = 1920,
	.vsync_start = 1920 + 6,
	.vsync_end = 1920 + 6 + 2,
	.vtotal = 1920 + 6 + 2 + 46,
	.width_mm = 64,
	.height_mm = 114,
};

static int novatek_auo_get_modes(struct drm_panel *panel,
				 struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &novatek_auo_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs novatek_auo_panel_funcs = {
	.prepare = novatek_auo_prepare,
	.unprepare = novatek_auo_unprepare,
	.get_modes = novatek_auo_get_modes,
};

static int novatek_auo_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct novatek_auo *ctx;
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

	drm_panel_init(&ctx->panel, dev, &novatek_auo_panel_funcs,
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

static int novatek_auo_remove(struct mipi_dsi_device *dsi)
{
	struct novatek_auo *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id novatek_auo_of_match[] = {
	{ .compatible = "novatek,auo" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, novatek_auo_of_match);

static struct mipi_dsi_driver novatek_auo_driver = {
	.probe = novatek_auo_probe,
	.remove = novatek_auo_remove,
	.driver = {
		.name = "panel-novatek-auo",
		.of_match_table = novatek_auo_of_match,
	},
};
module_mipi_dsi_driver(novatek_auo_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for auo novatek 1080p video");
MODULE_LICENSE("GPL v2");
