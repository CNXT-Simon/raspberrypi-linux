// SPDX-License-Identifier: GPL-2.0
/*
 * cx2070x.h -- ALSA SoC audio driver for CX2070X
 *
 * Copyright (C) 2022 Synaptics Incorporated.
 *
 *************************************************************************
 *  Modified Date:  19/05/22
 *  File Version:   5.19.0.0
 *************************************************************************
 */
#ifndef __CX2070X_H__
#define __CX2070X_H__

extern const struct dev_pm_ops cx2070x_pm_ops;
extern const struct regmap_config cx2070x_regmap_config;

extern int cx2070x_probe(struct device *dev, struct regmap *regmap);
extern void cx2070x_remove(struct device *dev);

#endif /* #ifdef __CX2070X_H__ */
