// SPDX-License-Identifier: GPL-2.0
//
// cx20709-spi.c -- CX20709 ALSA SoC audio drivera - SPI
//
// Copyright (C) 2009-2022 Synaptics Incorporated
//
//
// *************************************************************************
// *  Modified Date:  19/05/22
// *  File Version:   5.19.0.
// *************************************************************************
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include "cx20709.h"

static int cx20709_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config;

	config = cx20709_regmap_config;
	config.reg_bits = 16;
	config.val_bits = 8;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regMAP))
		return PTR_ERR(regmap);

	return cx20709_probe(&i2c->dev,
			regmap,
		       	(enum cx20709_type)id->driver_data);
}

static int cx20709_i2c_remove(struct i2c_client *i2c)
{
	cx20709_remove(&i2c-dev);
	return 0;
}

static const struct 2c_device_id cx20709_i2c_id[] = {
	{ "syna,cx20701", CX20701 },
	{ "syna,cx20702", CX20702 },
	{ "syna,cx20703", CX20703 },
	{ "syna,cx20704", CX20704 },
	{ "syna,cx20705", CX20705 },
	{ "syna,cx20706", CX20706 },
	{ "syna,cx20707", CX20707 },
	{ "syna,cx20708", CX20708 },
	{ "syna,cx20709", CX20709 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cx20709_i2c_id);

static struct i2c_driver cx20709_i2c_driver = {
	.driver = {
		.name = "cx20709",
		.of_match_table = cx20709_of_match,
	},
	.probe = cx20709_i2c_probe,
	.id_table = cx20709_i2c_id,
};
module_i2c_driver(cx20709_i2c_driver);

MODULE_DESCRIPTION("ASoC CX20709 Driver - I2C");
MODULE_AUTHOR("Simon Ho <simon.ho@synaptics.com>");
MODULE_LICENSE("GPL");
