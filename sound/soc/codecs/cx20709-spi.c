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
//#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#include "cx20709.h"

static int cx20709_spi_probe(struct spi_device *spi)
{
	struct regmap *regmap;
	struct regmap_config config;

	config = cx20709_regmap_config;
	config.reg_bits = 16;
	config.val_bits = 8;
	config.read_flag_mask = 0x21;
	config.write_flag_mask = 0x20;

	regmap = devm_regmap_init_spi(spi, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);


	return cx20709_probe(&spi->dev,
			regmap,
		       	(enum cx20709_type)spi->driver_data);
}

static const struct spi_device_id cx20709_spi_id[] = {
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
MODULE_DEVICE_TABLE(spi, cx20709_spi_id);

static struct spi_driver cx20709_spi_driver = {
	.driver = {
		.name	= "cx20709",
		.of_match_table = cx2082x_of_match,
	},
	.probe = cx20709_spi_probe,
	.id_table = cx20709_spi_id,
};
module_spi_driver(cx20709_spi_driver);

MODULE_DESCRIPTION("ASoC CX20709 Driver - SPI");
MODULE_AUTHOR("Simon Ho <simon.ho@synaptics.com>");
MODULE_LICENSE("GPL v2");
