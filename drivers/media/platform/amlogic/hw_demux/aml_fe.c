#include <dvb-frontends/cxd2878.h>
#include <tuners/mxl603.h>

#include <media/dvb_frontend.h>

static int aml_fe_probe(struct i2c_client *client)
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
    struct dvb_adapter *adap = client->dev.platform_data;
    struct i2c_adapter *i2c = client->adapter;

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

    ret = dvb_register_frontend(adap, fe);
    if (ret < 0) {
        dev_err(&i2c->dev,"%s: dvb frontend register error: %d.\n", KBUILD_MODNAME, ret);
        goto err;
    }

    i2c_set_clientdata(client, fe);

    return ret;

err:
    dvb_frontend_detach(fe);
    return ret;
}

static void aml_fe_remove(struct i2c_client *client)
{
    int ret;
    struct dvb_frontend *fe = i2c_get_clientdata(client);

    ret = dvb_unregister_frontend(fe);
    if (ret < 0) {
        dev_err(&client->dev,"%s: dvb frontend unregister error: %d.\n", KBUILD_MODNAME, ret);
    }
    else {
        dev_info(&client->dev, "%s: frontend successfully removed.\n", KBUILD_MODNAME);
    }
    dvb_frontend_detach(fe);
}

static const struct i2c_device_id aml_fe_id_table[] = {
    { "aml-fe" },
    {}
};
MODULE_DEVICE_TABLE(i2c, aml_fe_id_table);

static struct i2c_driver aml_fe_driver = {
    .driver = {
        .name                = "aml-fe",
        .suppress_bind_attrs = true,
    },
    .probe      = aml_fe_probe,
    .remove     = aml_fe_remove,
    .id_table   = aml_fe_id_table,
};

module_i2c_driver(aml_fe_driver);

MODULE_AUTHOR("Marek Czerski<ma.czerski@gmail.com>");
MODULE_DESCRIPTION("Amlogic frontend drivers");
MODULE_LICENSE("GPL");
