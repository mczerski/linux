// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MaxLinear MxL603 Tuner Driver
 *
 * Copyright (C) 2014 Sasa Savic <sasa.savic.sr@gmail.com>
 */

#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/property.h>
#include <media/dvb_frontend.h>
#include <media/dvb_fwnode.h>
#include "mxl603.h"
#include "mxl60x_common.h"

struct dvb_frontend *mxl603_attach(struct dvb_frontend *fe,
				   struct i2c_adapter *i2c, u8 addr,
				   struct mxl603_config *cfg)
{
	struct mxl60x_config common = {
		.xtal_freq_hz      = (enum mxl60x_xtal_freq)cfg->xtal_freq_hz,
		.if_freq_hz        = (enum mxl60x_if_freq)cfg->if_freq_hz,
		.agc_type          = (enum mxl60x_agc)cfg->agc_type,
		.xtal_cap          = cfg->xtal_cap,
		.gain_level        = cfg->gain_level,
		.if_out_gain_level = cfg->if_out_gain_level,
		.agc_set_point     = cfg->agc_set_point,
		.agc_invert_pol    = cfg->agc_invert_pol,
		.invert_if         = cfg->invert_if,
		.loop_thru_enable  = cfg->loop_thru_enable,
		.clk_out_enable    = cfg->clk_out_enable,
		.clk_out_div       = cfg->clk_out_div,
		.clk_out_ext       = cfg->clk_out_ext,
		.xtal_sharing_mode = cfg->xtal_sharing_mode,
		.single_supply_3_3V = cfg->single_supply_3_3V,
	};

	return mxl60x_attach(fe, i2c, addr, &common, MXL60X_603);
}
EXPORT_SYMBOL_GPL(mxl603_attach);

/*
 * Device-tree glue: the tuner as an I2C device that attaches to the frontend
 * connected to its IF output (include/media/dvb_fwnode.h).
 */
struct mxl603_dt {
	struct dvb_fe_part part;
	struct mxl603_config cfg;
	struct i2c_adapter *i2c;
	u8 addr;
};

static int mxl603_dt_attach(struct dvb_fe_part *part, struct dvb_frontend *fe)
{
	struct mxl603_dt *dt = container_of(part, struct mxl603_dt, part);

	return dvb_attach(mxl603_attach, fe, dt->i2c, dt->addr, &dt->cfg) ?
	       0 : -ENODEV;
}

static const struct dvb_fe_part_ops mxl603_dt_part_ops = {
	.attach = mxl603_dt_attach,
};

/* 5 MHz IF; xtal_cap, AGC and IF inversion of the WeTek Play 2 ATSC card */
static const struct mxl603_config mxl603_dt_default_config = {
	.if_freq_hz = MXL603_IF_5MHz,
	.agc_type = MXL603_AGC_SELF,
	.xtal_cap = 31,
	.gain_level = 11,
	.if_out_gain_level = 11,
	.agc_set_point = 66,
	.clk_out_enable = 1,
	.single_supply_3_3V = 1,
};

static int mxl603_dt_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct mxl603_dt *dt;
	struct clk *xtal;
	u32 val;

	dt = devm_kzalloc(dev, sizeof(*dt), GFP_KERNEL);
	if (!dt)
		return -ENOMEM;

	dt->cfg = mxl603_dt_default_config;
	dt->addr = client->addr;
	dt->i2c = client->adapter;

	xtal = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(xtal))
		return dev_err_probe(dev, PTR_ERR(xtal),
				     "failed to get crystal clock\n");

	switch (clk_get_rate(xtal)) {
	case 16000000:
		dt->cfg.xtal_freq_hz = MXL603_XTAL_16MHz;
		break;
	case 24000000:
		dt->cfg.xtal_freq_hz = MXL603_XTAL_24MHz;
		break;
	default:
		return dev_err_probe(dev, -EINVAL,
				     "unsupported crystal frequency %lu Hz\n",
				     clk_get_rate(xtal));
	}

	if (!device_property_read_u32(dev, "maxlinear,xtal-cap", &val)) {
		if (val > 31)
			return dev_err_probe(dev, -EINVAL,
					     "invalid maxlinear,xtal-cap %u\n",
					     val);
		dt->cfg.xtal_cap = val;
	}
	if (device_property_read_bool(dev, "maxlinear,external-agc"))
		dt->cfg.agc_type = MXL603_AGC_EXTERNAL;
	dt->cfg.invert_if = device_property_read_bool(dev, "maxlinear,invert-if");

	dt->part.dev = dev;
	dt->part.ops = &mxl603_dt_part_ops;
	dt->part.in_port = DVB_FE_PORT_NONE;
	dt->part.out_port = DVB_FE_PORT_OUT;

	return devm_dvb_fe_part_register(&dt->part);
}

static const struct of_device_id mxl603_of_match[] = {
	{ .compatible = "maxlinear,mxl603" },
	{ }
};
MODULE_DEVICE_TABLE(of, mxl603_of_match);

static struct i2c_driver mxl603_dt_driver = {
	.driver = {
		.name = "mxl603",
		.of_match_table = mxl603_of_match,
	},
	.probe = mxl603_dt_probe,
};
module_i2c_driver(mxl603_dt_driver);

MODULE_DESCRIPTION("MaxLinear MxL603 Tuner Driver");
MODULE_AUTHOR("Sasa Savic <sasa.savic.sr@gmail.com>");
MODULE_LICENSE("GPL");
