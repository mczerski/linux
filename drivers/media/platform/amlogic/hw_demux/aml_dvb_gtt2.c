// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Author: Marek Czerski <ma.czerski@gmail.com>
 */

#include <dvb-frontends/cxd2878.h>
#include <tuners/mxl603.h>

#include <media/dvb_frontend.h>

static int aml_dvb_gtt2_probe(struct i2c_client *client)
{
    static struct cxd2878_config cxd2878cfg = {
        .addr_slvt = 0x6c,
        .xtal = SONY_DEMOD_XTAL_24000KHz,
        .tuner_addr = 0,
        .tuner_xtal = 0,
        .ts_mode = 1,
        .ts_ser_data = 0,
        .ts_clk = 1,
        .ts_clk_mask = 1,
        .ts_valid = 0,
        .atscCoreDisable = 0,
        .lock_flag = 1,
        .write_properties = NULL,
        .read_properties = NULL,
    };
    static struct mxl603_config mxl603cfg = {
        .xtal_freq_hz = MXL603_XTAL_16MHz,
        .if_freq_hz = MXL603_IF_5MHz,
        .agc_type = MXL603_AGC_SELF,
        .xtal_cap = 16,
        .gain_level = 11,
        .if_out_gain_level = 11,
        .agc_set_point = 66,
        .agc_invert_pol = 0,
        .invert_if = 1,
        .loop_thru_enable = 0,
        .clk_out_enable = 1,
        .clk_out_div = 0,
        .clk_out_ext = 0,
        .xtal_sharing_mode = 0,
        .single_supply_3_3V = 1,
    };
    int ret = 0;
    struct i2c_adapter *i2c = client->adapter;
    struct device_node *np = client->dev.of_node;
    struct dvb_frontend * fe = dvb_attach(cxd2878_attach, &cxd2878cfg, i2c);

    if (!fe) {
        dev_err(&i2c->dev,"%s: cxd2878_attach error\n", KBUILD_MODNAME);
        return -ENODEV;
    }

    if (!dvb_attach(mxl603_attach, fe, i2c, 0x63, &mxl603cfg)) {
        dev_err(&i2c->dev,"%s: mx603_attach error\n", KBUILD_MODNAME);
        ret = -ENODEV;
        goto err;
    }

    i2c_set_clientdata(client, fe);

    if (np)
        dev_info(&i2c->dev, "have node: %s\n", np->name);
    else
        dev_info(&i2c->dev, "does not have node\n");

    return ret;

err:
    dvb_frontend_detach(fe);
    return ret;
}

static const struct i2c_device_id aml_dvb_gtt2_id_table[] = {
    { "aml-dvb-gtt2" },
    {}
};
MODULE_DEVICE_TABLE(i2c, aml_dvb_gtt2_id_table);

static const struct of_device_id aml_dvb_gtt2_dt_match[] = {
    { .compatible = "aml-dvb,cxd2878" },
    {}
};
MODULE_DEVICE_TABLE(of, aml_dvb_gtt2_dt_match);

static struct i2c_driver aml_dvb_gtt2_driver = {
    .driver = {
        .name                = "amlogic-dvb-gtt2",
        //.of_match_table      = aml_dvb_gtt2_dt_match,
    },
    .probe      = aml_dvb_gtt2_probe,
    .id_table   = aml_dvb_gtt2_id_table,
};

module_i2c_driver(aml_dvb_gtt2_driver);

MODULE_AUTHOR("Marek Czerski<ma.czerski@gmail.com>");
MODULE_DESCRIPTION("Amlogic DVB for GTT-2 frontend driver");
MODULE_LICENSE("GPL");

