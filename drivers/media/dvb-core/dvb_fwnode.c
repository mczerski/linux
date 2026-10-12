// SPDX-License-Identifier: GPL-2.0-only
/*
 * DVB frontends described by firmware nodes
 *
 * Frontend parts register themselves in a list. A TS receiver follows the
 * firmware graph from its endpoint to a demodulator and from there through
 * the input ports to the other parts, claims them and assembles the
 * frontend.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/property.h>
#include <linux/slab.h>

#include <media/dvb_frontend.h>
#include <media/dvb_fwnode.h>

/* demodulator, tuners, SEC and other parts of one frontend */
#define DVB_FE_MAX_PARTS	8

static DEFINE_MUTEX(dvb_fe_parts_lock);
static LIST_HEAD(dvb_fe_parts);

/**
 * struct dvb_fe_chain - the parts of a frontend used by a TS receiver
 * @consumer: device of the TS receiver
 * @fe: frontend of the demodulator
 * @parts: the demodulator, then the parts behind its input
 * @owners: modules of the drivers of @parts
 * @num_parts: number of entries in @parts
 * @num_claimed: parts marked busy, linked and with their module pinned
 * @num_attached: parts after the demodulator attached to @fe
 */
struct dvb_fe_chain {
	struct device *consumer;
	struct dvb_frontend *fe;
	struct dvb_fe_part *parts[DVB_FE_MAX_PARTS];
	struct module *owners[DVB_FE_MAX_PARTS];
	unsigned int num_parts;
	unsigned int num_claimed;
	unsigned int num_attached;
};

static struct dvb_fe_part *dvb_fe_part_find(const struct fwnode_handle *fwnode,
					    u32 port)
{
	struct dvb_fe_part *part;

	lockdep_assert_held(&dvb_fe_parts_lock);

	list_for_each_entry(part, &dvb_fe_parts, list)
		if (dev_fwnode(part->dev) == fwnode && part->out_port == port)
			return part;

	return NULL;
}

static void dvb_fe_part_unregister(void *data)
{
	struct dvb_fe_part *part = data;

	mutex_lock(&dvb_fe_parts_lock);
	WARN_ON(part->busy);
	list_del(&part->list);
	mutex_unlock(&dvb_fe_parts_lock);
}

int devm_dvb_fe_part_register(struct dvb_fe_part *part)
{
	bool demod;
	int ret = 0;

	if (!part->dev || !dev_fwnode(part->dev) || !part->ops)
		return -EINVAL;

	/* a demodulator provides a frontend, every other part attaches to one */
	demod = !!part->ops->get_frontend;
	if (demod == !!part->ops->attach)
		return -EINVAL;

	part->busy = false;

	mutex_lock(&dvb_fe_parts_lock);
	if (dvb_fe_part_find(dev_fwnode(part->dev), part->out_port))
		ret = -EEXIST;
	else
		list_add_tail(&part->list, &dvb_fe_parts);
	mutex_unlock(&dvb_fe_parts_lock);
	if (ret)
		return ret;

	return devm_add_action_or_reset(part->dev, dvb_fe_part_unregister, part);
}
EXPORT_SYMBOL_GPL(devm_dvb_fe_part_register);

/*
 * Add the part whose output port is the remote end of @ep to @chain. @demod
 * is set for the endpoint of the TS receiver, whose remote end must be the
 * TS output of a demodulator.
 */
static int dvb_fe_chain_add(struct dvb_fe_chain *chain,
			    struct fwnode_handle *ep, bool demod)
{
	struct fwnode_handle *remote __free(fwnode_handle) =
		fwnode_graph_get_remote_endpoint(ep);
	struct fwnode_handle *parent __free(fwnode_handle) = NULL;
	struct fwnode_endpoint fep;
	struct dvb_fe_part *part;
	unsigned int i;
	int ret;

	if (!remote)
		return -ENOLINK;

	parent = fwnode_graph_get_port_parent(remote);
	if (!parent)
		return -ENOLINK;

	if (!fwnode_device_is_available(parent))
		return -ENODEV;

	ret = fwnode_graph_parse_endpoint(remote, &fep);
	if (ret)
		return ret;

	part = dvb_fe_part_find(parent, fep.port);
	if (!part)
		return dev_err_probe(chain->consumer, -EPROBE_DEFER,
				     "waiting for %pfwf port %u\n",
				     parent, fep.port);

	if (demod != !!part->ops->get_frontend) {
		dev_err(chain->consumer, "%pfwf port %u is not a %s output\n",
			parent, fep.port,
			demod ? "demodulator" : "tuner or SEC");
		return -EINVAL;
	}

	if (part->busy) {
		dev_err(chain->consumer, "%pfwf is used for another TS input\n",
			parent);
		return -EBUSY;
	}

	for (i = 0; i < chain->num_parts; i++) {
		if (chain->parts[i] == part) {
			dev_err(chain->consumer, "loop at %pfwf\n", parent);
			return -ELOOP;
		}
	}

	if (chain->num_parts == ARRAY_SIZE(chain->parts)) {
		dev_err(chain->consumer, "more than %u frontend parts\n",
			DVB_FE_MAX_PARTS);
		return -E2BIG;
	}

	chain->parts[chain->num_parts++] = part;

	return 0;
}

/* Collect the demodulator behind @endpoint and the parts behind its input */
static int dvb_fe_chain_collect(struct dvb_fe_chain *chain,
				struct fwnode_handle *endpoint)
{
	unsigned int i;
	int ret;

	lockdep_assert_held(&dvb_fe_parts_lock);

	ret = dvb_fe_chain_add(chain, endpoint, true);
	if (ret)
		return ret;

	for (i = 0; i < chain->num_parts; i++) {
		struct dvb_fe_part *part = chain->parts[i];
		struct fwnode_handle *ep;

		if (part->in_port == DVB_FE_PORT_NONE)
			continue;

		fwnode_graph_for_each_endpoint(dev_fwnode(part->dev), ep) {
			struct fwnode_endpoint fep;

			if (fwnode_graph_parse_endpoint(ep, &fep) ||
			    fep.port != part->in_port)
				continue;

			ret = dvb_fe_chain_add(chain, ep, false);
			/* unconnected and disabled inputs are left out */
			if (ret == -ENOLINK || ret == -ENODEV)
				continue;
			if (ret) {
				fwnode_handle_put(ep);
				return ret;
			}
		}
	}

	return 0;
}

static int dvb_fe_chain_claim(struct dvb_fe_chain *chain)
{
	unsigned int i;

	lockdep_assert_held(&dvb_fe_parts_lock);

	for (i = 0; i < chain->num_parts; i++) {
		struct dvb_fe_part *part = chain->parts[i];
		struct device_link *link;

		/*
		 * The managed link makes the driver core unbind the TS
		 * receiver before the driver of the part. If that driver is
		 * already being unbound, the part is about to go away.
		 */
		link = device_link_add(chain->consumer, part->dev,
				       DL_FLAG_AUTOREMOVE_CONSUMER);
		if (!link) {
			dev_err(chain->consumer, "cannot link to %s\n",
				dev_name(part->dev));
			return -EINVAL;
		}
		if (READ_ONCE(link->status) == DL_STATE_SUPPLIER_UNBIND)
			return dev_err_probe(chain->consumer, -EPROBE_DEFER,
					     "%s is being unbound\n",
					     dev_name(part->dev));

		chain->owners[i] = part->dev->driver->owner;
		if (!try_module_get(chain->owners[i]))
			return dev_err_probe(chain->consumer, -EPROBE_DEFER,
					     "driver of %s is being removed\n",
					     dev_name(part->dev));

		part->busy = true;
		chain->num_claimed++;
	}

	return 0;
}

static void dvb_fe_chain_unclaim(struct dvb_fe_chain *chain)
{
	unsigned int i;

	mutex_lock(&dvb_fe_parts_lock);
	for (i = 0; i < chain->num_claimed; i++)
		chain->parts[i]->busy = false;
	mutex_unlock(&dvb_fe_parts_lock);

	for (i = 0; i < chain->num_claimed; i++)
		module_put(chain->owners[i]);

	chain->num_claimed = 0;
}

static void dvb_fe_chain_detach(struct dvb_fe_chain *chain)
{
	while (chain->num_attached) {
		struct dvb_fe_part *part = chain->parts[chain->num_attached--];

		if (part->ops->detach)
			part->ops->detach(part, chain->fe);
	}

	dvb_frontend_detach(chain->fe);
	chain->fe = NULL;
}

static void dvb_fe_chain_release(void *data)
{
	struct dvb_fe_chain *chain = data;

	dvb_fe_chain_detach(chain);
	dvb_fe_chain_unclaim(chain);
	kfree(chain);
}

struct dvb_frontend *devm_dvb_frontend_get(struct device *dev,
					   struct fwnode_handle *endpoint)
{
	struct dvb_fe_chain *chain;
	struct dvb_frontend *fe;
	int ret;

	chain = kzalloc_obj(*chain);
	if (!chain)
		return ERR_PTR(-ENOMEM);

	chain->consumer = dev;

	mutex_lock(&dvb_fe_parts_lock);
	ret = dvb_fe_chain_collect(chain, endpoint);
	if (!ret)
		ret = dvb_fe_chain_claim(chain);
	mutex_unlock(&dvb_fe_parts_lock);
	if (ret)
		goto err_unclaim;

	fe = chain->parts[0]->ops->get_frontend(chain->parts[0]);
	if (IS_ERR_OR_NULL(fe)) {
		ret = fe ? PTR_ERR(fe) : -ENODEV;
		dev_err(dev, "no frontend from %s: %d\n",
			dev_name(chain->parts[0]->dev), ret);
		goto err_unclaim;
	}
	chain->fe = fe;

	while (chain->num_attached + 1 < chain->num_parts) {
		struct dvb_fe_part *part = chain->parts[chain->num_attached + 1];

		ret = part->ops->attach(part, fe);
		if (ret) {
			dev_err(dev, "attaching %s failed: %d\n",
				dev_name(part->dev), ret);
			goto err_detach;
		}
		chain->num_attached++;
	}

	ret = devm_add_action_or_reset(dev, dvb_fe_chain_release, chain);
	if (ret)
		return ERR_PTR(ret);

	return fe;

err_detach:
	dvb_fe_chain_detach(chain);
err_unclaim:
	dvb_fe_chain_unclaim(chain);
	kfree(chain);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(devm_dvb_frontend_get);

static int dvb_fwnode_ts_polarity(struct fwnode_handle *fwnode,
				  const char *propname, bool *active_low)
{
	u32 val;

	*active_low = false;

	if (fwnode_property_read_u32(fwnode, propname, &val))
		return 0;
	if (val > 1)
		return -EINVAL;

	*active_low = !val;

	return 0;
}

int dvb_fwnode_ts_endpoint_parse(struct fwnode_handle *fwnode,
				 struct dvb_ts_endpoint *ts)
{
	u32 val;
	int ret;

	memset(ts, 0, sizeof(*ts));

	ret = fwnode_graph_parse_endpoint(fwnode, &ts->base);
	if (ret)
		return ret;

	if (fwnode_property_read_u32(fwnode, "bus-width", &ts->bus_width) ||
	    (ts->bus_width != 1 && ts->bus_width != 8))
		return -EINVAL;

	if (!fwnode_property_read_u32(fwnode, "data-shift", &val)) {
		/* a serial bus uses one of the lines, a parallel bus all */
		if (val > 7 || (ts->bus_width == 8 && val))
			return -EINVAL;
		ts->data_shift = val;
	}

	if (!fwnode_property_read_u32(fwnode, "pclk-sample", &val)) {
		if (val > 1)
			return -EINVAL;
		ts->clk_edge = val ? DVB_TS_CLK_EDGE_RISING :
				     DVB_TS_CLK_EDGE_FALLING;
	}

	ts->clk_gapped = fwnode_property_read_bool(fwnode, "ts-clock-gapped");

	ret = dvb_fwnode_ts_polarity(fwnode, "data-enable-active",
				     &ts->valid_active_low);
	if (!ret)
		ret = dvb_fwnode_ts_polarity(fwnode, "ts-sync-active",
					     &ts->sync_active_low);
	if (!ret)
		ret = dvb_fwnode_ts_polarity(fwnode, "ts-error-active",
					     &ts->error_active_low);

	return ret;
}
EXPORT_SYMBOL_GPL(dvb_fwnode_ts_endpoint_parse);
