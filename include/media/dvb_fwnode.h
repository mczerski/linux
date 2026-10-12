/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * DVB frontends described by firmware nodes
 */

#ifndef _DVB_FWNODE_H_
#define _DVB_FWNODE_H_

#include <linux/fwnode.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/types.h>

struct device;
struct dvb_frontend;
struct dvb_fe_part;

/**
 * DOC: Frontends described by firmware nodes
 *
 * On boards described by device tree or software nodes, the chips of a DVB
 * frontend are devices on their control bus, and the signal path between
 * them is a firmware graph:
 *
 * - the TS output port of a demodulator is linked to a TS input of a TS
 *   receiver, for example the demux of a SoC;
 * - the signal input port of the demodulator is linked to the output port
 *   of a tuner, whose input may in turn be linked to another part.
 *
 * Each chip driver registers a &struct dvb_fe_part with
 * devm_dvb_fe_part_register(). The TS receiver driver calls
 * devm_dvb_frontend_get() with its endpoint of a TS link. It gets the
 * &struct dvb_frontend of the demodulator with the tuner and the other
 * parts attached, ready for dvb_register_frontend().
 *
 * Port 0 (%DVB_FE_PORT_IN) is the signal input and port 1
 * (%DVB_FE_PORT_OUT) the output of a part. Chips with several instances use
 * other port numbers, given by their bindings.
 */

#define DVB_FE_PORT_IN		0
#define DVB_FE_PORT_OUT		1
#define DVB_FE_PORT_NONE	U32_MAX

/**
 * struct dvb_fe_part_ops - operations of a frontend part
 * @get_frontend: demodulators: return the frontend provided on the output
 *	port, or an ERR_PTR(). Called each time a TS receiver gets the
 *	frontend. When the receiver is done, dvb_frontend_detach() is called
 *	on the frontend.
 * @attach: tuners, SEC and LNB controllers and other parts: hook into @fe,
 *	for example fill fe->ops.tuner_ops and fe->tuner_priv.
 * @detach: undo @attach before dvb_frontend_detach() is called. Parts whose
 *	cleanup is done by the release callbacks they installed in the
 *	frontend, such as drivers attached with dvb_attach(), leave it NULL.
 *
 * A demodulator implements @get_frontend, every other part @attach.
 */
struct dvb_fe_part_ops {
	struct dvb_frontend *(*get_frontend)(struct dvb_fe_part *part);
	int (*attach)(struct dvb_fe_part *part, struct dvb_frontend *fe);
	void (*detach)(struct dvb_fe_part *part, struct dvb_frontend *fe);
};

/**
 * struct dvb_fe_part - a chip, or one instance in a chip, of a DVB frontend
 * @dev: device of the part; its firmware node holds the graph ports
 * @ops: operations
 * @in_port: port with the RF or IF signal input, or %DVB_FE_PORT_NONE
 * @out_port: port with the output: TS for demodulators, IF or RF for the
 *	other parts
 */
struct dvb_fe_part {
	struct device *dev;
	const struct dvb_fe_part_ops *ops;
	u32 in_port;
	u32 out_port;

	/* private: */
	struct list_head list;
	bool busy;
};

/**
 * devm_dvb_fe_part_register() - make a frontend part available
 * @part: the part, usually embedded in the private data of the driver
 *
 * Register the part as the last step of the probe function: a TS receiver
 * can use it as soon as it is registered. The part is unregistered when the
 * driver of @part->dev is unbound.
 *
 * Return: 0 on success or a negative error code.
 */
int devm_dvb_fe_part_register(struct dvb_fe_part *part);

/**
 * devm_dvb_frontend_get() - get the frontend connected to a TS input
 * @dev: device of the TS receiver
 * @endpoint: endpoint of the receiver on the TS link
 *
 * Find the demodulator at the other end of the link and the parts connected
 * to its input port, and to their input ports, attach those parts to the
 * frontend of the demodulator and link @dev to their devices with managed
 * device links: unbinding the driver of a part first unbinds the driver of
 * @dev. The modules of the parts stay loaded while the frontend is used.
 *
 * The frontend must be unregistered with dvb_unregister_frontend() before
 * the device resources of @dev are released, in the remove callback or by
 * a device resource action added after this call. The release detaches the
 * parts and calls dvb_frontend_detach().
 *
 * Device links added before a failure stay until the driver of @dev is
 * unbound or its probe fails.
 *
 * Return: the frontend, or an ERR_PTR(): -ENODEV if the demodulator is
 * disabled, -ENOLINK if @endpoint is not connected, -EPROBE_DEFER while a
 * part is not registered, -EBUSY if a part is used for another TS input, or
 * another negative error code.
 */
struct dvb_frontend *devm_dvb_frontend_get(struct device *dev,
					   struct fwnode_handle *endpoint);

/**
 * enum dvb_ts_clk_edge - edge of the TS clock at which data is sampled
 * @DVB_TS_CLK_EDGE_UNSPECIFIED: not given in the firmware node
 * @DVB_TS_CLK_EDGE_RISING: data is sampled at the rising edge
 * @DVB_TS_CLK_EDGE_FALLING: data is sampled at the falling edge
 */
enum dvb_ts_clk_edge {
	DVB_TS_CLK_EDGE_UNSPECIFIED,
	DVB_TS_CLK_EDGE_RISING,
	DVB_TS_CLK_EDGE_FALLING,
};

/**
 * struct dvb_ts_endpoint - one end of an MPEG-TS link
 * @base: port and endpoint numbers
 * @bus_width: 1 for a serial bus, 8 for a parallel bus ("bus-width")
 * @data_shift: serial bus: data line carrying the stream, 0 to 7
 *	("data-shift")
 * @clk_edge: clock edge at which data is sampled ("pclk-sample")
 * @clk_gapped: the clock only runs while data is valid ("ts-clock-gapped")
 * @valid_active_low: VALID is active low ("data-enable-active" = 0)
 * @sync_active_low: SYNC, the start of a packet, is active low
 *	("ts-sync-active" = 0)
 * @error_active_low: ERROR is active low ("ts-error-active" = 0)
 */
struct dvb_ts_endpoint {
	struct fwnode_endpoint base;
	unsigned int bus_width;
	unsigned int data_shift;
	enum dvb_ts_clk_edge clk_edge;
	bool clk_gapped;
	bool valid_active_low;
	bool sync_active_low;
	bool error_active_low;
};

/**
 * dvb_fwnode_ts_endpoint_parse() - parse the TS bus properties of an endpoint
 * @fwnode: the endpoint
 * @ts: the parsed properties
 *
 * Return: 0 on success, -EINVAL if "bus-width" is missing or a property has
 * an invalid value, or another negative error code.
 */
int dvb_fwnode_ts_endpoint_parse(struct fwnode_handle *fwnode,
				 struct dvb_ts_endpoint *ts);

#endif /* _DVB_FWNODE_H_ */
