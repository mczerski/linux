// SPDX-License-Identifier: GPL-2.0-only
/*
 * Amlogic DVB TS input and S2P initialization
 *
 * Copyright (C) 2026 Kağan Kadioğlu <kagankadioglutk@hotmail.com>
 */

#include <linux/pinctrl/consumer.h>
#include "amlogic_dvb.h"
#include "amlogic-dvb-regs.h"

static int aml_ts_input_init(struct aml_dvb *dvb, int idx)
{
	struct device_node *tsin_np;
	char tsin_name[32];
	u32 ts_serial = 0;
	u32 ts_control = 0; /* DTS ts<n>_control → FEC_INPUT_CONTROL[11:0] */
	u32 ctrl;
	int ret;

	snprintf(tsin_name, sizeof(tsin_name), "tsin@%d", idx);
	tsin_np = of_get_child_by_name(dvb->dev->of_node, tsin_name);
	if (tsin_np && of_device_is_available(tsin_np)) {
		of_property_read_u32(tsin_np, "ts-control", &ts_control);
		of_property_read_u32(tsin_np, "ts-serial", &ts_serial);
	}
	of_node_put(tsin_np);

	ctrl = ts_serial ? (TS_IN_ENABLE | TS_IN_SERIAL) : TS_IN_CTRL_PARALLEL;

	dev_info(dvb->dev, "ts_input_init: initializing TS input %d\n", idx);

	/*
     * TS_IN_CTRL address verified with vendor devmem:
     *   TS_IN_CTRL(0) → regmap_ts offset 0x000 → phys 0xFFD06000 = 0x00030003
     *   TS_IN_CTRL(1) → regmap_ts offset 0x140 → phys 0xFFD06140 = 0x00030003
     * regmap_ts ≠ regmap_demux (0xFF638000) — no stride conflict.
     */
	ret = aml_write_reg(dvb, TS_IN_CTRL(idx), TS_IN_RESET);
	if (ret) {
		dev_err(dvb->dev,
			"ts_input_init: failed to reset TS input %d: %d\n", idx,
			ret);
		return ret;
	}
	udelay(10);
	ret = aml_write_reg(dvb, TS_IN_CTRL(idx), ctrl);
	if (ret) {
		dev_err(dvb->dev,
			"ts_input_init: failed to enable TS input %d: %d\n",
			idx, ret);
		return ret;
	}

	dvb->ts[idx].is_serial = (bool)ts_serial;
	dvb->ts[idx].mode = ts_serial ? TS_IN_SERIAL : TS_IN_PARALLEL;
	/* DTS ts<n>_control, max 12-bit */
	dvb->ts[idx].fec_ctrl = ts_control & 0xFFF;
	dvb->ts[idx].enabled = true;
	dev_info(
		dvb->dev,
		"ts_input_init: TS input %d initialized OK (mode=0x%x fec_ctrl=0x%03x)\n",
		idx, ctrl, dvb->ts[idx].fec_ctrl);
	return 0;
}

static int aml_s2p_init(struct aml_dvb *dvb, int idx)
{
	/* vendor verified: 0xFFD06040 = 0x0000CCCC
     * CLK_DIV=0xCC, CLK_INVERT+DATA_INVERT set, BIT0(ENABLE)=0 (bypassed in parallel mode)
     */
	u32 ctrl = TS_S2P_VENDOR_INIT;
	struct device_node *np = dvb->dev->of_node;
	struct device_node *child;
	int ret;

	dev_info(dvb->dev, "s2p_init: starting S2P %d\n", idx);

	/* vendor value fixed at 0x0000CCCC — no DTS override */
	(void)np;
	(void)child;

	/* Reset the hardware */
	ret = aml_write_reg(dvb, TS_S2P_CTRL(idx), TS_S2P_RESET);
	if (ret)
		return ret;
	udelay(10);

	/* Write the final CTRL value (target: 0x401 or 0xC01) */
	dev_info(dvb->dev, "s2p_init: writing S2P %d ctrl=0x%x\n", idx, ctrl);
	ret = aml_write_reg(dvb, TS_S2P_CTRL(idx), ctrl);

	dvb->s2p[idx].enabled = true;
	return ret;
}

int aml_ts_hw_init(struct aml_dvb *dvb)
{
	int i, ret;

	dev_info(dvb->dev, "ts_hw_init: starting\n");

	/*
     * Clear STB_TOP_CONFIG: reset all DEMUX_X_INPUT_SOURCE fields.
     * Hardware reset value is undefined — vendor driver writes on every init.
     * aml_dvb_apply_ts_source() will write the correct value afterwards.
     */
	ret = aml_write_reg(dvb, STB_TOP_CONFIG, 0x00000000);
	if (ret)
		dev_warn(dvb->dev,
			 "ts_hw_init: STB_TOP_CONFIG clear failed: %d\n", ret);

	/* Initialise TS inputs */
	for (i = 0; i < dvb->caps.num_ts_inputs; i++) {
		dev_info(dvb->dev, "ts_hw_init: initializing TS input %d/%d\n",
			 i, dvb->caps.num_ts_inputs);
		ret = aml_ts_input_init(dvb, i);
		if (ret) {
			dev_err(dvb->dev,
				"ts_hw_init: aml_ts_input_init(%d) failed: %d\n",
				i, ret);
			goto err;
		}
	}

	/* Initialise S2P converters */
	for (i = 0; i < dvb->caps.num_s2p; i++) {
		dev_info(dvb->dev, "ts_hw_init: initializing S2P %d/%d\n", i,
			 dvb->caps.num_s2p);
		ret = aml_s2p_init(dvb, i);
		if (ret) {
			dev_err(dvb->dev,
				"ts_hw_init: aml_s2p_init(%d) failed: %d\n", i,
				ret);
			goto err;
		}
	}

	/* Configure the TS_TOP_CONFIG register */
	/* vendor verified (CoreELEC devmem 0xFFD063C4 = 0x7700BB47):
     * bit[31:24]=0x77 framing enable bitleri, bit[15:8]=pkt_len-1, bit[7:0]=sync_byte
     */
	u32 ts_top_val = (0x77u << 24) | ((dvb->ts_packet_len - 1) << 8) |
			 dvb->ts_sync_byte;
	dev_info(
		dvb->dev,
		"ts_hw_init: writing TS_TOP_CONFIG = 0x%x (sync_byte=0x%x, packet_len=%d)\n",
		ts_top_val, dvb->ts_sync_byte, dvb->ts_packet_len);
	ret = aml_write_reg(dvb, TS_TOP_CONFIG, ts_top_val);
	if (ret) {
		dev_err(dvb->dev,
			"ts_hw_init: failed to write TS_TOP_CONFIG: %d\n", ret);
		goto err;
	}

	dev_info(dvb->dev, "ts_hw_init: completed successfully\n");
	return 0;

err:
	aml_ts_hw_release(dvb);
	dev_err(dvb->dev, "ts_hw_init: failed with error %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(aml_ts_hw_init);

void aml_ts_hw_release(struct aml_dvb *dvb)
{
	int i;

	dev_info(dvb->dev, "ts_hw_release: starting\n");

	aml_write_reg(dvb, TS_TOP_CONFIG, 0);

	for (i = 0; i < dvb->caps.num_s2p; i++) {
		if (dvb->s2p[i].enabled) {
			dev_dbg(dvb->dev, "ts_hw_release: disabling S2P %d\n",
				i);
			aml_write_reg(dvb, TS_S2P_CTRL(i), 0);
			dvb->s2p[i].enabled = false;
		}
	}

	for (i = 0; i < dvb->caps.num_ts_inputs; i++) {
		if (dvb->ts[i].enabled) {
			dev_dbg(dvb->dev,
				"ts_hw_release: disabling TS input %d\n", i);
			aml_write_reg(dvb, TS_IN_CTRL(i), 0);
			dvb->ts[i].enabled = false;
		}
	}
	dev_info(dvb->dev, "ts_hw_release: done\n");
}
EXPORT_SYMBOL_GPL(aml_ts_hw_release);
