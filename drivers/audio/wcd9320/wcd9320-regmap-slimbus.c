// SPDX-License-Identifier: GPL-2.0-only
/*
 * regmap over SLIMbus for the wcd9320 codec. The running kernel was built
 * without CONFIG_REGMAP_SLIMBUS (nothing in its config selects it), so this
 * is drivers/base/regmap/regmap-slimbus.c reproduced for this module, on the
 * exported __regmap_init().
 */
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slimbus.h>

static int wcd9320_regmap_slimbus_write(void *context, const void *data, size_t count)
{
	struct slim_device *sdev = context;

	return slim_write(sdev, *(u16 *)data, count - 2, (u8 *)data + 2);
}

static int wcd9320_regmap_slimbus_read(void *context, const void *reg, size_t reg_size,
				       void *val, size_t val_size)
{
	struct slim_device *sdev = context;

	return slim_read(sdev, *(u16 *)reg, val_size, val);
}

static const struct regmap_bus wcd9320_regmap_slimbus_bus = {
	.write = wcd9320_regmap_slimbus_write,
	.read = wcd9320_regmap_slimbus_read,
	.reg_format_endian_default = REGMAP_ENDIAN_LITTLE,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};

struct regmap *wcd9320_regmap_init_slimbus(struct slim_device *slimbus,
					   struct regmap_config *config)
{
	static struct lock_class_key lock_key;

	if (config->reg_bits != 16 || config->val_bits != 8)
		return ERR_PTR(-ENOTSUPP);

	return __regmap_init(&slimbus->dev, &wcd9320_regmap_slimbus_bus, &slimbus->dev,
			     config, &lock_key, "wcd9320-slim");
}
