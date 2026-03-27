// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Author: Marek Czerski <ma.czerski@gmail.com>
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <media/dvb_frontend.h>
#include "aml_dvb.h"

#define MAX_FE 3

struct aml_dvb_bridge {
	struct device *dev;
	struct dvb_adapter *adap;
	struct dvb_frontend *fe[MAX_FE];
	struct i2c_client *demod[MAX_FE];
	struct i2c_client *tuner[MAX_FE];
	bool demod_module[MAX_FE];
	bool tuner_module[MAX_FE];
	int num_fe;
};

static struct dvb_adapter *aml_dvb_get_dvb_adapter(struct aml_dvb_bridge *br)
{
	struct device_node *demux_np;
	struct platform_device *demux_pdev;
	struct dvb_adapter *adap;

	demux_np = of_parse_phandle(br->dev->of_node, "demux", 0);
	if (!demux_np) {
		dev_err(br->dev, "failed to read demux\n");
		return NULL;
	}

	demux_pdev = of_find_device_by_node(demux_np);
	of_node_put(demux_np);
	if (!demux_pdev) {
		dev_err(br->dev, "demux is not a proper device\n");
		return ERR_PTR(-EINVAL);
	}

	adap = aml_get_dvb_adapter(&demux_pdev->dev);
    if (IS_ERR(adap)) {
        dev_err(br->dev, "demux is not a amlogic-dvb-demux driver\n");
    }
	return adap;
}

static struct i2c_client *aml_dvb_get_demod(struct aml_dvb_bridge *br,
					    struct device_node *fe_np)
{
	struct device_node *demod_np;
	struct i2c_client *demod;

	demod_np = of_parse_phandle(fe_np, "demod-dev", 0);
	if (!demod_np) {
		dev_err(br->dev, "failed to read demod-dev for frontend %s\n",
			fe_np->name);
		return NULL;
	}

	demod = of_find_i2c_device_by_node(demod_np);
	of_node_put(demod_np);
	if (!demod || !i2c_client_has_driver(demod)) {
		dev_err(br->dev, "demod-dev is not i2c device on frontend %s\n",
			fe_np->name);
		return NULL;
	}

	return demod;
}

static struct i2c_client *aml_dvb_get_tuner(struct aml_dvb_bridge *br,
					    struct device_node *fe_np)
{
	struct device_node *tuner_np;
	struct i2c_client *tuner;

	tuner_np = of_parse_phandle(fe_np, "tuner-dev", 0);
	if (!tuner_np) {
		dev_err(br->dev, "failed to read tuner-dev for frontend %s\n",
			fe_np->name);
		return NULL;
	}

	tuner = of_find_i2c_device_by_node(tuner_np);
	of_node_put(tuner_np);
	if (!tuner || !i2c_client_has_driver(tuner)) {
		dev_err(br->dev, "tuner-dev is not i2c device on frontend %s\n",
			fe_np->name);
		return NULL;
	}

	return tuner;
}

static struct i2c_adapter *aml_dvb_get_i2c_adapter(struct device *dev,
						   struct device_node *np,
						   const char *phandle)
{
	struct device_node *i2c_np;
	struct i2c_adapter *i2c;

	i2c_np = of_parse_phandle(np, phandle, 0);
	if (!i2c_np)
		return NULL;

	i2c = of_get_i2c_adapter_by_node(i2c_np);
	of_node_put(i2c_np);

	return i2c;
}

static int aml_dvb_get_demod_by_module(struct aml_dvb_bridge *br,
				       struct device_node *fe_np,
				       const char *demod_module)
{
	struct i2c_adapter *i2c;
	struct i2c_client *demod;
	struct dvb_frontend *fe;
	const char *demod_id = NULL;
	u32 demod_addr;

	of_property_read_string(fe_np, "demod-id", &demod_id);

	if (of_property_read_u32(fe_np, "demod-i2c-addr", &demod_addr)) {
		dev_err(br->dev,
			"failed to read demod-i2c-addr for frontend %s\n",
			fe_np->name);
		return -EINVAL;
	}

	i2c = aml_dvb_get_i2c_adapter(br->dev, fe_np, "demod-i2c-bus");
	if (!i2c) {
		dev_err(br->dev,
			"failed to read demod-i2c-bus for frontend %s\n",
			fe_np->name);
		return -EINVAL;
	}

	demod = dvb_module_probe(demod_module, demod_id, i2c, demod_addr, NULL);
	if (!demod) {
		dev_err(br->dev,
			"failed to claim demod for frontend %s. One of demod-module or demod-dev must be properly defined\n",
			fe_np->name);
		goto err_adapter;
	}

	fe = i2c_get_clientdata(demod);
	if (!fe) {
		dev_err(br->dev, "demod missing client data for frontend %s\n",
			fe_np->name);
		goto err_module;
	}

	br->demod[br->num_fe] = demod;
	br->fe[br->num_fe] = fe;

	return 0;

err_module:
	dvb_module_release(demod);

err_adapter:
    i2c_put_adapter(i2c);
	return -ENODEV;
}

static int aml_dvb_get_tuner_by_module(struct aml_dvb_bridge *br,
				       struct device_node *fe_np,
				       const char *tuner_module)
{
	struct i2c_adapter *i2c;
	struct i2c_client *tuner;
	const char *tuner_id;
	u32 tuner_addr;

	of_property_read_string(fe_np, "tuner-id", &tuner_id);

	if (of_property_read_u32(fe_np, "tuner-i2c-addr", &tuner_addr)) {
		dev_err(br->dev,
			"failed to read tuner-i2c-addr for frontend %s\n",
			fe_np->name);
		return -EINVAL;
	}

	i2c = aml_dvb_get_i2c_adapter(br->dev, fe_np, "tuner-i2c-bus");
	if (!i2c) {
		dev_err(br->dev,
			"failed to read tuner-i2c-bus for frontend %s\n",
			fe_np->name);
		return -EINVAL;
	}

	tuner = dvb_module_probe(tuner_module, tuner_id, i2c, tuner_addr,
				 br->fe[br->num_fe]);
	if (!tuner) {
		dev_err(br->dev,
			"failed to claim tuner for frontend %s. One of tuner-module or runer-dev must be properly defined\n",
			fe_np->name);
        goto err_adapter;
	}

	br->tuner[br->num_fe] = tuner;

	return 0;

err_adapter:
	i2c_put_adapter(i2c);
	return -ENODEV;
}

static int aml_dvb_get_demod_by_device(struct aml_dvb_bridge *br,
				       struct device_node *fe_np)
{
	struct i2c_client *demod;
	struct dvb_frontend *fe;

	demod = aml_dvb_get_demod(br, fe_np);
	if (!demod) {
		return -ENODEV;
	}

    if (!try_module_get(demod->dev.driver->owner)) {
		dev_err(br->dev, "faild to claim demod driver module for frontend %s\n",
			fe_np->name);
        goto err_device;
    }

	fe = i2c_get_clientdata(demod);
	if (!fe) {
		dev_err(br->dev, "demod missing client data for frontend %s\n",
			fe_np->name);
        goto err_module;
	}

	br->demod[br->num_fe] = demod;
	br->fe[br->num_fe] = fe;

	return 0;

err_module:
    module_put(demod->dev.driver->owner);
err_device:
    put_device(&demod->dev);
    return -ENODEV;
}

static int aml_dvb_get_tuner_by_device(struct aml_dvb_bridge *br,
				       struct device_node *fe_np)
{
	struct i2c_client *tuner;

	tuner = aml_dvb_get_tuner(br, fe_np);
	if (!tuner) {
		return -ENODEV;
	}

    if (!try_module_get(tuner->dev.driver->owner)) {
		dev_err(br->dev, "faild to claim tuner driver module for frontend %s\n",
			fe_np->name);
        goto err;
    }

	br->tuner[br->num_fe] = tuner;

	return 0;

err:
    put_device(&tuner->dev);
    return -ENODEV;
}

static int aml_dvb_bridge_init(struct aml_dvb_bridge *br)
{
	struct device_node *fes, *fe_np;
	int i, ret;

	br->adap = aml_dvb_get_dvb_adapter(br);
    if (IS_ERR(br->adap)) {
        return PTR_ERR(br->adap);
    }
	if (!br->adap) {
		return -EPROBE_DEFER;
    }

	br->num_fe = 0;

	fes = of_get_child_by_name(br->dev->of_node, "frontends");
	if (!fes) {
		dev_err(br->dev, "failed to read frontends\n");
		return -EINVAL;
	}

	for_each_child_of_node(fes, fe_np) {
		const char *demod_module, *tuner_module;
		bool is_demod_module, is_tuner_module;

		if (br->num_fe >= MAX_FE)
			break;

		is_demod_module = of_property_read_string(fe_np, "demod-module",
							  &demod_module) == 0;
		if (is_demod_module) {
			if (aml_dvb_get_demod_by_module(br, fe_np,
							demod_module))
				continue;
		} else {
			if (aml_dvb_get_demod_by_device(br, fe_np))
				continue;
		}
		br->demod_module[br->num_fe] = is_demod_module;

		is_tuner_module = of_property_read_string(fe_np, "tuner-module",
							  &tuner_module) == 0;
		if (is_tuner_module) {
			aml_dvb_get_tuner_by_module(br, fe_np, tuner_module);
		} else {
			aml_dvb_get_tuner_by_device(br, fe_np);
		}
		br->tuner_module[br->num_fe] = is_tuner_module;

		br->num_fe++;
	}

	if (!br->num_fe) {
		dev_err(br->dev, "no frontends defined\n");
		return -EINVAL;
	}

	for (i = 0; i < br->num_fe; i++) {
		ret = dvb_register_frontend(br->adap, br->fe[i]);
		if (ret) {
			dev_err(br->dev, "failed to register fe-%d: %d\n", i,
				ret);
			goto err;
		}
	}

	dev_info(br->dev, "attached %d frontend(s)\n", br->num_fe);

	return 0;

err:
	while (--i >= 0) {
		dvb_unregister_frontend(br->fe[i]);
		dvb_frontend_detach(br->fe[i]);
	}

	return ret;
}

static int aml_dvb_bridge_probe(struct platform_device *pdev)
{
	struct aml_dvb_bridge *br;
	int ret;

	if (!pdev->dev.of_node)
		return -EINVAL;

	br = devm_kzalloc(&pdev->dev, sizeof(*br), GFP_KERNEL);
	if (!br)
		return -ENOMEM;

	br->dev = &pdev->dev;
	platform_set_drvdata(pdev, br);

	ret = aml_dvb_bridge_init(br);
	if (ret == -EPROBE_DEFER)
		return ret;

	if (ret) {
		dev_err(&pdev->dev, "aml dvb bridge probe failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static void aml_dvb_bridge_remove(struct platform_device *pdev)
{
	struct aml_dvb_bridge *br = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < br->num_fe; i++) {
		dvb_unregister_frontend(br->fe[i]);
		dvb_frontend_detach(br->fe[i]);
		br->fe[i] = NULL;
		if (br->tuner[i] && br->tuner_module[i]) {
			dvb_module_release(br->tuner[i]);
            i2c_put_adapter(br->tuner[i]->adapter);
		} else if (br->tuner[i]) {
            module_put(br->tuner[i]->dev.driver->owner);
			put_device(&br->tuner[i]->dev);
		}
		if (br->demod_module[i]) {
			dvb_module_release(br->demod[i]);
            i2c_put_adapter(br->demod[i]->adapter);
		} else {
            module_put(br->demod[i]->dev.driver->owner);
			put_device(&br->demod[i]->dev);
		}
		br->demod[i] = NULL;
		br->tuner[i] = NULL;
	}

	br->num_fe = 0;
}

static const struct of_device_id aml_dvb_bridge_of_match[] = {
	{ .compatible = "amlogic,dvb-bridge" },
	{}
};
MODULE_DEVICE_TABLE(of, aml_dvb_bridge_of_match);

static struct platform_driver aml_dvb_bridge_driver = {
    .probe  = aml_dvb_bridge_probe,
    .remove = aml_dvb_bridge_remove,
    .driver = {
        .name = "amlogic-dvb-bridge",
        .of_match_table = aml_dvb_bridge_of_match,
    },
};

module_platform_driver(aml_dvb_bridge_driver);

MODULE_AUTHOR("Marek Czerski<ma.czerski@gmail.com>");
MODULE_DESCRIPTION("Amlogic DVB frontend bridge driver");
MODULE_LICENSE("GPL");
