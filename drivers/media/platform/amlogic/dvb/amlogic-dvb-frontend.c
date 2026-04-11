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
#include "amlogic_dvb.h"

struct aml_dvb_fe {
	struct device *dev;
	struct dvb_adapter *adap;
	struct dvb_frontend *fe;
};

static struct component_match *aml_dvb_build_match(struct aml_dvb_fe *priv)
{
    struct device_node *fe_np = priv->dev->of_node;
	struct device_node *demod_np;
	struct device_node *tuner_np;
	struct component_match *match = NULL;

	/* demod */
	demod_np = of_parse_phandle(fe_np, "demod-dev", 0);
	if (demod_np) {
		component_match_add(priv->dev, &match, component_compare_of,
				    demod_np);
		of_node_put(demod_np);
	}

	/* tuner */
	tuner_np = of_parse_phandle(fe_np, "tuner-dev", 0);
	if (tuner_np) {
		component_match_add(priv->dev, &match, component_compare_of,
				    tuner_np);
		of_node_put(tuner_np);
	}

	if (!match) {
		dev_warn(priv->dev, "No components to bind found\n");
	}
	return match;
}

static int aml_dvb_fe_bind(struct device *dev)
{
	struct aml_dvb_fe *priv = dev_get_drvdata(dev);
	int ret;

	ret = component_bind_all(dev, &priv->fe);
	if (ret) {
		dev_info(dev, "Failed to bind components: %d\n", ret);
		return ret;
	}

	ret = dvb_register_frontend(priv->adap, priv->fe);
	if (ret) {
		dev_err(dev, "failed to register frontend: %d\n", ret);
		goto err;
	}

	dev_info(dev, "frontend attached\n");

	return 0;

err:
	dvb_frontend_detach(priv->fe);
	component_unbind_all(dev, &priv->fe);
	return ret;
}

static void aml_dvb_fe_unbind(struct device *dev)
{
	struct aml_dvb_fe *priv = dev_get_drvdata(dev);

	dvb_unregister_frontend(priv->fe);
	dvb_frontend_detach(priv->fe);
	component_unbind_all(dev, &priv->fe);
}

static const struct component_master_ops aml_dvb_fe_ops = {
	.bind = aml_dvb_fe_bind,
	.unbind = aml_dvb_fe_unbind,
};

static int aml_dvb_init_frontend(struct aml_dvb_fe *priv)
{
	struct component_match *match = NULL;
	int ret;

	match = aml_dvb_build_match(priv);
	if (!match)
		return -ENODEV;

	ret = component_master_add_with_match(priv->dev,
					      &aml_dvb_fe_ops, match);
	if (ret) {
		dev_err(priv->dev,
			"failed to add component master: %d\n", ret);
		return ret;
	}

	return 0;
}

static int aml_dvb_fe_probe(struct platform_device *pdev)
{
	struct aml_dvb_fe *priv;
	int ret;

	if (!pdev->dev.of_node)
		return -EINVAL;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	priv->adap = aml_dvb_adapter(pdev->dev.parent);
	if (IS_ERR(priv->adap)) {
		dev_err(priv->dev, "Parent node is not an amlogic-dvb driver\n");
		return PTR_ERR(priv->adap);
	}

    ret = aml_dvb_init_frontend(priv);
    if (ret) {
        return ret;
    }

	dev_info(&pdev->dev, "initialized\n");

	return 0;
}

static void aml_dvb_fe_remove(struct platform_device *pdev)
{
	struct aml_dvb_fe *priv = platform_get_drvdata(pdev);

	component_master_del(priv->dev, &aml_dvb_fe_ops);
}

static const struct of_device_id aml_dvb_fe_of_match[] = {
	{ .compatible = "amlogic,dvb-fe" },
	{}
};
MODULE_DEVICE_TABLE(of, aml_dvb_fe_of_match);

static struct platform_driver aml_dvb_fe_driver = {
	.probe  = aml_dvb_fe_probe,
	.remove = aml_dvb_fe_remove,
	.driver = {
		.name = "amlogic-dvb-fe",
		.of_match_table = aml_dvb_fe_of_match,
	},
};

module_platform_driver(aml_dvb_fe_driver);

MODULE_AUTHOR("Marek Czerski<ma.czerski@gmail.com>");
MODULE_DESCRIPTION("Amlogic DVB frontend bridge driver");
MODULE_LICENSE("GPL");
