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

struct renesas_sharp {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *supply;
	bool prepared;
};

static inline struct renesas_sharp *to_renesas_sharp(struct drm_panel *panel)
{
	return container_of(panel, struct renesas_sharp, panel);
}

#define dsi_generic_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_generic_write(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static int renesas_sharp_on(struct renesas_sharp *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	dsi_generic_write_seq(dsi, 0xb0, 0x04);
	dsi_generic_write_seq(dsi, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xc2,
			      0x32, 0xf7, 0x80, 0x07, 0x08, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xd6, 0x01);
	dsi_generic_write_seq(dsi, 0xc0, 0x0f, 0x0f);
	dsi_generic_write_seq(dsi, 0xec, 0x00, 0x10);
	dsi_generic_write_seq(dsi, 0xc7,
			      0x05, 0x19, 0x22, 0x2b, 0x38, 0x51, 0x41, 0x50,
			      0x5c, 0x64, 0x6b, 0x74, 0x0f, 0x23, 0x2b, 0x32,
			      0x3f, 0x52, 0x44, 0x55, 0x61, 0x69, 0x70, 0x77);
	dsi_generic_write_seq(dsi, 0xc8,
			      0x03, 0x18, 0x21, 0x2b, 0x38, 0x51, 0x42, 0x4f,
			      0x5d, 0x65, 0x6c, 0x74, 0x0d, 0x22, 0x2a, 0x32,
			      0x3e, 0x52, 0x41, 0x54, 0x5d, 0x66, 0x6d, 0x77);
	dsi_generic_write_seq(dsi, 0xc9,
			      0x00, 0x15, 0x1e, 0x28, 0x36, 0x50, 0x42, 0x50,
			      0x5e, 0x66, 0x6d, 0x74, 0x0a, 0x1f, 0x27, 0x2f,
			      0x3d, 0x51, 0x41, 0x55, 0x5e, 0x67, 0x6e, 0x77);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}

	return 0;
}

static int renesas_sharp_off(struct renesas_sharp *ctx)
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
	msleep(110);

	return 0;
}

static int renesas_sharp_prepare(struct drm_panel *panel)
{
	struct renesas_sharp *ctx = to_renesas_sharp(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->supply);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulator: %d\n", ret);
		return ret;
	}

	ret = renesas_sharp_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		regulator_disable(ctx->supply);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int renesas_sharp_unprepare(struct drm_panel *panel)
{
	struct renesas_sharp *ctx = to_renesas_sharp(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = renesas_sharp_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	regulator_disable(ctx->supply);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode renesas_sharp_mode = {
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

static int renesas_sharp_get_modes(struct drm_panel *panel,
				   struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &renesas_sharp_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs renesas_sharp_panel_funcs = {
	.prepare = renesas_sharp_prepare,
	.unprepare = renesas_sharp_unprepare,
	.get_modes = renesas_sharp_get_modes,
};

static int renesas_sharp_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct renesas_sharp *ctx;
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

	drm_panel_init(&ctx->panel, dev, &renesas_sharp_panel_funcs,
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

static int renesas_sharp_remove(struct mipi_dsi_device *dsi)
{
	struct renesas_sharp *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id renesas_sharp_of_match[] = {
	{ .compatible = "renesas,sharp" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, renesas_sharp_of_match);

static struct mipi_dsi_driver renesas_sharp_driver = {
	.probe = renesas_sharp_probe,
	.remove = renesas_sharp_remove,
	.driver = {
		.name = "panel-renesas-sharp",
		.of_match_table = renesas_sharp_of_match,
	},
};
module_mipi_dsi_driver(renesas_sharp_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for sharp renesas 1080p video");
MODULE_LICENSE("GPL v2");
