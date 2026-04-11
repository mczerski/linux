/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Amlogic DVB/TS hardware demultiplexer driver
 *
 * Copyright (C) 2026 Kağan Kadioğlu <kagankadioglutk@hotmail.com>
 */
#ifndef __AML_DVB_H
#define __AML_DVB_H

#define AML_DVB_CARD_NAME  "amlogic-dvb"
#define AML_DVB_VERSION    "V2.68"

#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "amlogic-dvb-regs.h" /* AML_TS_REG_FLAG and other register macros */
#include <linux/atomic.h>
#include <linux/timer.h>
#include <linux/debugfs.h>
#include <linux/netdevice.h>
#include <linux/u64_stats_sync.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h> /* ADDED: for struct i2c_client */

#include <media/dmxdev.h>
#include <media/dvbdev.h>
#include <media/dvb_demux.h>
#include <media/dvb_net.h>
#include <media/dvb_ringbuffer.h>
#include <media/dvb_frontend.h>
#include <linux/dvb/ca.h>

#include "amlogic-dvb-soc.h"
#include "amlogic-dvb-hwops.h"

/* Map legacy kernel functions to their current equivalents */
#ifndef del_timer_sync
  #define del_timer_sync timer_delete_sync
#endif
#ifndef del_timer
  #define del_timer timer_delete
#endif

/* Hardware limits — updated values */
#define AML_MAX_DEMUX		3
#define AML_MAX_TS_INPUT	2
#define AML_MAX_S2P		3
#define AML_MAX_DSC		2
#define AML_MAX_ASYNCFIFO	3
#define AML_HW_PID_MAX		32
#define AML_MAX_FRONTEND	4
#define AML_MAX_GPIOS		16	/* platform GPIO table size */

/* Section filter buffer — same as vendor aml_dvb.h */
#define AML_SEC_BUF_GRP_COUNT	4
#define AML_SEC_GRP_LEN_SHIFT	0xc		/* 1 << 0xc = 4096 byte/slot */
#define AML_SEC_BUF_COUNT	(AML_SEC_BUF_GRP_COUNT * 8)  /* 32 buffer slot */
#define AML_SEC_WD_BUSY0_MAX	5		/* busy=0 watchdog threshold */
#define AML_SEC_WD_BUSY1_MAX	0x500		/* busy=1 watchdog threshold */

/* TS packet constants */
#define AML_TS_PACKET_SIZE		188
#define AML_TS_BURST_PACKETS		32
#define AML_TS_DMA_SIZE			(AML_TS_PACKET_SIZE * AML_TS_BURST_PACKETS)
#define AML_TS_CHUNK_PACKETS		512
#define AML_ASYNC_FLUSH_SIZE		(256 * 1024)  /* 256KB — processing unit per IRQ.
						       * V2.17: 256KB→64KB (to increase IRQ rate)
						       * V2.19: 64KB→256KB (to reduce CPU load)
						       *
						       * During Tvheadend multi-TP scan, dvb_dmx_swfilter()
						       * is serialised with spin_lock(&demux->lock).
						       * 64KB@8ms → 117 IRQ/s, 40K pkt/s × 100 filter = 100% CPU
						       * 256KB@32ms → 29 IRQ/s → ~4× less CPU usage.
						       * Section filter max latency: 32ms (timeout 15s → no issue).
						       * Streaming latency: 32ms (acceptable for live TV). */
/*
 * AML_ASYNC_BUF_SIZE = 512KB  (same as vendor asyncfifo_buf_len default)
 *
 * VENDOR SOURCE ANALYSIS (aml_dmx.c async_fifo_set_regs):
 *   REG1 FLUSH_CNT = size >> 7  ← UNIT is 128 BYTES (not 1KB!)
 *   REG3 IRQ_THRESH = (size >> (factor+7)) - 1
 *
 * For 512KB buffer:
 *   FLUSH_CNT = 512K >> 7 = 4096 = 0x1000
 *   factor = dmx_get_order(512K/256K) = dmx_get_order(2) = 1
 *   IRQ_THRESH = (512K >> 8) - 1 = 2047 = 0x7FF  (IRQ every 256KB)
 *   ring_entries = 512K / 256K = 2  (buf_toggle: 0,1)
 *
 * NOTE: AsyncFIFO is only for DVR recording.
 * PAT/PMT section filters use the DMX IRQ path, not AsyncFIFO.
 */
#define AML_ASYNC_BUF_SIZE		(512 * 1024)

/* Demux resources per core — PREVIOUSLY MISSING CONSTANTS ARE HERE */
#define AML_CHANNEL_COUNT	32
#define AML_FILTER_COUNT	32
#define AML_FILTER_LEN		15

/* Buffer sizes */
#define AML_PES_BUF_SIZE	(64 * 1024)
#define AML_SEC_BUF_SIZE	(4096 * 32)
/* AML_ASYNC_BUF_SIZE defined above */
#define AML_TS_SEG_SIZE		AML_TS_DMA_SIZE
#define AML_SMALLSEC_SIZE	(16 * 4 * 256)

/* Default timeout (90KHz ticks) */
#define AML_TIMEOUT_DEFAULT	9000

/* Descrambler key register bits */
#define DESC_WRITE_CW_LO	BIT(5)
#define DESC_WRITE_CW_HI	BIT(6)

/* TS source definitions */
enum aml_ts_source {
	AML_TS_SRC_NONE = 0,
	AML_TS_SRC_FRONTEND_TS0,
	AML_TS_SRC_FRONTEND_TS1,
	AML_TS_SRC_FRONTEND_TS2,
	AML_TS_SRC_FRONTEND_TS3,
	AML_TS_SRC_DVR,
};

#include "amlogic-dvb-regs.h"

struct aml_dvb;
struct aml_dvb_hw_ops;

/* Statistics struct — u64_stats_t based */
struct aml_dmx_stats {
	u64_stats_t ts_packets;
	u64_stats_t section_count;
	u64_stats_t pes_count;
	u64_stats_t ts_errors;
	u64_stats_t crc_errors;
	u64_stats_t overflow_errors;
	u64_stats_t cc_errors;
	u64_stats_t pcr_count;
	u64_stats_t fifo_errors;
	u64_stats_t ts_drops;
	u64_stats_t irq_count;
	u64_stats_t tasklet_count;
	struct u64_stats_sync syncp;
};

/* Per-CPU tasklet (legacy) */
struct aml_cpu_dmx {
	struct tasklet_struct tasklet;
	int cpu;
	struct aml_dmx *dmx;
} ____cacheline_aligned;

struct aml_hw_pid {
	u16 pid;
	bool enabled;
};

struct aml_feed_priv {
	int pid_slot;
	int sec_slot;
	int cpu;
};

struct aml_dmx {
	struct aml_dvb *dvb;
	unsigned int id;
	void __iomem *base;

	struct dvb_demux demux;
	struct dmxdev dmxdev;
	struct dvb_net dvbnet;
	struct dvb_frontend *frontend;

	int irq;
	int dvr_irq;
	char irq_name[32]; /* /proc/interrupts label */

	struct work_struct irq_work;

	dma_addr_t pes_buf_addr;
	void *pes_buf_virt;
	size_t pes_buf_size;

	dma_addr_t sec_buf_addr;
	void *sec_buf_virt;
	size_t sec_buf_size;

	unsigned int source;
	bool suspended;
	bool sf_mode; /* SW filter fallback active */
	int hw_pid_count; /* active HW PID slot count */

	atomic_t dvr_feed_count; /* active DVR feed count */

	u32 features;
#define AML_DMX_CRC_CHECK	BIT(0)

	u16 video_pid;
	u16 audio_pid;
	u16 pcr_pid;
	u16 sub_pid;

	spinlock_t lock ____cacheline_aligned;
	struct aml_dmx_stats stats;

	/* Channel and filter arrays — AML_CHANNEL_COUNT etc. used here */
	struct {
		u16 pid;
		bool used;
		int type; /* DMX_TYPE_SEC / DMX_TYPE_TS */
		int pkt_type; /* FM memory packet type (3=section) */
		int pes_type; /* DMX_PES_* */
		int filter_count;
		struct dvb_demux_feed *feed;
		struct dvb_demux_feed *dvr_feed;
	} channel[AML_CHANNEL_COUNT];

	struct {
		int chan_id;
		bool used;
		struct dmx_section_filter *filter;
		u8 value[AML_FILTER_LEN];
		u8 maskandmode[AML_FILTER_LEN];
		u8 maskandnotmode[AML_FILTER_LEN];
		bool neq;
	} filter[AML_FILTER_COUNT];

	/* ── Section DMA buffer — same as vendor aml_dmx struct ── */
	unsigned long sec_pages; /* DMA buffer virtual addr */
	dma_addr_t sec_pages_map; /* DMA buffer physical addr */
	int sec_total_len;
	struct {
		unsigned long addr; /* virtual (CPU access) */
		int len;
	} sec_buf[AML_SEC_BUF_COUNT];
	u32 sec_buf_watchdog_count[AML_SEC_BUF_COUNT];

	/* ── DVR recorder state ── */
	bool record; /* TS_RECORDER_ENABLE active */
	u32 chan_record_bits; /* DEMUX_CHAN_RECORD_EN bitmask */
	int chan_count; /* active channel count */

	DECLARE_BITMAP(pid_bitmap, AML_HW_PID_MAX);
	spinlock_t pid_lock ____cacheline_aligned;

	DECLARE_BITMAP(sec_bitmap, 32);
	spinlock_t sec_lock ____cacheline_aligned;

	u32 irq_threshold;
	u32 irq_min;
	u32 irq_max;
	u32 irq_count;
	unsigned long irq_check;

	void *dvr_buf;
	dma_addr_t dvr_dma;
	size_t dvr_buf_size;
	u32 dvr_head;
	u32 dvr_tail ____cacheline_aligned;
	wait_queue_head_t dvr_wait;

	struct aml_cpu_dmx __percpu *pcpu;

	u64 pcr;

	unsigned long smallsec_buf;
	dma_addr_t smallsec_dma;
};

/* Descrambler */
struct aml_dsc {
	struct aml_dvb *dvb;
	unsigned int id;
	struct dvb_device *dev;
	bool enabled;
	spinlock_t lock;
	struct {
		u16 pid;
		bool used;
		u8 cw_even[8];
		u8 cw_odd[8];
	} channel[8];
};

struct aml_ts_input {
	bool enabled;
	bool is_serial; /* true: serial input → FEC_SEL=STS */
	u32 mode;
	int clk_div;
	u32 invert;
	u32 fec_ctrl; /* DTS ts<n>_control value → FEC_INPUT_CONTROL[11:0] */
};

struct aml_s2p {
	bool enabled;
	int clk_div;
	u32 invert;
	int serial_sel;
};

struct aml_asyncfifo {
	unsigned int id;
	bool enabled;
	unsigned int source;
	void __iomem *base;
	dma_addr_t buf_addr;
	void *buf_virt;
	size_t buf_size;
	size_t flush_size;
	u32 head ____cacheline_aligned;
	u32 tail ____cacheline_aligned;
	u8 pad[L1_CACHE_BYTES];

	int fill_irq;
	int flush_irq;
	char irq_name[16]; /* /proc/interrupts label — persistent storage */

	struct work_struct poll_work;
	atomic_t pending;
	u32 poll_budget;
	u32 ring_entries;
	struct aml_dvb *dvb;

	u32 irq_threshold;
	u32 irq_threshold_min;
	u32 irq_threshold_max;

	u32 prev_chunk; /* abs_chunk from the last IRQ — for delta calculation */

	int cpu;
};

/* Hardware capabilities (per SoC) */
struct aml_dvb_hw_caps {
	const char *soc_name;
	u8 num_demux;
	u8 num_ts_inputs;
	u8 num_s2p;
	u8 num_dsc;
	u8 num_asyncfifo;
	u16 num_pid_filters;
	u16 num_sec_filters;
	bool has_descrambler;
	bool has_ciplus;
	bool has_hwfilter;
	bool has_dma;
	u32 max_bitrate;
};

struct aml_dvb {
	struct device *dev;
	struct platform_device *pdev;
	struct dvb_adapter adapter;
	struct aml_dvb_soc_data *soc;
	const struct aml_dvb_hw_ops *hwops;

	void __iomem *base_ts; /* 0xffd06000: TS_IN, S2P, TOP_CONFIG */
	void __iomem *base_demux; /* 0xff638000: DEMUX core + ASYNC FIFO */
	void __iomem *
		base_asyncfifo; /* 0xFFD09000: async_fifo2 (per-FIFO channel) */

	struct regmap *regmap_ts; /* stb  region 0xFFD06000 (TS_IN, S2P, TOP) */
	struct regmap
		*regmap_demux; /* demux region 0xFF638000 (core, PID/section) */
	struct regmap
		*regmap_async; /* async_fifo   0xFFD09000 (DMA ring buffer) */

	struct clk *clk_demux;
	struct clk *clk_asyncfifo;
	struct clk *clk_parser;
	struct clk *clk_ahbarb0;

	struct reset_control *rst_demux;
	struct reset_control *rst_asyncfifo;

	struct aml_dmx demux[AML_MAX_DEMUX];
	struct aml_ts_input ts[AML_MAX_TS_INPUT];
	struct aml_s2p s2p[AML_MAX_S2P];
	struct aml_dsc dsc[AML_MAX_DSC];
	struct aml_asyncfifo asyncfifo[AML_MAX_ASYNCFIFO];

	u32 ts_sync_byte;
	u32 ts_packet_len;

	struct workqueue_struct *pcr_wq;
	struct workqueue_struct *ts_wq;
	struct delayed_work watchdog_work;
	struct delayed_work rate_work;
	unsigned int watchdog_disable[AML_MAX_DEMUX];
	unsigned int reset_flag;

	struct aml_dvb_hw_caps caps;

	spinlock_t slock ____cacheline_aligned;
	struct dentry *debugfs_root;
	struct debugfs_regset32 regset;

	atomic_t feed_count; /* active feed count */
	u64_stats_t stats_irq;
	u64_stats_t stats_packet;
	struct u64_stats_sync stats_syncp;

	u32 target_bitrate;
	u32 current_rate;

	/* Platform GPIO table — /sys/kernel/debug/gpio consumer names */
	struct {
		struct gpio_desc *gd;
		const char *label;
		const char *consumer;
	} gpios[AML_MAX_GPIOS];
	int gpio_count;

	struct gpio_desc *tuner_reset_gpio;
	struct gpio_desc *tuner_power_gpio;

	struct timer_list watchdog_timer;
};

/* Helper functions
 *
 * Register routing (verified with dev/mem):
 *   TS_IN_CTRL(n)  = AML_TS_REG(n * 0x140)       ─┐ regmap_ts    (0xffd06000)
 *   TS_S2P_CTRL(n) = AML_TS_REG(n * 0x140 + 0x40) ─┘
 *   DEMUX_CONTROL  = 0xff638010                   ─┐ regmap_demux (0xff638000)
 *   STB_VERSION    = 0xff638000                   ─┘
 *   ASYNC_FIFO_*   = n * 0x1000                   ── regmap_async (0xffd09000)
 *                    FIFO[0]=+0x1000 (0xffd0a000)
 *                    FIFO[1]=+0x0000 (0xffd09000)
 */

static inline int aml_write_reg(struct aml_dvb *dvb, u32 reg, u32 val)
{
	if (AML_IS_TS_REG(reg))
		return regmap_write(dvb->regmap_ts, AML_TS_OFF(reg), val);
	return regmap_write(dvb->regmap_demux, reg, val);
}

static inline int aml_read_reg(struct aml_dvb *dvb, u32 reg, u32 *val)
{
	if (AML_IS_TS_REG(reg))
		return regmap_read(dvb->regmap_ts, AML_TS_OFF(reg), val);
	return regmap_read(dvb->regmap_demux, reg, val);
}

static inline int aml_update_bits(struct aml_dvb *dvb, u32 reg, u32 mask,
				  u32 val)
{
	if (AML_IS_TS_REG(reg))
		return regmap_update_bits(dvb->regmap_ts, AML_TS_OFF(reg), mask,
					  val);
	return regmap_update_bits(dvb->regmap_demux, reg, mask, val);
}

/* Async FIFO register helpers — regmap_async (0xffd09000 base) */
static inline int aml_write_async(struct aml_dvb *dvb, u32 reg, u32 val)
{
	return regmap_write(dvb->regmap_async, reg, val);
}

static inline int aml_read_async(struct aml_dvb *dvb, u32 reg, u32 *val)
{
	return regmap_read(dvb->regmap_async, reg, val);
}

static inline int aml_update_async(struct aml_dvb *dvb, u32 reg, u32 mask,
				   u32 val)
{
	return regmap_update_bits(dvb->regmap_async, reg, mask, val);
}

/* Register map config */
extern const struct regmap_config aml_demux_regmap_config;
extern const struct regmap_config aml_async_regmap_config;

/* Core functions */
struct dvb_adapter *aml_dvb_adapter(struct device *dev);
int aml_dvb_clk_enable(struct aml_dvb *dvb);
void aml_dvb_clk_disable(struct aml_dvb *dvb);
void aml_dvb_hw_reset(struct aml_dvb *dvb);
int aml_dvb_runtime_suspend(struct device *dev);
int aml_dvb_runtime_resume(struct device *dev);

/* Demux hardware */
int aml_dmx_hw_init(struct aml_dmx *dmx);
void aml_dmx_hw_release(struct aml_dmx *dmx);
int aml_dmx_set_source(struct aml_dmx *dmx, unsigned int source);
void aml_dmx_enable_recording(struct aml_dmx *dmx, bool enable);
void aml_dmx_process_section(struct aml_dmx *dmx); /* called from IRQ handler */
int aml_dmx_hw_pid_alloc(struct aml_dmx *dmx, u16 pid);
void aml_dmx_hw_pid_free(struct aml_dmx *dmx, int slot);
int aml_dmx_dvb_init(struct aml_dmx *dmx, struct dvb_adapter *adap);
void aml_dmx_dvb_release(struct aml_dmx *dmx);

/* DVB callbacks */
int aml_dvb_start_feed(struct dvb_demux_feed *feed);
int aml_dvb_stop_feed(struct dvb_demux_feed *feed);

/* TS subsystem */
int aml_ts_hw_init(struct aml_dvb *dvb);
void aml_ts_hw_release(struct aml_dvb *dvb);

/* Async FIFO (DVR) */
int aml_asyncfifo_init(struct aml_dvb *dvb);
void aml_asyncfifo_release(struct aml_dvb *dvb);
int aml_asyncfifo_set_source(struct aml_asyncfifo *afifo, unsigned int source);
irqreturn_t aml_asyncfifo_irq_handler(int irq, void *dev_id);
bool aml_asyncfifo_has_data(struct aml_dvb *dvb);
void aml_asyncfifo_process_all(struct aml_dvb *dvb);
u32 aml_get_ring_fill_percent(struct aml_dvb *dvb);
void aml_asyncfifo_check_backpressure(struct aml_dvb *dvb);

/* Descrambler */
int aml_dsc_init_all(struct aml_dvb *dvb);
void aml_dsc_release_all(struct aml_dvb *dvb);

/* PTS extraction */
u64 aml_dmx_get_video_pts(struct aml_dmx *dmx);
u64 aml_dmx_get_audio_pts(struct aml_dmx *dmx);
u64 aml_dmx_get_pcr(struct aml_dmx *dmx);

/* DebugFS */
#ifdef CONFIG_DEBUG_FS
void aml_dvb_debugfs_init(struct aml_dvb *dvb);
void aml_dvb_debugfs_exit(struct aml_dvb *dvb);
#else
static inline void aml_dvb_debugfs_init(struct aml_dvb *dvb)
{
}
static inline void aml_dvb_debugfs_exit(struct aml_dvb *dvb)
{
}
#endif
/* Advanced features */
int aml_smallsec_init(struct aml_dmx *dmx);
void aml_smallsec_release(struct aml_dmx *dmx);
int aml_timeout_init(struct aml_dmx *dmx);
void aml_timeout_release(struct aml_dmx *dmx);
void aml_watchdog_init(struct aml_dvb *dvb);
void aml_watchdog_release(struct aml_dvb *dvb);

/* Statistics */
void aml_stats_init(struct aml_dmx *dmx);
void aml_stats_cleanup(struct aml_dmx *dmx);
void aml_stats_update_packet(struct aml_dmx *dmx, int type, u64 count);
void aml_stats_update_error(struct aml_dmx *dmx, int error_type);
int aml_stats_get_summary(struct aml_dmx *dmx, char *buf, size_t size);
void aml_stats_reset(struct aml_dmx *dmx);

#endif /* __AML_DVB_H */
