// SPDX-License-Identifier: GPL-2.0
/*
 * Sony IMX132 CMOS image sensor driver
 *
 * Copyright (C) 2026 Rob Lymm
 *
 * A 1/4.4" 2.4 MP sensor with a two-lane MIPI CSI-2 output, fitted as the
 * front camera of the Sony Xperia Z2 (module SEM02BN1 or LGI02BN1).
 *
 * The register layout is Sony's usual one, close enough to SMIA that the
 * names below are the SMIA ones. Every value in imx132_mode_1976x1144 was
 * read back from the sensor's own power-on defaults rather than taken from a
 * vendor table: no register table for this part exists in any kernel, and the
 * stock Android camera stack computes its writes at run time. The geometry
 * those defaults describe matches Sony's own device tree for the phone
 * exactly, which is the reason to trust them.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#include <media/v4l2-cci.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>

#define IMX132_REG_CHIP_ID		CCI_REG16(0x0000)
#define IMX132_CHIP_ID			0x0132

#define IMX132_REG_MODE_SELECT		CCI_REG8(0x0100)
#define IMX132_MODE_STANDBY		0x00
#define IMX132_MODE_STREAMING		0x01

#define IMX132_REG_IMAGE_ORIENTATION	CCI_REG8(0x0101)
#define IMX132_ORIENTATION_HFLIP	BIT(0)
#define IMX132_ORIENTATION_VFLIP	BIT(1)

#define IMX132_REG_GROUP_HOLD		CCI_REG8(0x0104)

#define IMX132_REG_CSI_DATA_FORMAT	CCI_REG16(0x0112)
#define IMX132_CSI_DATA_FORMAT_RAW10	0x0a0a

#define IMX132_REG_EXPOSURE		CCI_REG16(0x0202)
#define IMX132_EXPOSURE_MIN		1
#define IMX132_EXPOSURE_STEP		1
#define IMX132_EXPOSURE_MARGIN		4	/* below frame length */
#define IMX132_EXPOSURE_DEFAULT		800

/*
 * Analogue gain is Sony's usual reciprocal law:
 *	gain = 256 / (256 - value)
 * so 0 is unity and the register saturates well before 256.
 */
#define IMX132_REG_ANALOGUE_GAIN	CCI_REG16(0x0204)
#define IMX132_ANA_GAIN_MIN		0
#define IMX132_ANA_GAIN_MAX		224
#define IMX132_ANA_GAIN_STEP		1
#define IMX132_ANA_GAIN_DEFAULT		0

#define IMX132_REG_VT_PIX_CLK_DIV	CCI_REG16(0x0300)
#define IMX132_REG_VT_SYS_CLK_DIV	CCI_REG16(0x0302)
#define IMX132_REG_PRE_PLL_CLK_DIV	CCI_REG16(0x0304)
#define IMX132_REG_PLL_MULTIPLIER	CCI_REG16(0x0306)

#define IMX132_REG_FRAME_LENGTH		CCI_REG16(0x0340)
#define IMX132_REG_LINE_LENGTH		CCI_REG16(0x0342)
#define IMX132_REG_X_ADDR_START		CCI_REG16(0x0344)
#define IMX132_REG_Y_ADDR_START		CCI_REG16(0x0346)
#define IMX132_REG_X_ADDR_END		CCI_REG16(0x0348)
#define IMX132_REG_Y_ADDR_END		CCI_REG16(0x034a)
#define IMX132_REG_X_OUTPUT_SIZE	CCI_REG16(0x034c)
#define IMX132_REG_Y_OUTPUT_SIZE	CCI_REG16(0x034e)
#define IMX132_REG_X_EVEN_INC		CCI_REG16(0x0380)

#define IMX132_VBLANK_MIN		16
#define IMX132_FRAME_LENGTH_MAX		0xffff

/* The rate the Xperia Z2 feeds it. */
#define IMX132_XCLK_FREQ		19200000

/*
 * vt_pix_clk = xclk / pre_pll_clk_div * pll_multiplier
 *		      / (vt_pix_clk_div * vt_sys_clk_div)
 *	      = 19.2 MHz / 1 * 45 / (10 * 1)
 */
#define IMX132_PIXEL_RATE		86400000

/* pixel_rate * bits_per_sample / lanes / 2, for DDR */
#define IMX132_LINK_FREQ		216000000

#define IMX132_NATIVE_WIDTH		1976
#define IMX132_NATIVE_HEIGHT		1200

static const char * const imx132_supply_name[] = {
	"vana",		/* analogue, 2.7 V  (Xperia Z2: pm8941 l17) */
	"vdig",		/* digital core, 1.2 V (pm8941 l3) */
	"vif",		/* interface and I/O (pm8941 lvs2) */
};

#define IMX132_NUM_SUPPLIES ARRAY_SIZE(imx132_supply_name)

/*
 * Four entries, one per flip combination, in the order
 * none, hflip, vflip, both. The sensor's native order is BGGR: Sony's own
 * device tree gives it subdev_code 0x3007, MEDIA_BUS_FMT_SBGGR10_1X10.
 */
static const u32 imx132_mbus_formats[] = {
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
};

struct imx132_mode {
	unsigned int width;
	unsigned int height;
	unsigned int line_length;
	unsigned int frame_length;
	struct v4l2_rect crop;
};

/*
 * The sensor's power-on default, read back over CCI. The crop is
 * self-consistent: 1975 - 0 + 1 = 1976 and 1171 - 28 + 1 = 1144.
 */
static const struct imx132_mode imx132_mode_1976x1144 = {
	.width		= 1976,
	.height		= 1144,
	.line_length	= 2250,
	.frame_length	= 1200,
	.crop = {
		.left	= 0,
		.top	= 28,
		.width	= 1976,
		.height	= 1144,
	},
};

struct imx132 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct regmap *regmap;

	struct clk *xclk;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[IMX132_NUM_SUPPLIES];

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vflip;
};

static inline struct imx132 *to_imx132(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx132, sd);
}

/* Pick the mbus code matching the current flip settings. */
static u32 imx132_get_format_code(struct imx132 *imx132)
{
	unsigned int i = (imx132->vflip->val ? 2 : 0) |
			 (imx132->hflip->val ? 1 : 0);

	return imx132_mbus_formats[i];
}

/* -------------------------------------------------------------------------
 * Controls
 */

static int imx132_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx132 *imx132 =
		container_of(ctrl->handler, struct imx132, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&imx132->sd);
	const struct imx132_mode *mode = &imx132_mode_1976x1144;
	int ret = 0;
	u8 orient;

	if (ctrl->id == V4L2_CID_VBLANK) {
		int exposure_max = mode->height + ctrl->val -
				   IMX132_EXPOSURE_MARGIN;
		int exposure_def = min(exposure_max,
				       IMX132_EXPOSURE_DEFAULT);

		__v4l2_ctrl_modify_range(imx132->exposure,
					 imx132->exposure->minimum,
					 exposure_max, imx132->exposure->step,
					 exposure_def);
	}

	/*
	 * Applying controls to a powered-down sensor is pointless, and the
	 * range update above still has to happen, so bail out after it.
	 */
	if (pm_runtime_get_if_in_use(&client->dev) == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		cci_write(imx132->regmap, IMX132_REG_EXPOSURE, ctrl->val, &ret);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		cci_write(imx132->regmap, IMX132_REG_ANALOGUE_GAIN, ctrl->val,
			  &ret);
		break;
	case V4L2_CID_VBLANK:
		cci_write(imx132->regmap, IMX132_REG_FRAME_LENGTH,
			  mode->height + ctrl->val, &ret);
		break;
	case V4L2_CID_HBLANK:
		cci_write(imx132->regmap, IMX132_REG_LINE_LENGTH,
			  mode->width + ctrl->val, &ret);
		break;
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		orient = (imx132->hflip->val ? IMX132_ORIENTATION_HFLIP : 0) |
			 (imx132->vflip->val ? IMX132_ORIENTATION_VFLIP : 0);
		cci_write(imx132->regmap, IMX132_REG_IMAGE_ORIENTATION, orient,
			  &ret);
		break;
	default:
		dev_info(&client->dev, "unhandled control 0x%x\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx132_ctrl_ops = {
	.s_ctrl = imx132_set_ctrl,
};

static const s64 imx132_link_freq_menu[] = {
	IMX132_LINK_FREQ,
};

static int imx132_init_controls(struct imx132 *imx132)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx132->sd);
	const struct imx132_mode *mode = &imx132_mode_1976x1144;
	struct v4l2_ctrl_handler *hdl = &imx132->ctrl_handler;
	struct v4l2_fwnode_device_properties props;
	int exposure_max, vblank_def, vblank_max;
	struct v4l2_ctrl *ctrl;
	int ret;

	ret = v4l2_ctrl_handler_init(hdl, 10);
	if (ret)
		return ret;

	ctrl = v4l2_ctrl_new_int_menu(hdl, &imx132_ctrl_ops,
				      V4L2_CID_LINK_FREQ,
				      ARRAY_SIZE(imx132_link_freq_menu) - 1, 0,
				      imx132_link_freq_menu);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ctrl = v4l2_ctrl_new_std(hdl, &imx132_ctrl_ops, V4L2_CID_PIXEL_RATE,
				 IMX132_PIXEL_RATE, IMX132_PIXEL_RATE, 1,
				 IMX132_PIXEL_RATE);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->frame_length - mode->height;
	vblank_max = IMX132_FRAME_LENGTH_MAX - mode->height;
	imx132->vblank = v4l2_ctrl_new_std(hdl, &imx132_ctrl_ops,
					   V4L2_CID_VBLANK, IMX132_VBLANK_MIN,
					   vblank_max, 1, vblank_def);

	/* Horizontal blanking is fixed: the line length is not programmed. */
	imx132->hblank = v4l2_ctrl_new_std(hdl, &imx132_ctrl_ops,
					   V4L2_CID_HBLANK,
					   mode->line_length - mode->width,
					   mode->line_length - mode->width, 1,
					   mode->line_length - mode->width);
	if (imx132->hblank)
		imx132->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	exposure_max = mode->frame_length - IMX132_EXPOSURE_MARGIN;
	imx132->exposure = v4l2_ctrl_new_std(hdl, &imx132_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX132_EXPOSURE_MIN, exposure_max,
					     IMX132_EXPOSURE_STEP,
					     min(exposure_max,
						 IMX132_EXPOSURE_DEFAULT));

	v4l2_ctrl_new_std(hdl, &imx132_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  IMX132_ANA_GAIN_MIN, IMX132_ANA_GAIN_MAX,
			  IMX132_ANA_GAIN_STEP, IMX132_ANA_GAIN_DEFAULT);

	imx132->hflip = v4l2_ctrl_new_std(hdl, &imx132_ctrl_ops,
					  V4L2_CID_HFLIP, 0, 1, 1, 0);
	imx132->vflip = v4l2_ctrl_new_std(hdl, &imx132_ctrl_ops,
					  V4L2_CID_VFLIP, 0, 1, 1, 0);
	/* A flip changes the Bayer order, so the format must be re-read. */
	if (imx132->hflip)
		imx132->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;
	if (imx132->vflip)
		imx132->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	if (hdl->error) {
		ret = hdl->error;
		dev_err(&client->dev, "control init failed: %d\n", ret);
		goto error;
	}

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		goto error;

	ret = v4l2_ctrl_new_fwnode_properties(hdl, &imx132_ctrl_ops, &props);
	if (ret)
		goto error;

	imx132->sd.ctrl_handler = hdl;

	return 0;

error:
	v4l2_ctrl_handler_free(hdl);

	return ret;
}

/* -------------------------------------------------------------------------
 * Subdev operations
 */

static int imx132_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx132 *imx132 = to_imx132(sd);

	if (code->index)
		return -EINVAL;

	code->code = imx132_get_format_code(imx132);

	return 0;
}

static int imx132_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx132 *imx132 = to_imx132(sd);

	if (fse->index)
		return -EINVAL;

	if (fse->code != imx132_get_format_code(imx132))
		return -EINVAL;

	fse->min_width = imx132_mode_1976x1144.width;
	fse->max_width = fse->min_width;
	fse->min_height = imx132_mode_1976x1144.height;
	fse->max_height = fse->min_height;

	return 0;
}

static int imx132_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx132 *imx132 = to_imx132(sd);
	const struct imx132_mode *mode = &imx132_mode_1976x1144;
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;

	/* Only the one mode, so the request is simply normalised to it. */
	format = v4l2_subdev_state_get_format(state, 0);
	format->code = imx132_get_format_code(imx132);
	format->width = mode->width;
	format->height = mode->height;
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_RAW;
	format->ycbcr_enc = V4L2_YCBCR_ENC_601;
	format->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	format->xfer_func = V4L2_XFER_FUNC_NONE;

	crop = v4l2_subdev_state_get_crop(state, 0);
	*crop = mode->crop;

	fmt->format = *format;

	return 0;
}

static int imx132_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = *v4l2_subdev_state_get_crop(state, 0);
		return 0;

	case V4L2_SEL_TGT_NATIVE_SIZE:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = IMX132_NATIVE_WIDTH;
		sel->r.height = IMX132_NATIVE_HEIGHT;
		return 0;
	}

	return -EINVAL;
}

static int imx132_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = 0,
	};

	return imx132_set_pad_format(sd, state, &fmt);
}

/*
 * Program the mode. Every value here is the sensor's own reset default, so
 * this is closer to an assertion than a configuration; it is written out in
 * full so that a future second mode has something to differ from.
 */
static int imx132_configure(struct imx132 *imx132)
{
	const struct imx132_mode *mode = &imx132_mode_1976x1144;
	struct regmap *map = imx132->regmap;
	int ret = 0;

	cci_write(map, IMX132_REG_CSI_DATA_FORMAT,
		  IMX132_CSI_DATA_FORMAT_RAW10, &ret);

	cci_write(map, IMX132_REG_PRE_PLL_CLK_DIV, 1, &ret);
	cci_write(map, IMX132_REG_PLL_MULTIPLIER, 45, &ret);
	cci_write(map, IMX132_REG_VT_PIX_CLK_DIV, 10, &ret);
	cci_write(map, IMX132_REG_VT_SYS_CLK_DIV, 1, &ret);

	cci_write(map, IMX132_REG_X_ADDR_START, mode->crop.left, &ret);
	cci_write(map, IMX132_REG_Y_ADDR_START, mode->crop.top, &ret);
	cci_write(map, IMX132_REG_X_ADDR_END,
		  mode->crop.left + mode->crop.width - 1, &ret);
	cci_write(map, IMX132_REG_Y_ADDR_END,
		  mode->crop.top + mode->crop.height - 1, &ret);
	cci_write(map, IMX132_REG_X_OUTPUT_SIZE, mode->width, &ret);
	cci_write(map, IMX132_REG_Y_OUTPUT_SIZE, mode->height, &ret);
	cci_write(map, IMX132_REG_X_EVEN_INC, 1, &ret);

	cci_write(map, IMX132_REG_LINE_LENGTH, mode->line_length, &ret);

	return ret;
}

static int imx132_enable_streams(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state, u32 pad,
				 u64 streams_mask)
{
	struct imx132 *imx132 = to_imx132(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = pm_runtime_resume_and_get(&client->dev);
	if (ret < 0)
		return ret;

	ret = imx132_configure(imx132);
	if (ret)
		goto err_rpm_put;

	/* Frame length, exposure, gain and orientation come from here. */
	ret = __v4l2_ctrl_handler_setup(imx132->sd.ctrl_handler);
	if (ret)
		goto err_rpm_put;

	ret = cci_write(imx132->regmap, IMX132_REG_MODE_SELECT,
			IMX132_MODE_STREAMING, NULL);
	if (ret)
		goto err_rpm_put;

	return 0;

err_rpm_put:
	pm_runtime_mark_last_busy(&client->dev);
	pm_runtime_put_autosuspend(&client->dev);

	return ret;
}

static int imx132_disable_streams(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state, u32 pad,
				  u64 streams_mask)
{
	struct imx132 *imx132 = to_imx132(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = cci_write(imx132->regmap, IMX132_REG_MODE_SELECT,
			IMX132_MODE_STANDBY, NULL);
	if (ret)
		dev_err(&client->dev, "failed to stop streaming\n");

	pm_runtime_mark_last_busy(&client->dev);
	pm_runtime_put_autosuspend(&client->dev);

	return ret;
}

static const struct v4l2_subdev_core_ops imx132_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops imx132_video_ops = {
	.s_stream = v4l2_subdev_s_stream_helper,
};

static const struct v4l2_subdev_pad_ops imx132_pad_ops = {
	.enum_mbus_code = imx132_enum_mbus_code,
	.get_fmt = v4l2_subdev_get_fmt,
	.set_fmt = imx132_set_pad_format,
	.get_selection = imx132_get_selection,
	.enum_frame_size = imx132_enum_frame_size,
	.enable_streams = imx132_enable_streams,
	.disable_streams = imx132_disable_streams,
};

static const struct v4l2_subdev_ops imx132_subdev_ops = {
	.core = &imx132_core_ops,
	.video = &imx132_video_ops,
	.pad = &imx132_pad_ops,
};

static const struct v4l2_subdev_internal_ops imx132_internal_ops = {
	.init_state = imx132_init_state,
};

/* -------------------------------------------------------------------------
 * Power management
 *
 * The order and the delays are Sony's, from the sirius device tree in their
 * published kernel: vdig, vio, vana, reset released, then the clock.
 */

static int imx132_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx132 *imx132 = to_imx132(sd);
	int ret;

	ret = regulator_bulk_enable(IMX132_NUM_SUPPLIES, imx132->supplies);
	if (ret) {
		dev_err(dev, "failed to enable regulators\n");
		return ret;
	}
	usleep_range(1000, 2000);

	gpiod_set_value_cansleep(imx132->reset_gpio, 1);
	usleep_range(1000, 2000);

	ret = clk_prepare_enable(imx132->xclk);
	if (ret) {
		dev_err(dev, "failed to enable clock\n");
		goto err_reset;
	}

	/* Sony waits 1 ms after the clock starts before touching the bus. */
	usleep_range(1000, 2000);

	return 0;

err_reset:
	gpiod_set_value_cansleep(imx132->reset_gpio, 0);
	regulator_bulk_disable(IMX132_NUM_SUPPLIES, imx132->supplies);

	return ret;
}

static int imx132_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx132 *imx132 = to_imx132(sd);

	clk_disable_unprepare(imx132->xclk);
	gpiod_set_value_cansleep(imx132->reset_gpio, 0);
	regulator_bulk_disable(IMX132_NUM_SUPPLIES, imx132->supplies);

	/*
	 * Sony's power-off sequence holds vdig down for 98 ms before it may
	 * be raised again.
	 */
	msleep(98);

	return 0;
}

/* -------------------------------------------------------------------------
 * Probe
 */

static int imx132_identify_module(struct imx132 *imx132)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx132->sd);
	u64 val;
	int ret;

	ret = cci_read(imx132->regmap, IMX132_REG_CHIP_ID, &val, NULL);
	if (ret)
		return dev_err_probe(&client->dev, ret,
				     "failed to read chip id\n");

	if (val != IMX132_CHIP_ID)
		return dev_err_probe(&client->dev, -ENODEV,
				     "chip id mismatch: expected %x, got %llx\n",
				     IMX132_CHIP_ID, val);

	return 0;
}

static int imx132_check_hwcfg(struct device *dev)
{
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY,
	};
	struct fwnode_handle *endpoint;
	int ret;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint)
		return dev_err_probe(dev, -EINVAL, "no endpoint node\n");

	ret = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep_cfg);
	fwnode_handle_put(endpoint);
	if (ret)
		return dev_err_probe(dev, ret, "failed to parse endpoint\n");

	if (ep_cfg.bus.mipi_csi2.num_data_lanes != 2) {
		ret = dev_err_probe(dev, -EINVAL,
				    "only 2 data lanes are supported\n");
		goto done;
	}

	if (ep_cfg.nr_of_link_frequencies != 1 ||
	    ep_cfg.link_frequencies[0] != IMX132_LINK_FREQ) {
		ret = dev_err_probe(dev, -EINVAL,
				    "link frequency must be %d\n",
				    IMX132_LINK_FREQ);
		goto done;
	}

	ret = 0;

done:
	v4l2_fwnode_endpoint_free(&ep_cfg);

	return ret;
}

static int imx132_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct imx132 *imx132;
	unsigned int i;
	u32 xclk_freq;
	int ret;

	imx132 = devm_kzalloc(dev, sizeof(*imx132), GFP_KERNEL);
	if (!imx132)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&imx132->sd, client, &imx132_subdev_ops);
	imx132->sd.internal_ops = &imx132_internal_ops;

	ret = imx132_check_hwcfg(dev);
	if (ret)
		return ret;

	imx132->regmap = devm_cci_regmap_init_i2c(client, 16);
	if (IS_ERR(imx132->regmap))
		return dev_err_probe(dev, PTR_ERR(imx132->regmap),
				     "failed to init regmap\n");

	imx132->xclk = devm_clk_get(dev, NULL);
	if (IS_ERR(imx132->xclk))
		return dev_err_probe(dev, PTR_ERR(imx132->xclk),
				     "failed to get xclk\n");

	xclk_freq = clk_get_rate(imx132->xclk);
	if (xclk_freq != IMX132_XCLK_FREQ)
		return dev_err_probe(dev, -EINVAL,
				     "xclk frequency %u not supported\n",
				     xclk_freq);

	for (i = 0; i < IMX132_NUM_SUPPLIES; i++)
		imx132->supplies[i].supply = imx132_supply_name[i];

	ret = devm_regulator_bulk_get(dev, IMX132_NUM_SUPPLIES,
				      imx132->supplies);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get regulators\n");

	imx132->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_LOW);
	if (IS_ERR(imx132->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(imx132->reset_gpio),
				     "failed to get reset gpio\n");

	ret = imx132_power_on(dev);
	if (ret)
		return ret;

	ret = imx132_identify_module(imx132);
	if (ret)
		goto err_power_off;

	ret = imx132_init_controls(imx132);
	if (ret)
		goto err_power_off;

	imx132->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			    V4L2_SUBDEV_FL_HAS_EVENTS;
	imx132->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	imx132->pad.flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&imx132->sd.entity, 1, &imx132->pad);
	if (ret) {
		dev_err_probe(dev, ret, "failed to init entity pads\n");
		goto err_free_ctrls;
	}

	imx132->sd.state_lock = imx132->ctrl_handler.lock;
	ret = v4l2_subdev_init_finalize(&imx132->sd);
	if (ret < 0) {
		dev_err_probe(dev, ret, "subdev init failed\n");
		goto err_media_cleanup;
	}

	/*
	 * Keep the sensor powered until the runtime PM core takes over, so
	 * that an autosuspend cannot race the async registration.
	 */
	pm_runtime_set_active(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_enable(dev);

	ret = v4l2_async_register_subdev_sensor(&imx132->sd);
	if (ret < 0) {
		dev_err_probe(dev, ret, "failed to register subdev\n");
		goto err_pm;
	}

	pm_runtime_set_autosuspend_delay(dev, 1000);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;

err_pm:
	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);
	v4l2_subdev_cleanup(&imx132->sd);
err_media_cleanup:
	media_entity_cleanup(&imx132->sd.entity);
err_free_ctrls:
	v4l2_ctrl_handler_free(&imx132->ctrl_handler);
err_power_off:
	imx132_power_off(dev);

	return ret;
}

static void imx132_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx132 *imx132 = to_imx132(sd);
	struct device *dev = &client->dev;

	v4l2_async_unregister_subdev(sd);
	v4l2_subdev_cleanup(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&imx132->ctrl_handler);

	pm_runtime_disable(dev);
	if (!pm_runtime_status_suspended(dev))
		imx132_power_off(dev);
	pm_runtime_set_suspended(dev);
}

static const struct dev_pm_ops imx132_pm_ops = {
	SET_RUNTIME_PM_OPS(imx132_power_off, imx132_power_on, NULL)
};

static const struct of_device_id imx132_dt_ids[] = {
	{ .compatible = "sony,imx132" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx132_dt_ids);

static struct i2c_driver imx132_i2c_driver = {
	.driver = {
		.name = "imx132",
		.of_match_table	= imx132_dt_ids,
		.pm = &imx132_pm_ops,
	},
	.probe = imx132_probe,
	.remove = imx132_remove,
};

module_i2c_driver(imx132_i2c_driver);

MODULE_AUTHOR("Rob Lymm");
MODULE_DESCRIPTION("Sony IMX132 sensor driver");
MODULE_LICENSE("GPL");
