// SPDX-License-Identifier: GPL-2.0-only
/*
 * Amlogic DVB/TS hardware demultiplexer driver - Core
 *
 * Copyright (C) 2026 Kağan Kadioğlu <kagankadioglutk@hotmail.com>
 */

/*
 * Change History:
 *   V1.00 - Initial mainline port (DVB-S/S2/C/T2, SM1 support)
 *   V1.10 - TS port mismatch fix (ts-source: demux index / tsin distinction)
 *   V1.20 - Skip disabled tsin DTS nodes with for_each_child
 *   V1.30 - R848 I2C address: 0x3d→0x7a (AVL6862 repeater 8-bit format)
 *   V1.40 - power-gpios: &gpio→&gpio_ao (GPIOE_2 aobus offset 14)
 *   V1.50 - ts-source/ts-port distinction: TVH dmx0, physical port tsin_b
 *   V2.00 - regs.h: frontend.c pm_runtime/asyncfifo_set_source/set_ts_rate
 *           ts.c: s2p_init DTS node scan (amlogic,clkinv/sopinv)
 *   V2.01 - regs.h: AML_DEMUX_REG stride 0x200→0x140 (datasheet: core delta=0x140)
 *           regs.h: DEMUX_CONTROL offset 0x00→0x04 (0x00=STB_VERSION_O RO!)
 *           regs.h: TS_IN_CTRL/TS_S2P_CTRL FIXME warning (address conflict)
 *           ts.c: TS_IN_CTRL write placed under guard (address unverified)
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/netdevice.h>
#include <linux/u64_stats_sync.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/timer.h>

#include <media/dmxdev.h>
#include <media/dvbdev.h>
#include <media/dvb_demux.h>

#include "amlogic_dvb.h"
#include "amlogic-dvb-regs.h"
#include "amlogic-dvb-soc.h"
#include "amlogic-dvb-hwops.h"
#include <linux/proc_fs.h>

#ifndef MAX_DELSYS
#define MAX_DELSYS 8
#endif
#include <linux/seq_file.h>
// compat.h removed — dvb_register_adapter used directly

static short adapter_nr[AML_MAX_DEMUX] = { -1, -1, -1 };
module_param_array(adapter_nr, short, NULL, 0444);
MODULE_PARM_DESC(adapter_nr, "DVB adapter numbers");

u32 aml_dvb_debug = 0;
EXPORT_SYMBOL_GPL(aml_dvb_debug);
module_param_named(debug, aml_dvb_debug, uint, 0644);
MODULE_PARM_DESC(debug, "Debug level (0=off, 1=info, 2=verbose)");

#define dvb_dbg(dvb, level, fmt, ...) \
    do { \
        if (READ_ONCE(aml_dvb_debug) >= level) \
            dev_dbg((dvb)->dev, fmt, ##__VA_ARGS__); \
    } while (0)

/* ---------------------------------------------------------------------- */
/* Default hardware operations */
/* ---------------------------------------------------------------------- */
static void aml_default_reset_fifo(struct aml_dvb *dvb, int id)
{
	/* IDLE state — BIT29=1, BIT31=0 (dev/mem: inactive=0x20000000) */
	aml_write_async(dvb, ASYNC_FIFO_CTRL(id), ASYNC_FIFO_IDLE);
}

static void aml_default_enable_ts_input(struct aml_dvb *dvb, int idx)
{
	/* ts[idx].mode should have been set by aml_ts_input_init() reading
     * from DTS. If init has not been done yet, default is parallel. */
	u32 mode = dvb->ts[idx].mode ? TS_IN_SERIAL : TS_IN_PARALLEL;
	aml_write_reg(dvb, TS_IN_CTRL(idx), TS_IN_ENABLE | mode);
}

static void aml_default_disable_ts_input(struct aml_dvb *dvb, int idx)
{
	aml_write_reg(dvb, TS_IN_CTRL(idx), 0);
}

static void aml_default_set_ts_rate(struct aml_dvb *dvb, u32 rate_kbps)
{
	u32 div;
	u32 clk_val;

	if (!dvb->hwops->set_ts_rate)
		return;

	div = 27000 / rate_kbps;
	if (div < 1)
		div = 1;
	if (div > 255)
		div = 255;

	clk_val = TS_CLK_ENABLE | (div & TS_CLK_DIV_MASK);
	aml_write_reg(dvb, HHI_TS_CLK_CNTL, clk_val);

	aml_write_reg(dvb, TS_RATE_CTRL,
		      TS_RATE_ENABLE | (rate_kbps & TS_RATE_MASK));
}

static int aml_default_get_irq_status(struct aml_dvb *dvb, int demux_id,
				      u32 *status)
{
	return aml_read_reg(dvb, STB_INT_STATUS(demux_id), status);
}

static const struct aml_dvb_hw_ops aml_default_hwops = {
	.reset_fifo = aml_default_reset_fifo,
	.enable_ts_input = aml_default_enable_ts_input,
	.disable_ts_input = aml_default_disable_ts_input,
	.set_ts_rate = aml_default_set_ts_rate,
	.get_irq_status = aml_default_get_irq_status,
};

/* ---------------------------------------------------------------------- */
/* Hardware capability detection */
/* ---------------------------------------------------------------------- */
static void aml_dvb_detect_capabilities(struct aml_dvb *dvb)
{
	struct device *dev = dvb->dev;
	struct aml_dvb_hw_caps *caps = &dvb->caps;

	caps->soc_name = "Unknown";
	caps->num_demux = 3;
	caps->num_ts_inputs = 2;
	caps->num_s2p = 3;
	caps->num_pid_filters = 32;
	caps->num_sec_filters = 32;
	caps->has_descrambler = true;
	caps->has_hwfilter = true;
	caps->has_dma = true;
	caps->max_bitrate = 120;
	caps->num_dsc = 2;

	if (of_device_is_compatible(dev->of_node, "amlogic,sm1-dvb")) {
		caps->soc_name = "SM1 (S905X3)";
		caps->has_ciplus = true;
		caps->max_bitrate = 192;
		/* S905X3 memory map (XLS verified):
         *   async_fifo  = 0xFFD0A000  ✓
         *   async_fifo2 = 0xFFD09000  ✓
         *   async_fifo3 = 0xFFD26000  S905D3-specific, NOT present on S905X3
         * DTS: 0xffd09000 0x2000 — sadece 2 blok mapped
         */
		caps->num_asyncfifo = 2;
	} else if (of_device_is_compatible(dev->of_node, "amlogic,g12b-dvb")) {
		caps->soc_name = "G12B (S922X)";
		caps->has_ciplus = true;
		caps->max_bitrate = 192;
		caps->num_asyncfifo = 2;
	} else if (of_device_is_compatible(dev->of_node, "amlogic,g12a-dvb")) {
		caps->soc_name = "G12A (S905X2)";
		caps->has_ciplus = false;
		caps->max_bitrate = 168;
		caps->num_asyncfifo = 2;
	} else if (of_device_is_compatible(dev->of_node, "amlogic,gxm-dvb")) {
		caps->soc_name = "GXM (S912)";
		caps->has_ciplus = false;
		caps->max_bitrate = 168;
		caps->num_asyncfifo = 2;
	} else if (of_device_is_compatible(dev->of_node, "amlogic,gxl-dvb")) {
		caps->soc_name = "GXL (S905X)";
		caps->has_ciplus = false;
		caps->max_bitrate = 144;
		caps->num_asyncfifo = 2;
	} else if (of_device_is_compatible(dev->of_node, "amlogic,gxbb-dvb")) {
		caps->soc_name = "GXBB (S905)";
		caps->has_ciplus = false;
		caps->max_bitrate = 144;
		caps->num_asyncfifo = 2;
	}

	dev_info(
		dev,
		"Detected %s: %d demux, %d TS inputs, %d PID filters, %d async FIFO, %d Mbps max\n",
		caps->soc_name, caps->num_demux, caps->num_ts_inputs,
		caps->num_pid_filters, caps->num_asyncfifo, caps->max_bitrate);

	if (caps->has_ciplus)
		dev_info(dev, "CI+ support available\n");
}

/* ---------------------------------------------------------------------- */
/* Reset framework support */
/* ---------------------------------------------------------------------- */
static int aml_dvb_init_reset(struct aml_dvb *dvb)
{
	struct device *dev = dvb->dev;
	int ret;

	dvb->rst_demux = devm_reset_control_get_optional(dev, "demux");
	if (IS_ERR(dvb->rst_demux)) {
		if (PTR_ERR(dvb->rst_demux) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dev_info(dev, "Reset control not available\n");
		dvb->rst_demux = NULL;
		return 0;
	}

	if (dvb->rst_demux) {
		dev_info(dev, "Performing hardware reset\n");
		ret = reset_control_assert(dvb->rst_demux);
		if (ret)
			dev_warn(dev, "Failed to assert reset: %d\n", ret);
		usleep_range(1000, 2000);
		ret = reset_control_deassert(dvb->rst_demux);
		if (ret)
			dev_warn(dev, "Failed to deassert reset: %d\n", ret);
		usleep_range(1000, 2000);
		dev_info(dev, "Hardware reset completed\n");
	}

	dvb->rst_asyncfifo = devm_reset_control_get_optional(dev, "asyncfifo");
	if (IS_ERR(dvb->rst_asyncfifo)) {
		if (PTR_ERR(dvb->rst_asyncfifo) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dvb->rst_asyncfifo = NULL;
	}

	return 0;
}

/* ---------------------------------------------------------------------- */
/* Multiple register region support */
/* ---------------------------------------------------------------------- */
static int aml_dvb_map_registers(struct aml_dvb *dvb)
{
	struct platform_device *pdev = to_platform_device(dvb->dev);
	struct resource *res;
	void __iomem *base;

	/*
     * "ts" region: 0xffd06000 — TS_IN ports, S2P, TOP_CONFIG
     * Verified with dev/mem: packet counter (0xffd0611C) is changing
     */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ts");
	if (!res) {
		/* Legacy DTS compatibility: if only a single "demux" entry exists, use it as "ts" */
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "demux");
		if (!res)
			res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res) {
			dev_err(dvb->dev, "Failed to get TS memory resource\n");
			return -ENODEV;
		}
		dev_warn(
			dvb->dev,
			"Using legacy single-region DTS — demux core NOT mapped!\n");
	}
	/* devm_ioremap: map without claiming region (vdec sharing compatibility) */
	base = devm_ioremap(dvb->dev, res->start, resource_size(res));
	if (!base)
		return -ENOMEM;
	dvb->base_ts = base;
	dev_info(dvb->dev, "TS registers: %pR\n", res);

	/*
     * "demux" region: 0xff638000 — DEMUX core (PID filter, section filter)
     * dev/mem: DEMUX_CONTROL idx 0x04 = 0x009FF8EF, STB_VERSION idx 0x00 = 0x00FFFFFF
     *
     * devm_ioremap() is used (not devm_ioremap_resource()):
     *   The meson_vdec driver already claims this region with request_mem_region.
     *   devm_ioremap_resource() would attempt a second claim and return EBUSY.
     *   devm_ioremap() only maps, does not claim — sharing is safe.
     */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "demux");
	if (res) {
		base = devm_ioremap(dvb->dev, res->start, resource_size(res));
		if (!base) {
			dev_err(dvb->dev, "Failed to map demux core region\n");
			return -ENOMEM;
		}
		dvb->base_demux = base;
		dvb->base_asyncfifo = base;
		dev_info(dvb->dev,
			 "Demux core registers: %pR (shared ioremap)\n", res);
	} else {
		/* Expected for SM1 (S905X3): "demux" region is absent from DTS.
         * Proven with devmem2: 0xff638000 = AXI bus noise.
         * On SM1 all DMX registers are in the "ts" region (0xffd06000).
         * base_demux → base_ts redirection is CORRECT. */
		dev_info(
			dvb->dev,
			"No 'demux' region — SM1: using 'ts' base (0xffd06000) for DMX registers\n");
		dvb->base_demux = dvb->base_ts;
	}

	/*
     * "async-fifo" region: 0xFFD09000 — async_fifo2 + async_fifo DMA blocks
     * S905X3: async_fifo2=0xFFD09000, async_fifo=0xFFD0A000 (two separate 4KB blocks)
     * DTS: reg = <0x0 0xffd09000 0x0 0x2000>
     * devm_ioremap: this region may also conflict with other drivers
     */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "async-fifo");
	if (res) {
		base = devm_ioremap(dvb->dev, res->start, resource_size(res));
		if (!base) {
			dev_err(dvb->dev, "Failed to map async-fifo region\n");
			return -ENOMEM;
		}
		dvb->base_asyncfifo = base;
		dev_info(dvb->dev, "Async FIFO registers: %pR\n", res);
	}
	/* If base_asyncfifo is absent, the fallback above (base_demux or base_ts) applies */

	return 0;
}

static int aml_dvb_init_regmaps(struct aml_dvb *dvb)
{
	/* regmap_ts: TS_IN ports and TOP_CONFIG (0xffd06000) */
	dvb->regmap_ts = devm_regmap_init_mmio(dvb->dev, dvb->base_ts,
					       &aml_demux_regmap_config);
	if (IS_ERR(dvb->regmap_ts)) {
		dev_err(dvb->dev, "Failed to init TS regmap: %ld\n",
			PTR_ERR(dvb->regmap_ts));
		return PTR_ERR(dvb->regmap_ts);
	}

	/* regmap_demux: DEMUX core + ASYNC FIFO (0xff638000) */
	dvb->regmap_demux = devm_regmap_init_mmio(dvb->dev, dvb->base_demux,
						  &aml_demux_regmap_config);
	if (IS_ERR(dvb->regmap_demux)) {
		dev_err(dvb->dev, "Failed to init demux regmap: %ld\n",
			PTR_ERR(dvb->regmap_demux));
		return PTR_ERR(dvb->regmap_demux);
	}

	/* ASYNC FIFO — separate physical block (0xFFD09000), its own regmap
     *
     * DTS: reg = <0x0 0xffd09000 0x0 0x2000>
     *   FIFO[1] @ offset 0x0000 (async_fifo2 = 0xFFD09000)
     *   FIFO[0] @ offset 0x1000 (async_fifo  = 0xFFD0A000)
     * max_register = 0x1FFC (aml_async_regmap_config)
     */
	if (dvb->base_asyncfifo && dvb->base_asyncfifo != dvb->base_demux &&
	    dvb->base_asyncfifo != dvb->base_ts) {
		dvb->regmap_async =
			devm_regmap_init_mmio(dvb->dev, dvb->base_asyncfifo,
					      &aml_async_regmap_config);
		if (IS_ERR(dvb->regmap_async)) {
			dev_warn(
				dvb->dev,
				"Failed to init async-fifo regmap, falling back to demux\n");
			dvb->regmap_async = dvb->regmap_demux;
		}
	} else {
		/* async-fifo region not defined in DTS — share demux regmap */
		dvb->regmap_async = dvb->regmap_demux;
	}

	return 0;
}

/* ---------------------------------------------------------------------- */
/* Pinctrl initialization */
/* ---------------------------------------------------------------------- */
static int aml_pinctrl_init(struct aml_dvb *dvb)
{
	struct device *dev = dvb->dev;
	struct pinctrl *pctrl;

	pctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pctrl)) {
		dev_err(dev, "Failed to setup pinctrl\n");
		return PTR_ERR(pctrl);
	}

	return 0;
}

/* ---------------------------------------------------------------------- */
/* Clock and reset */
/* ---------------------------------------------------------------------- */
int aml_dvb_clk_enable(struct aml_dvb *dvb)
{
	int ret;

	ret = clk_prepare_enable(dvb->clk_demux);
	if (ret)
		return ret;

	if (dvb->clk_asyncfifo) {
		ret = clk_prepare_enable(dvb->clk_asyncfifo);
		if (ret) {
			clk_disable_unprepare(dvb->clk_demux);
			return ret;
		}
	}

	if (dvb->clk_parser) {
		ret = clk_prepare_enable(dvb->clk_parser);
		if (ret) {
			if (dvb->clk_asyncfifo)
				clk_disable_unprepare(dvb->clk_asyncfifo);
			clk_disable_unprepare(dvb->clk_demux);
			return ret;
		}
	}

	if (dvb->clk_ahbarb0) {
		ret = clk_prepare_enable(dvb->clk_ahbarb0);
		if (ret) {
			if (dvb->clk_parser)
				clk_disable_unprepare(dvb->clk_parser);
			if (dvb->clk_asyncfifo)
				clk_disable_unprepare(dvb->clk_asyncfifo);
			clk_disable_unprepare(dvb->clk_demux);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(aml_dvb_clk_enable);

void aml_dvb_clk_disable(struct aml_dvb *dvb)
{
	if (dvb->clk_ahbarb0)
		clk_disable_unprepare(dvb->clk_ahbarb0);
	if (dvb->clk_parser)
		clk_disable_unprepare(dvb->clk_parser);
	if (dvb->clk_asyncfifo)
		clk_disable_unprepare(dvb->clk_asyncfifo);
	clk_disable_unprepare(dvb->clk_demux);
}
EXPORT_SYMBOL_GPL(aml_dvb_clk_disable);

void aml_dvb_hw_reset(struct aml_dvb *dvb)
{
	if (dvb->rst_demux)
		reset_control_reset(dvb->rst_demux);
	if (dvb->rst_asyncfifo)
		reset_control_reset(dvb->rst_asyncfifo);
}
EXPORT_SYMBOL_GPL(aml_dvb_hw_reset);

/* ---------------------------------------------------------------------- */
/* Runtime PM */
/* ---------------------------------------------------------------------- */
int aml_dvb_runtime_suspend(struct device *dev)
{
	struct aml_dvb *dvb = dev_get_drvdata(dev);

	/* S905X3 demux has its own power domain (SEPARATE from vdec).
     * AO_RTI_GEN_PWR_SLEEP0 (0xC81000E8) is inaccessible — AO region
     * is not in our regmap, writing would go to the wrong address.
     * The kernel power-domain framework already manages this. */
	dev_info(dev, "runtime_suspend: disabling clocks\n");
	aml_dvb_clk_disable(dvb);
	return 0;
}
EXPORT_SYMBOL_GPL(aml_dvb_runtime_suspend);

int aml_dvb_runtime_resume(struct device *dev)
{
	struct aml_dvb *dvb = dev_get_drvdata(dev);
	int ret;

	/* Power domain is managed by the kernel.
     * Just enable the clocks. */
	dev_info(dev, "runtime_resume: enabling clocks\n");
	ret = aml_dvb_clk_enable(dvb);
	if (ret) {
		dev_err(dev, "runtime_resume: clock enable failed: %d\n", ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(aml_dvb_runtime_resume);

/* ---------------------------------------------------------------------- */
/* Watchdog */
/* ---------------------------------------------------------------------- */
static void aml_watchdog_timer_func(struct timer_list *t)
{
	struct aml_dvb *dvb = container_of(t, struct aml_dvb, watchdog_timer);
	int i;

	for (i = 0; i < dvb->caps.num_demux; i++) {
		if (dvb->watchdog_disable[i])
			continue;
	}
	mod_timer(&dvb->watchdog_timer, jiffies + msecs_to_jiffies(1000));
}

void aml_watchdog_init(struct aml_dvb *dvb)
{
	timer_setup(&dvb->watchdog_timer, aml_watchdog_timer_func, 0);
	mod_timer(&dvb->watchdog_timer, jiffies + msecs_to_jiffies(1000));
}
EXPORT_SYMBOL_GPL(aml_watchdog_init);

void aml_watchdog_release(struct aml_dvb *dvb)
{
	del_timer_sync(&dvb->watchdog_timer);
}
EXPORT_SYMBOL_GPL(aml_watchdog_release);

/* ---------------------------------------------------------------------- */
/* Interrupt handling */
/* ---------------------------------------------------------------------- */
static irqreturn_t aml_dvb_irq_handler(int irq, void *dev_id)
{
	struct aml_dmx *dmx = dev_id;
	struct aml_dvb *dvb = dmx->dvb;

	u64_stats_update_begin(&dvb->stats_syncp);
	u64_stats_add(&dvb->stats_irq, 1);
	u64_stats_update_end(&dvb->stats_syncp);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t aml_dvb_irq_thread(int irq, void *dev_id)
{
	struct aml_dmx *dmx = dev_id;
	u32 status;

	aml_read_reg(dmx->dvb, STB_INT_STATUS(dmx->id), &status);
	if (!status)
		return IRQ_NONE;

	/* Vendor order: process FIRST, ACK AFTER */

	/* HW section filter — SEC_BUFF_READY */
	if (status & INT_SECTION_READY)
		aml_dmx_process_section(dmx);

	/* PCR/PTS */
	if (status & INT_NEW_PDTS)
		dmx->pcr = aml_dmx_get_pcr(dmx);

	/* TS error counter */
	if (status & INT_TS_ERROR) {
		u64_stats_update_begin(&dmx->stats.syncp);
		u64_stats_add(&dmx->stats.ts_errors, 1);
		u64_stats_update_end(&dmx->stats.syncp);
	}

	/* ACK — AFTER processing (vendor order) */
	aml_write_reg(dmx->dvb, STB_INT_STATUS(dmx->id), status);

	return IRQ_HANDLED;
}

static int aml_dvb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_dvb *dvb;
	int ret, i;

	dev_dbg(dev, "aml_dvb_probe: starting\n");

	dvb = devm_kzalloc(dev, sizeof(*dvb), GFP_KERNEL);
	if (!dvb)
		return -ENOMEM;

	dvb->dev = dev;
	dvb->pdev = pdev;
	platform_set_drvdata(pdev, dvb);

	dev_dbg(dev, "aml_dvb_probe: setting DMA mask\n");
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(40));
	if (ret) {
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
		if (ret) {
			dev_err(dev, "DMA mask setup failed: %d\n", ret);
			return dev_err_probe(dev, ret,
					     "DMA mask setup failed\n");
		}
	}

	dev_dbg(dev, "aml_dvb_probe: mapping registers\n");
	ret = aml_dvb_map_registers(dvb);
	if (ret) {
		dev_err(dev, "aml_dvb_map_registers failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "aml_dvb_probe: initializing regmaps\n");
	ret = aml_dvb_init_regmaps(dvb);
	if (ret) {
		dev_err(dev, "aml_dvb_init_regmaps failed: %d\n", ret);
		return ret;
	}

	dvb->hwops = &aml_default_hwops;

	spin_lock_init(&dvb->slock);
	u64_stats_init(&dvb->stats_syncp);
	atomic_set(&dvb->feed_count, 0);

	dev_dbg(dev, "aml_dvb_probe: detecting capabilities\n");
	aml_dvb_detect_capabilities(dvb);

	dev_dbg(dev, "aml_dvb_probe: initializing reset\n");
	ret = aml_dvb_init_reset(dvb);
	if (ret) {
		dev_err(dev, "aml_dvb_init_reset failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "aml_dvb_probe: initializing pinctrl\n");
	ret = aml_pinctrl_init(dvb);
	if (ret) {
		dev_err(dev, "aml_pinctrl_init failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "aml_dvb_probe: getting clocks\n");
	dvb->clk_demux = devm_clk_get_optional(dev, "demux");
	if (IS_ERR(dvb->clk_demux)) {
		ret = PTR_ERR(dvb->clk_demux);
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_warn(dev, "failed to get demux clock (ignoring): %d\n",
			 ret);
		dvb->clk_demux = NULL;
	}

	dvb->clk_asyncfifo = devm_clk_get_optional(dev, "asyncfifo");
	if (IS_ERR(dvb->clk_asyncfifo)) {
		ret = PTR_ERR(dvb->clk_asyncfifo);
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_warn(dev, "failed to get asyncfifo clock (ignoring): %d\n",
			 ret);
		dvb->clk_asyncfifo = NULL;
	}

	/* SM1 (>= G12A): vendor enables three clocks — parser_top and ahbarb0 required.
     * parser_top off → video cannot reach PES parser.
     * ahbarb0 off    → AHB DMA stops, section/PES cannot be transferred.
     * optional: no error if absent from DTS on older SoCs. */
	dvb->clk_parser = devm_clk_get_optional(dev, "parser_top");
	if (IS_ERR(dvb->clk_parser)) {
		ret = PTR_ERR(dvb->clk_parser);
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_warn(dev, "failed to get parser_top clock (ignoring): %d\n",
			 ret);
		dvb->clk_parser = NULL;
	}

	dvb->clk_ahbarb0 = devm_clk_get_optional(dev, "ahbarb0");
	if (IS_ERR(dvb->clk_ahbarb0)) {
		ret = PTR_ERR(dvb->clk_ahbarb0);
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_warn(dev, "failed to get ahbarb0 clock (ignoring): %d\n",
			 ret);
		dvb->clk_ahbarb0 = NULL;
	}

	dev_dbg(dev, "aml_dvb_probe: creating workqueue\n");
	dvb->ts_wq = alloc_workqueue(
		"aml_dvb_ts", WQ_UNBOUND | WQ_HIGHPRI | WQ_MEM_RECLAIM, 0);
	if (!dvb->ts_wq) {
		dev_err(dev, "failed to create workqueue\n");
		return -ENOMEM;
	}

	dev_dbg(dev, "aml_dvb_probe: enabling clocks\n");
	ret = aml_dvb_clk_enable(dvb);
	if (ret) {
		dev_err(dev, "aml_dvb_clk_enable failed: %d\n", ret);
		goto err_free_wq;
	}

	dev_dbg(dev, "aml_dvb_probe: hardware reset\n");
	aml_dvb_hw_reset(dvb);

	dev_dbg(dev, "aml_dvb_probe: registering DVB adapter\n");
	ret = dvb_register_adapter(&dvb->adapter, "Amlogic DVB", THIS_MODULE,
				   dev, adapter_nr);
	if (ret) {
		dev_err(dev, "failed to register DVB adapter: %d\n", ret);
		goto err_clk_disable;
	}
	dvb->adapter.priv = dvb;

	/* ========== Assign TS initial values ========== */
	dvb->ts_sync_byte = 0x47;
	dvb->ts_packet_len = 188;
	/* ================================================== */

	dev_dbg(dev, "aml_dvb_probe: initializing TS hardware\n");
	ret = aml_ts_hw_init(dvb);
	if (ret) {
		dev_err(dev, "aml_ts_hw_init failed: %d\n", ret);
		goto err_unregister_adapter;
	}

	dev_dbg(dev, "aml_dvb_probe: initializing async FIFO\n");
	ret = aml_asyncfifo_init(dvb);
	if (ret) {
		dev_err(dev, "aml_asyncfifo_init failed: %d\n", ret);
		goto err_ts_release;
	}

	dev_dbg(dev, "aml_dvb_probe: initializing descrambler\n");
	ret = aml_dsc_init_all(dvb);
	if (ret) {
		dev_err(dev, "aml_dsc_init_all failed: %d\n", ret);
		goto err_asyncfifo_release;
	}

	dev_dbg(dev, "aml_dvb_probe: initializing %d demux cores\n",
		dvb->caps.num_demux);
	for (i = 0; i < dvb->caps.num_demux; i++) {
		struct aml_dmx *dmx = &dvb->demux[i];

		dev_dbg(dev, "aml_dvb_probe: demux %d - setting up structure\n",
			i);
		dmx->dvb = dvb;
		dmx->id = i;
		dmx->base = dvb->base_demux + i * 0x140;
		spin_lock_init(&dmx->lock);
		spin_lock_init(&dmx->pid_lock);
		spin_lock_init(&dmx->sec_lock);
		bitmap_zero(dmx->pid_bitmap, AML_HW_PID_MAX);
		bitmap_zero(dmx->sec_bitmap, 32);
		u64_stats_init(&dmx->stats.syncp);

		dev_dbg(dev,
			"aml_dvb_probe: demux %d - calling aml_dmx_hw_init\n",
			i);
		ret = aml_dmx_hw_init(dmx);
		if (ret) {
			dev_err(dev,
				"aml_dmx_hw_init for demux %d failed: %d\n", i,
				ret);
			goto err_demux_release;
		}
		dev_dbg(dev, "aml_dvb_probe: demux %d - aml_dmx_hw_init OK\n",
			i);

		dev_dbg(dev,
			"aml_dvb_probe: demux %d - calling aml_dmx_dvb_init\n",
			i);
		ret = aml_dmx_dvb_init(dmx, &dvb->adapter);
		if (ret) {
			dev_err(dev,
				"aml_dmx_dvb_init for demux %d failed: %d\n", i,
				ret);
			aml_dmx_hw_release(dmx);
			goto err_demux_release;
		}
		dev_dbg(dev, "aml_dvb_probe: demux %d - aml_dmx_dvb_init OK\n",
			i);

		snprintf(dmx->irq_name, sizeof(dmx->irq_name), "demux%d", i);
		dev_dbg(dev,
			"aml_dvb_probe: demux %d - getting IRQ (name: %s)\n", i,
			dmx->irq_name);
		ret = platform_get_irq_byname_optional(pdev, dmx->irq_name);
		if (ret < 0) {
			ret = platform_get_irq(pdev, i);
			if (ret < 0) {
				dev_err(dev, "failed to get demux%d IRQ: %d\n",
					i, ret);
				aml_dmx_dvb_release(dmx);
				aml_dmx_hw_release(dmx);
				goto err_demux_release;
			}
		}
		dmx->irq = ret;
		dev_dbg(dev, "aml_dvb_probe: demux %d - got IRQ %d\n", i,
			dmx->irq);

		dev_dbg(dev,
			"aml_dvb_probe: demux %d - requesting threaded IRQ\n",
			i);
		ret = devm_request_threaded_irq(dev, dmx->irq,
						aml_dvb_irq_handler,
						aml_dvb_irq_thread, IRQF_SHARED,
						dmx->irq_name, dmx);
		if (ret) {
			dev_err(dev,
				"failed to request IRQ %d for demux %d: %d\n",
				dmx->irq, i, ret);
			aml_dmx_dvb_release(dmx);
			aml_dmx_hw_release(dmx);
			goto err_demux_release;
		}
		dev_dbg(dev,
			"aml_dvb_probe: demux %d - IRQ handler registered OK\n",
			i);
	}
	dev_dbg(dev,
		"aml_dvb_probe: all demux cores initialized successfully\n");

	/*
     * Register Async FIFO IRQs.
     *
     * DTS interrupt order (vendor /proc/interrupts verification):
     *   [0..num_demux-1] = dmx IRQs (registered above)
     *   [num_demux + 0]  = SPI 51 → asyncfifo0  (6219 hits during active recording)
     *   [num_demux + 1]  = SPI 57 → asyncfifo1
     *
     * fill_irq: FIFO full IRQ (data arrived)
     * flush_irq: timeout/flush IRQ — same line, no separate handler needed
     */
	for (i = 0; i < dvb->caps.num_asyncfifo; i++) {
		struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];

		if (!afifo->enabled)
			continue;

		if (i < dvb->caps.num_demux) {
			ret = aml_asyncfifo_set_source(&dvb->asyncfifo[i], i);
			if (ret) {
				dev_err(dev,
					"failed to set source for asyncfifo%d: %d\n",
					i, ret);
				goto err_demux_release;
			}
		}

		snprintf(afifo->irq_name, sizeof(afifo->irq_name),
			 "asyncfifo%d", i);
		ret = platform_get_irq(pdev, dvb->caps.num_demux + i);
		if (ret < 0) {
			dev_err(dev, "failed to get asyncfifo%d IRQ: %d\n", i,
				ret);
			goto err_demux_release;
		}
		afifo->fill_irq = ret;
		dev_info(dev, "afifo%d: IRQ %d\n", i, afifo->fill_irq);

		ret = devm_request_irq(dev, afifo->fill_irq,
				       aml_asyncfifo_irq_handler, IRQF_SHARED,
				       afifo->irq_name, afifo);
		if (ret) {
			dev_err(dev,
				"failed to request IRQ %d for asyncfifo%d: %d\n",
				afifo->fill_irq, i, ret);
			goto err_demux_release;
		}
		dev_info(dev, "afifo%d: IRQ %d registered OK\n", i,
			 afifo->fill_irq);
		/* Check DEMUX_CONTROL after IRQ registration */
		{
			u32 ctrl_check = 0;
			aml_read_reg(dvb, DEMUX_CONTROL(0), &ctrl_check);
			dev_info(dev,
				 "DEMUX_CONTROL(0) after IRQ setup = 0x%08x\n",
				 ctrl_check);
		}
	}

	dev_dbg(dev, "aml_dvb_probe: initializing watchdog\n");
	aml_watchdog_init(dvb);

	dev_dbg(dev, "aml_dvb_probe: initializing debugfs\n");
	aml_dvb_debugfs_init(dvb);

	pm_runtime_enable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_set_autosuspend_delay(dev, 1000);
	pm_runtime_use_autosuspend(dev);

	dev_info(dev, AML_DVB_CARD_NAME " " AML_DVB_VERSION " loaded\n");

	return 0;

err_demux_release:
	dev_err(dev, "aml_dvb_probe: error at demux %d, cleaning up\n", i);
	while (--i >= 0) {
		aml_dmx_dvb_release(&dvb->demux[i]);
		aml_dmx_hw_release(&dvb->demux[i]);
	}
	aml_dsc_release_all(dvb);
err_asyncfifo_release:
	aml_asyncfifo_release(dvb);
err_ts_release:
	aml_ts_hw_release(dvb);
err_unregister_adapter:
	dvb_unregister_adapter(&dvb->adapter);
err_clk_disable:
	aml_dvb_clk_disable(dvb);
err_free_wq:
	if (dvb->ts_wq)
		destroy_workqueue(dvb->ts_wq);
	dev_err(dev, "aml_dvb_probe: failed with error %d\n", ret);
	return ret;
}

static void aml_dvb_remove(struct platform_device *pdev)
{
	struct aml_dvb *dvb = platform_get_drvdata(pdev);
	int i;

	dev_dbg(dvb->dev, "aml_dvb_remove: starting\n");

	aml_watchdog_release(dvb);

	pm_runtime_disable(dvb->dev);
	pm_runtime_set_suspended(dvb->dev);

	aml_dvb_debugfs_exit(dvb);

	for (i = 0; i < dvb->caps.num_demux; i++) {
		dev_info(dvb->dev, "aml_dvb_remove: releasing demux %d\n", i);
		aml_dmx_dvb_release(&dvb->demux[i]);
		aml_dmx_hw_release(&dvb->demux[i]);
	}

	aml_dsc_release_all(dvb);
	aml_asyncfifo_release(dvb);
	aml_ts_hw_release(dvb);

	dvb_unregister_adapter(&dvb->adapter);
	aml_dvb_clk_disable(dvb);

	if (dvb->ts_wq)
		destroy_workqueue(dvb->ts_wq);

	dev_info(dvb->dev, "Amlogic DVB driver removed\n");
}

/* ---------------------------------------------------------------------- */
/* Power Management */
/* ---------------------------------------------------------------------- */
static int aml_dvb_suspend(struct device *dev)
{
	struct aml_dvb *dvb = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < dvb->caps.num_demux; i++)
		dvb->demux[i].suspended = true;

	return aml_dvb_runtime_suspend(dev);
}

static int aml_dvb_resume(struct device *dev)
{
	struct aml_dvb *dvb = dev_get_drvdata(dev);
	int i;

	aml_dvb_runtime_resume(dev);

	aml_dvb_hw_reset(dvb);
	aml_ts_hw_init(dvb);

	for (i = 0; i < dvb->caps.num_demux; i++) {
		if (!dvb->demux[i].suspended)
			continue;
		aml_dmx_hw_init(&dvb->demux[i]);
		dvb->demux[i].suspended = false;
	}

	return 0;
}

static const struct dev_pm_ops aml_dvb_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(aml_dvb_suspend, aml_dvb_resume)
		SET_RUNTIME_PM_OPS(aml_dvb_runtime_suspend,
				   aml_dvb_runtime_resume, NULL)
};

/* ---------------------------------------------------------------------- */
/* Device ID table */
/* ---------------------------------------------------------------------- */
static const struct of_device_id aml_dvb_of_match[] = {
	{ .compatible = "amlogic,sm1-dvb" },
	{ .compatible = "amlogic,g12b-dvb" },
	{ .compatible = "amlogic,g12a-dvb" },
	{ .compatible = "amlogic,gxm-dvb" },
	{ .compatible = "amlogic,gxl-dvb" },
	{ .compatible = "amlogic,gxbb-dvb" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aml_dvb_of_match);

static struct platform_driver aml_dvb_driver = {
    .probe    = aml_dvb_probe,
    .remove   = aml_dvb_remove,
    .driver    = {
        .name    = "amlogic-dvb",
        .of_match_table = aml_dvb_of_match,
        .pm    = &aml_dvb_pm_ops,
    },
};

/* ---------------------------------------------------------------------- */
/* Module Init/Exit — Register I2C and Platform Drivers Together */
/* ---------------------------------------------------------------------- */

static int __init aml_dvb_init(void)
{
	int ret;

	/* Register the platform driver (Demux).
     * The AVL6862 I2C driver self-registers with module_i2c_driver()
     * in its own module — no registration needed here. */
	ret = platform_driver_register(&aml_dvb_driver);
	if (ret) {
		pr_err("amlogic-dvb: Failed to register platform driver: %d\n",
		       ret);
		return ret;
	}

	return 0;
}

static void __exit aml_dvb_exit(void)
{
	platform_driver_unregister(&aml_dvb_driver);
}

/* Using manual init/exit instead of module_platform_driver */
module_init(aml_dvb_init);
module_exit(aml_dvb_exit);

MODULE_AUTHOR("Kağan Kadioğlu <kagankadioglutk@hotmail.com>");
MODULE_DESCRIPTION("Amlogic DVB/TS demultiplexer driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(AML_DVB_VERSION);
