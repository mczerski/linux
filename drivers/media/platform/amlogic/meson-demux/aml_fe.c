// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Amlogic DVB frontend bridge - component framework version
 * Author: Marek Czerski <ma.czerski@gmail.com>
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/component.h>
#include <media/dvb_frontend.h>
#include "aml_dvb.h"

#define MAX_FRONTENDS 4

struct aml_dvb_frontend {
	struct device dev;
	struct aml_dvb_bridge *bridge;
	struct dvb_frontend fe;
	bool initialized;
};

struct aml_dvb_bridge {
	struct device *dev;
	struct dvb_adapter *adap;
	struct aml_dvb_frontend frontends[MAX_FRONTENDS];
};

static void aml_dvb_frontend_release(struct device *dev)
{
	/* No-op since frontends are embedded in the bridge structure */
}

static struct device_type aml_dvb_frontend_type = {
	.name = "aml_dvb_frontend",
	.release = aml_dvb_frontend_release,
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

static struct component_match *aml_dvb_build_match(struct device *dev,
						   struct device_node *fe_np)
{
	struct device_node *demod_np;
	struct device_node *tuner_np;
	struct component_match *match = NULL;

	/* demod */
	demod_np = of_parse_phandle(fe_np, "demod-dev", 0);
	if (demod_np) {
		component_match_add(dev, &match, component_compare_of,
				    demod_np);
		of_node_put(demod_np);
	}

	/* tuner */
	tuner_np = of_parse_phandle(fe_np, "tuner-dev", 0);
	if (tuner_np) {
		component_match_add(dev, &match, component_compare_of,
				    tuner_np);
		of_node_put(tuner_np);
	}

	if (!match) {
		dev_warn(dev, "No components to bind found\n");
	}
	return match;
}

static int aml_dvb_frontend_bind(struct device *dev)
{
	struct aml_dvb_frontend *frontend =
		container_of(dev, struct aml_dvb_frontend, dev);
	struct aml_dvb_bridge *br = frontend->bridge;
	int ret;

	ret = component_bind_all(dev, &frontend->fe);
	if (ret) {
		dev_info(dev, "Failed to bind components: %d\n", ret);
		return ret;
	}

	ret = dvb_register_frontend(br->adap, &frontend->fe);
	if (ret) {
		dev_err(dev, "failed to register frontend: %d\n", ret);
		goto err;
	}

	dev_info(dev, "frontend attached\n");

	return 0;

err:
	dvb_frontend_detach(&frontend->fe);
	component_unbind_all(dev, &frontend->fe);
	return ret;
}

static void aml_dvb_frontend_unbind(struct device *dev)
{
	struct aml_dvb_frontend *frontend =
		container_of(dev, struct aml_dvb_frontend, dev);

	dvb_unregister_frontend(&frontend->fe);
	dvb_frontend_detach(&frontend->fe);
	component_unbind_all(dev, &frontend->fe);
}

static const struct component_master_ops aml_dvb_frontend_ops = {
	.bind = aml_dvb_frontend_bind,
	.unbind = aml_dvb_frontend_unbind,
};

static int aml_dvb_init_frontend(struct aml_dvb_bridge *br,
				 struct device_node *fe_np, int index)
{
	struct aml_dvb_frontend *frontend = &br->frontends[index];
	struct component_match *match = NULL;
	int ret;

	match = aml_dvb_build_match(br->dev, fe_np);
	if (!match)
		return -ENODEV;

	memset(frontend, 0, sizeof(*frontend));
	frontend->bridge = br;

	/* unique device is required for each frontend for component matching to work */
	device_initialize(&frontend->dev);
	frontend->dev.parent = br->dev;
	frontend->dev.type = &aml_dvb_frontend_type;
	frontend->dev.of_node = of_node_get(fe_np);
	dev_set_name(&frontend->dev, "frontend%d", index);

	ret = device_add(&frontend->dev);
	if (ret) {
		dev_err(br->dev, "failed to add frontend device %d: %d\n",
			index, ret);
		goto err1;
	}

	ret = component_master_add_with_match(&frontend->dev,
					      &aml_dvb_frontend_ops, match);
	if (ret) {
		dev_err(br->dev,
			"failed to add component master for frontend %d: %d\n",
			index, ret);
		goto err2;
	}

	frontend->initialized = true;

	return 0;

err2:
	device_del(&frontend->dev);

err1:
	of_node_put(fe_np);
	put_device(&frontend->dev);
	return ret;
}

static void aml_dvb_cleanup_frontend(struct aml_dvb_frontend *frontend)
{
	if (!frontend->initialized)
		return;

	component_master_del(&frontend->dev, &aml_dvb_frontend_ops);
	device_del(&frontend->dev);
	of_node_put(frontend->dev.of_node);
	put_device(&frontend->dev);
	frontend->initialized = false;
}

static int aml_dvb_bridge_probe(struct platform_device *pdev)
{
	struct aml_dvb_bridge *br;
	struct device_node *child;
	int i, ret;
	int fe_index = 0;

	if (!pdev->dev.of_node)
		return -EINVAL;

	br = devm_kzalloc(&pdev->dev, sizeof(*br), GFP_KERNEL);
	if (!br)
		return -ENOMEM;

	br->dev = &pdev->dev;
	platform_set_drvdata(pdev, br);

	br->adap = aml_dvb_get_dvb_adapter(br);
	if (IS_ERR(br->adap)) {
		return PTR_ERR(br->adap);
	}
	if (!br->adap) {
		return -EPROBE_DEFER;
	}

	/* Parse frontend child nodes */
	for_each_available_child_of_node(pdev->dev.of_node, child) {
		if (fe_index >= MAX_FRONTENDS) {
			dev_warn(&pdev->dev, "too many frontends, max is %d\n",
				 MAX_FRONTENDS);
			of_node_put(child);
			break;
		}

		ret = aml_dvb_init_frontend(br, child, fe_index);
		if (ret == -ENODEV) {
			dev_dbg(&pdev->dev,
				"no components for frontend %d, skipping\n",
				fe_index);
			continue;
		} else if (ret) {
			dev_err(&pdev->dev,
				"failed to initialize frontend %d: %d\n",
				fe_index, ret);
			of_node_put(child);
			goto err_cleanup;
		}
		fe_index++;
	}

	if (fe_index == 0) {
		dev_err(&pdev->dev, "no frontends found\n");
		return -EINVAL;
	}

	dev_info(&pdev->dev, "initialized %d frontend(s)\n", fe_index);

	return 0;

err_cleanup:
	for (i = 0; i < MAX_FRONTENDS; i++) {
		aml_dvb_cleanup_frontend(&br->frontends[i]);
	}
	return ret;
}

static void aml_dvb_bridge_remove(struct platform_device *pdev)
{
	struct aml_dvb_bridge *br = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < MAX_FRONTENDS; i++) {
		aml_dvb_cleanup_frontend(&br->frontends[i]);
	}
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
