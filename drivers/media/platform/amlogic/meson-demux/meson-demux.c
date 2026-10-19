// SPDX-License-Identifier: GPL-2.0-only
/*
 * Amlogic STB transport stream demultiplexer
 *
 * The STB block receives MPEG transport streams on its TS inputs and filters
 * them in three demux cores. This driver registers one DVB adapter with a
 * frontend, a demux and a dvr device for each connected TS input: frontend
 * N, demux N and dvr N belong to the same input. While a feed of an input
 * runs, a demux core records the input through an async FIFO into a ring in
 * memory and passes it to the software demux of dvb-core, which does the
 * section filtering. The core records only the packets of the PIDs of the
 * running feeds, matched by its PID slots, or the whole stream when the PIDs
 * do not fit into the slots or a feed takes the whole stream (PID 0x2000).
 * Once it records the whole stream, it does so until the last feed stops.
 *
 * The cores, the serial-to-parallel converters and the async FIFOs are taken
 * when an input starts and given back when it stops, so more inputs can be
 * connected than can be recorded at the same time.
 *
 * Copyright (C) 2026 Marek Czerski <m.czerski@ap-tech.pl>
 */

#include <linux/atomic.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/reset.h>
#include <linux/seq_file.h>
#include <linux/sizes.h>
#include <linux/workqueue.h>

#include <media/dmxdev.h>
#include <media/dvb_demux.h>
#include <media/dvb_frontend.h>
#include <media/dvb_fwnode.h>
#include <media/dvbdev.h>

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

/*
 * Registers of a demux core, as 32-bit word offsets. The three cores start
 * at words 0x00, 0x50 and 0xa0 of the STB window. The common registers at
 * words 0xf0 to 0xff are accessed as registers of core 0.
 */
#define STB_CORE_WORDS			0x50

#define STB_TEST_REG			0x01
#define FEC_INPUT_CONTROL		0x02
#define  FEC_SEL			GENMASK(14, 12)
/* S2P0 is source 6, S2P1 source 5 */
#define  FEC_SEL_S2P(n)			(6 - (n))
#define  FEC_INPUT_FEC_CLK		BIT(11)
#define  FEC_INPUT_SOP			BIT(10)
#define  FEC_INPUT_D_VALID		BIT(9)
#define  FEC_INPUT_D_FAIL		BIT(8)
#define FEC_INPUT_DATA			0x03
#define DEMUX_CONTROL			0x04
#define  ENABLE_FREE_CLK_FEC_DATA_VALID	BIT(31)
#define  ENABLE_FREE_CLK_STB_REG	BIT(30)
/* the second video output; with the recorder type it passes slots to the recorder */
#define  VIDEO2_ENABLE			BIT(25)
#define  VIDEO2_TYPE			GENMASK(24, 22)
#define  TS_RECORDER_BEFORE_FILTER	BIT(10)
#define  TS_RECORDER_ENABLE		BIT(9)
#define  SECTION_END_WITH_TABLE_ID	BIT(8)
#define  KEEP_DUPLICATE_PACKAGE		BIT(6)
#define  STB_DEMUX_ENABLE		BIT(4)
#define FM_WR_DATA			0x06
#define FM_WR_ADDR			0x07
#define  FM_WR_DATA_REQUEST		BIT(15)
#define MAX_FM_COMP_ADDR		0x08
/* the last filter memory word with a PID slot in use */
#define  MAX_FM_PID_WORD		GENMASK(3, 0)
#define OM_CMD_STATUS			0x0b
#define  OM_CMD_PENDING			BIT(0)
#define STB_INT_STATUS			0x23
#define STB_INT_MASK			0x32
#define DEMUX_PACKET_COUNT		0x49
#define  INPUT_PACKET_COUNT		GENMASK(30, 0)
/* TS input source for the demux: its own input or the stream of another demux */
#define TS_TOP_CONFIG1			0x4f
#define  FEC_SEL_DEMUX(demux)		(GENMASK(1, 0) << (2 * (demux)))

#define STB_TOP_CONFIG			0xf0
/*
 * polarity invert bits and TS input selection:
 * S2P0 in bits 0 to 6, S2P1 in bits 16 to 22
 */
#define  S2P_SHIFT(n)			(16 * (n))
#define  INVERT_S2P(n)			(GENMASK(6, 2) << S2P_SHIFT(n))
#define  S2P_FEC_SERIAL_SEL(n)		(GENMASK(1, 0) << S2P_SHIFT(n))

/*
 * Filter memory words 0 to 15 hold the 32 PID slots, two per word: the even
 * slot in bits 31 to 16, the odd one in bits 15 to 0. A slot holds a type
 * and a PID. Type 7 sends the packets of the PID to the recorder. The vendor
 * driver uses 31 slots.
 */
#define STB_PID_WORDS			16
#define STB_PID_SLOTS			31
#define STB_PID_SLOT_TYPE		GENMASK(15, 13)
#define STB_PID_SLOT_PID		GENMASK(12, 0)
#define  STB_PID_TYPE_RECORDER		7
/* an unused slot: type 7 with PID 0x1ffe, which never occurs */
#define STB_PID_SLOT_UNUSED		0xfffe
#define STB_PID_UNUSED			0xfffefffe
/* no PID in the slot */
#define STB_PID_NONE			0xffff
/* the PID of a feed of the whole stream */
#define STB_PID_ALL			0x2000
/* null packets */
#define STB_PID_NULL			0x1fff

/*
 * DEMUX_CONTROL for recording; TS_RECORDER_BEFORE_FILTER for the whole
 * stream. Packets of the slots reach the recorder only through the second
 * video output set to the recorder type. It stays set in both modes, as
 * switching it off while recording corrupts a packet.
 */
#define STB_RECORD			(ENABLE_FREE_CLK_FEC_DATA_VALID | \
					 ENABLE_FREE_CLK_STB_REG | \
					 VIDEO2_ENABLE | \
					 FIELD_PREP_CONST(VIDEO2_TYPE, \
							  STB_PID_TYPE_RECORDER) | \
					 TS_RECORDER_ENABLE | \
					 SECTION_END_WITH_TABLE_ID | \
					 KEEP_DUPLICATE_PACKAGE | \
					 STB_DEMUX_ENABLE)

/* async FIFO registers, byte offsets */
#define ASYNC_FIFO_REG0			0x00
#define ASYNC_FIFO_REG1			0x04
#define  ASYNC_FIFO_ERR			BIT(30)
#define  ASYNC_FIFO_RESET		BIT(22)
#define  ASYNC_FIFO_WRAP_EN		BIT(21)
#define  ASYNC_FIFO_FLUSH_EN		BIT(20)
#define  ASYNC_FIFO_FLUSH_CNT		GENMASK(14, 0)
#define ASYNC_FIFO_REG2			0x08
#define  ASYNC_FIFO_FULL		BIT(26)
#define  ASYNC_FIFO_SOURCE		GENMASK(24, 23)
#define  ASYNC_FIFO_ENDIAN		GENMASK(22, 21)
#define  ASYNC_FIFO_FILL_EN		BIT(20)
#define  ASYNC_FIFO_FILL_CNT		GENMASK(19, 0)
#define ASYNC_FIFO_REG3			0x0c
#define  ASYNC_FLUSH_SIZE_IRQ		GENMASK(15, 0)

static const u8 meson_stb_fifo_source[] = { 3, 2 };

/* the async FIFO counts in units of 128 bytes */
#define ASYNC_FIFO_UNIT			128

/*
 * An async FIFO writes the ring chunk by chunk and raises its interrupt
 * after each chunk, which the driver counts. While it runs, REG0 reads back
 * the address it writes to; at the interrupt that is at most one burst into
 * the next chunk, which corrects the count after a lost interrupt. The ring
 * needs no alignment beyond 4 bytes.
 */
#define STB_RING_SIZE			SZ_1M
#define STB_CHUNK_SIZE			SZ_64K
#define STB_NUM_CHUNKS			(STB_RING_SIZE / STB_CHUNK_SIZE)

/*
 * With only a few PIDs recorded, a chunk can take seconds or longer to fill.
 * The worker also runs at this interval and passes on what the FIFO wrote of
 * the current chunk, up to the 128-byte burst before the write address.
 */
#define STB_POLL_INTERVAL		msecs_to_jiffies(20)

/**
 * struct meson_stb_data - resources of the STB block of a SoC
 * @num_inputs: TS input ports
 * @num_cores: demux cores
 * @num_s2p: serial-to-parallel converters
 * @num_fifos: async FIFOs, at most ARRAY_SIZE(meson_stb_fifo_source)
 * @has_ts_top_config1: TS_TOP_CONFIG1 selects the stream of each core
 */
struct meson_stb_data {
	unsigned int num_inputs;
	unsigned int num_cores;
	unsigned int num_s2p;
	unsigned int num_fifos;
	bool has_ts_top_config1;
};

struct meson_stb_demux;
struct meson_stb_input;

struct meson_stb_fifo {
	struct meson_stb_demux *stb;
	unsigned int id;
	void __iomem *base;
	struct reset_control *rst;

	void *ring;
	dma_addr_t ring_dma;
	/* the input recorded into the ring, NULL while idle; under stb->lock */
	struct meson_stb_input *input;
	/* chunks written, counted by the interrupt handler */
	atomic_t chunks_done;
	/* chunks passed to the software demux */
	u32 chunks_read;
	/* bytes of the chunk after them passed to the software demux */
	u32 chunk_bytes_read;
	/* interrupts found missing from the write address */
	u32 lost_irqs;
	/* times the FIFO overwrote chunks not read yet */
	u32 overruns;
	struct delayed_work work;
};

struct meson_stb_input {
	struct meson_stb_demux *stb;
	/* TS input port, or -1 for a demux fed only through its dvr device */
	int port;
	/* number of the frontend, demux and dvr devices in the adapter */
	unsigned int index;
	struct dvb_ts_endpoint ts;
	struct dvb_frontend *fe;

	/* running feeds, under demux.mutex */
	unsigned int feeds;
	/* while recording, under stb->lock: core, S2P (or -1) and FIFO */
	int core;
	int s2p;
	struct meson_stb_fifo *fifo;
	/*
	 * While recording, under stb->lock: the PID of each slot, and whether
	 * the core records the whole stream instead of the PIDs of the slots.
	 */
	u16 slot_pid[STB_PID_SLOTS];
	bool record_all;

	struct dvb_demux demux;
	struct dmxdev dmxdev;
	struct dmx_frontend hw_frontend;
	struct dmx_frontend mem_frontend;
};

struct meson_stb_demux {
	struct device *dev;
	const struct meson_stb_data *data;
	void __iomem *stb;

	struct meson_stb_fifo *fifos;
	struct meson_stb_input *inputs;
	unsigned int num_inputs;
	struct dvb_adapter adapter;

	/*
	 * Protects the busy cores and S2P converters, the input of each FIFO,
	 * and STB_TOP_CONFIG.
	 */
	struct mutex lock;
	unsigned long cores_busy;
	unsigned long s2p_busy;

	struct dentry *debugfs;
};

static void __iomem *stb_reg(struct meson_stb_demux *stb, unsigned int core,
			     unsigned int reg)
{
	return stb->stb + (core * STB_CORE_WORDS + reg) * 4;
}

static u32 stb_read(struct meson_stb_demux *stb, unsigned int core,
		    unsigned int reg)
{
	return readl(stb_reg(stb, core, reg));
}

static void stb_write(struct meson_stb_demux *stb, unsigned int core,
		      unsigned int reg, u32 val)
{
	writel(val, stb_reg(stb, core, reg));
}

static void stb_update(struct meson_stb_demux *stb, unsigned int core,
		       unsigned int reg, u32 mask, u32 val)
{
	stb_write(stb, core, reg, (stb_read(stb, core, reg) & ~mask) | val);
}

static u32 fifo_read(struct meson_stb_fifo *fifo, unsigned int reg)
{
	return readl(fifo->base + reg);
}

static void fifo_write(struct meson_stb_fifo *fifo, unsigned int reg, u32 val)
{
	writel(val, fifo->base + reg);
}

static void fifo_update(struct meson_stb_fifo *fifo, unsigned int reg,
			u32 mask, u32 val)
{
	fifo_write(fifo, reg, (fifo_read(fifo, reg) & ~mask) | val);
}

/* Write and read back a test pattern, as the vendor driver does */
static int meson_stb_test_core(struct meson_stb_demux *stb, unsigned int core)
{
	static const u32 patterns[] = { 0x5550, 0xaaa0 };
	unsigned int i;
	u32 val;

	for (i = 0; i < ARRAY_SIZE(patterns); i++) {
		stb_write(stb, core, STB_TEST_REG, patterns[i]);
		val = stb_read(stb, core, STB_TEST_REG);
		if (val != patterns[i]) {
			dev_err(stb->dev, "core %u: test register reads 0x%x, not 0x%x\n",
				core, val, patterns[i]);
			return -ENODEV;
		}
	}

	return 0;
}

static int meson_stb_hw_init(struct meson_stb_demux *stb,
			     struct reset_control *rst)
{
	unsigned int core, i;
	u32 val, mask = 0, sel = 0;
	int ret;

	ret = reset_control_reset(rst);
	if (ret)
		return ret;

	for (i = 0; i < stb->data->num_fifos; i++) {
		ret = reset_control_reset(stb->fifos[i].rst);
		if (ret)
			return ret;
	}

	for (core = 0; core < stb->data->num_cores; core++) {
		ret = readl_poll_timeout(stb_reg(stb, core, OM_CMD_STATUS), val,
					 !(val & OM_CMD_PENDING), 10, 10000);
		if (ret) {
			dev_err(stb->dev, "core %u: still busy after reset\n",
				core);
			return ret;
		}

		ret = meson_stb_test_core(stb, core);
		if (ret)
			return ret;

		stb_write(stb, core, DEMUX_CONTROL, 0);
		stb_write(stb, core, MAX_FM_COMP_ADDR, 0);
		stb_write(stb, core, STB_INT_MASK, 0);
		stb_write(stb, core, STB_INT_STATUS, 0xffff);
		stb_write(stb, core, FEC_INPUT_CONTROL, 0);

		/* each core takes its own input */
		mask |= FEC_SEL_DEMUX(core);
		sel |= core << (2 * core);
	}

	stb_write(stb, 0, STB_TOP_CONFIG, 0);
	if (stb->data->has_ts_top_config1)
		stb_update(stb, 0, TS_TOP_CONFIG1, mask, sel);

	return 0;
}

static int meson_stb_fm_wait(struct meson_stb_demux *stb, unsigned int core)
{
	u32 val;

	return readl_poll_timeout(stb_reg(stb, core, FM_WR_ADDR), val,
				  !(val & FM_WR_DATA_REQUEST), 1, 1000);
}

/* Write a word of the filter memory of a core */
static int meson_stb_fm_write(struct meson_stb_demux *stb, unsigned int core,
			      unsigned int addr, u32 data)
{
	int ret;

	ret = meson_stb_fm_wait(stb, core);
	if (ret)
		return ret;

	stb_write(stb, core, FM_WR_DATA, data);
	stb_write(stb, core, FM_WR_ADDR, FM_WR_DATA_REQUEST | addr);

	return meson_stb_fm_wait(stb, core);
}

/* Route the TS input to its core, through its S2P converter if serial */
static int meson_stb_setup_core(struct meson_stb_input *in)
{
	struct meson_stb_demux *stb = in->stb;
	const struct dvb_ts_endpoint *ts = &in->ts;
	unsigned int i;
	u32 fec;
	int ret;

	/* no PID slot matches */
	for (i = 0; i < STB_PID_WORDS; i++) {
		ret = meson_stb_fm_write(stb, in->core, i, STB_PID_UNUSED);
		if (ret)
			return ret;
	}
	stb_update(stb, in->core, MAX_FM_COMP_ADDR, MAX_FM_PID_WORD, 0);
	for (i = 0; i < STB_PID_SLOTS; i++)
		in->slot_pid[i] = STB_PID_NONE;

	if (in->s2p >= 0) {
		stb_update(stb, 0, STB_TOP_CONFIG,
			   S2P_FEC_SERIAL_SEL(in->s2p) | INVERT_S2P(in->s2p),
			   in->port << S2P_SHIFT(in->s2p));
		fec = FIELD_PREP(FEC_SEL, FEC_SEL_S2P(in->s2p));
	} else {
		fec = FIELD_PREP(FEC_SEL, in->port);
	}

	/*
	 * Bits 11 to 8 invert the clock, SOP, VALID and FAIL signals. The
	 * S905X datasheet has the input sampled at the falling clock edge with
	 * CLK_POL set, taken to be bit 11.
	 */
	if (ts->clk_edge == DVB_TS_CLK_EDGE_FALLING)
		fec |= FEC_INPUT_FEC_CLK;
	if (ts->sync_active_low)
		fec |= FEC_INPUT_SOP;
	if (ts->valid_active_low)
		fec |= FEC_INPUT_D_VALID;
	if (ts->error_active_low)
		fec |= FEC_INPUT_D_FAIL;

	stb_write(stb, in->core, FEC_INPUT_CONTROL, fec);

	return 0;
}

static void meson_stb_fifo_start(struct meson_stb_fifo *fifo,
				 unsigned int core)
{
	atomic_set(&fifo->chunks_done, 0);
	fifo->chunks_read = 0;
	fifo->chunk_bytes_read = 0;
	fifo->lost_irqs = 0;

	/* the sequence of the vendor driver */
	fifo_write(fifo, ASYNC_FIFO_REG0, lower_32_bits(fifo->ring_dma));
	fifo_write(fifo, ASYNC_FIFO_REG1,
		   ASYNC_FIFO_RESET | ASYNC_FIFO_WRAP_EN |
		   FIELD_PREP(ASYNC_FIFO_FLUSH_CNT,
			      STB_RING_SIZE / ASYNC_FIFO_UNIT));
	fifo_update(fifo, ASYNC_FIFO_REG1, ASYNC_FIFO_RESET, 0);
	fifo_update(fifo, ASYNC_FIFO_REG1, ASYNC_FIFO_FLUSH_EN,
		    ASYNC_FIFO_FLUSH_EN);
	fifo_write(fifo, ASYNC_FIFO_REG2, FIELD_PREP(ASYNC_FIFO_ENDIAN, 1));
	fifo_update(fifo, ASYNC_FIFO_REG2, ASYNC_FIFO_FILL_EN,
		    ASYNC_FIFO_FILL_EN);
	fifo_update(fifo, ASYNC_FIFO_REG3, ASYNC_FLUSH_SIZE_IRQ,
		    FIELD_PREP(ASYNC_FLUSH_SIZE_IRQ,
			       STB_CHUNK_SIZE / ASYNC_FIFO_UNIT - 1));
	fifo_update(fifo, ASYNC_FIFO_REG2, ASYNC_FIFO_SOURCE,
		    FIELD_PREP(ASYNC_FIFO_SOURCE, meson_stb_fifo_source[core]));
}

static void meson_stb_fifo_stop(struct meson_stb_fifo *fifo)
{
	fifo_update(fifo, ASYNC_FIFO_REG1, ASYNC_FIFO_FLUSH_EN, 0);
	fifo_update(fifo, ASYNC_FIFO_REG2, ASYNC_FIFO_FILL_EN, 0);
}

/* The lowest free one of @num resources, or -1 */
static int meson_stb_find_free(unsigned long busy, unsigned int num)
{
	unsigned int n = find_first_zero_bit(&busy, num);

	return n < num ? n : -1;
}

/*
 * Collect the PIDs to record for the running feeds of an input into @pids,
 * counting @start as running and @stop as stopped. Returns the number of
 * PIDs, or -1 when the core has to record the whole stream: for a feed of
 * PID 0x2000 or more PIDs than slots.
 *
 * The null packets are always recorded. The core writes to the FIFO in
 * 128-byte bursts and keeps the rest of a packet until more packets come; with
 * only rare PIDs, such as a single table, the end of a section would wait
 * for the next repetition.
 */
static int meson_stb_feed_pids(struct meson_stb_input *in,
			       struct dvb_demux_feed *start,
			       struct dvb_demux_feed *stop, u16 *pids)
{
	struct dvb_demux *demux = &in->demux;
	int i, j, num = 0;

	pids[num++] = STB_PID_NULL;

	for (i = 0; i < demux->feednum; i++) {
		struct dvb_demux_feed *feed = &demux->feed[i];

		if (feed != start &&
		    (feed == stop || feed->state != DMX_STATE_GO))
			continue;
		if (feed->pid == STB_PID_ALL)
			return -1;

		for (j = 0; j < num && pids[j] != feed->pid; j++)
			;
		if (j < num)
			continue;
		if (num == STB_PID_SLOTS)
			return -1;
		pids[num++] = feed->pid;
	}

	return num;
}

static bool meson_stb_has_pid(const u16 *pids, int num, u16 pid)
{
	int i;

	for (i = 0; i < num; i++)
		if (pids[i] == pid)
			return true;

	return false;
}

static u16 meson_stb_slot(struct meson_stb_input *in, unsigned int slot)
{
	if (slot >= STB_PID_SLOTS || in->slot_pid[slot] == STB_PID_NONE)
		return STB_PID_SLOT_UNUSED;

	return FIELD_PREP(STB_PID_SLOT_TYPE, STB_PID_TYPE_RECORDER) |
	       FIELD_PREP(STB_PID_SLOT_PID, in->slot_pid[slot]);
}

/*
 * Set the PID slots of a core to @num PIDs. A PID keeps its slot while it is
 * in use, and only the filter memory words that change are written, so the
 * packets of the other PIDs keep flowing.
 */
static int meson_stb_write_slots(struct meson_stb_input *in, const u16 *pids,
				 int num)
{
	struct meson_stb_demux *stb = in->stb;
	unsigned long changed = 0;
	unsigned int slot, word, last = 0;
	int i, ret;

	for (slot = 0; slot < STB_PID_SLOTS; slot++) {
		u16 pid = in->slot_pid[slot];

		if (pid != STB_PID_NONE && !meson_stb_has_pid(pids, num, pid)) {
			in->slot_pid[slot] = STB_PID_NONE;
			changed |= BIT(slot / 2);
		}
	}

	for (i = 0; i < num; i++) {
		if (meson_stb_has_pid(in->slot_pid, STB_PID_SLOTS, pids[i]))
			continue;

		for (slot = 0; in->slot_pid[slot] != STB_PID_NONE; slot++)
			;
		in->slot_pid[slot] = pids[i];
		changed |= BIT(slot / 2);
	}

	for_each_set_bit(word, &changed, STB_PID_WORDS) {
		ret = meson_stb_fm_write(stb, in->core, word,
					 (u32)meson_stb_slot(in, 2 * word) << 16 |
					 meson_stb_slot(in, 2 * word + 1));
		if (ret)
			return ret;
	}

	for (slot = 0; slot < STB_PID_SLOTS; slot++)
		if (in->slot_pid[slot] != STB_PID_NONE)
			last = slot / 2;
	stb_update(stb, in->core, MAX_FM_COMP_ADDR, MAX_FM_PID_WORD,
		   FIELD_PREP(MAX_FM_PID_WORD, last));

	return 0;
}

/*
 * Take a core, an S2P converter for a serial input and an async FIFO, and
 * start recording @num PIDs of the input, or the whole stream if @num is
 * negative. Returns -EBUSY when one of them is not free.
 */
static int meson_stb_capture_start(struct meson_stb_input *in,
				   const u16 *pids, int num)
{
	struct meson_stb_demux *stb = in->stb;
	const struct meson_stb_data *data = stb->data;
	struct meson_stb_fifo *fifo = NULL;
	unsigned long fifos_busy = 0;
	int core, s2p = -1, f, ret;
	unsigned int i;

	mutex_lock(&stb->lock);

	for (i = 0; i < data->num_fifos; i++)
		if (stb->fifos[i].input)
			fifos_busy |= BIT(i);

	core = meson_stb_find_free(stb->cores_busy, data->num_fifos);
	f = meson_stb_find_free(fifos_busy, data->num_fifos);
	if (in->ts.bus_width == 1)
		s2p = meson_stb_find_free(stb->s2p_busy, data->num_s2p);

	if (core < 0 || f < 0 || (in->ts.bus_width == 1 && s2p < 0)) {
		dev_dbg(stb->dev, "TS%d: no %s free\n", in->port,
			core < 0 ? "demux core" :
			f < 0 ? "async FIFO" : "serial-to-parallel converter");
		ret = -EBUSY;
		goto unlock;
	}

	fifo = &stb->fifos[f];
	in->core = core;
	in->s2p = s2p;

	ret = meson_stb_setup_core(in);
	if (!ret && num >= 0)
		ret = meson_stb_write_slots(in, pids, num);
	if (ret)
		goto unlock;

	stb->cores_busy |= BIT(core);
	if (s2p >= 0)
		stb->s2p_busy |= BIT(s2p);
	in->fifo = fifo;
	fifo->input = in;
	in->record_all = num < 0;

	meson_stb_fifo_start(fifo, core);
	stb_write(stb, core, DEMUX_CONTROL, STB_RECORD |
		  (in->record_all ? TS_RECORDER_BEFORE_FILTER : 0));
	queue_delayed_work(system_highpri_wq, &fifo->work, STB_POLL_INTERVAL);

	dev_dbg(stb->dev, "TS%d: core %d, S2P %d, async FIFO %u\n",
		in->port, core, s2p, fifo->id);

unlock:
	mutex_unlock(&stb->lock);
	return ret;
}

static void meson_stb_capture_stop(struct meson_stb_input *in)
{
	struct meson_stb_demux *stb = in->stb;
	struct meson_stb_fifo *fifo = in->fifo;

	mutex_lock(&stb->lock);

	stb_write(stb, in->core, DEMUX_CONTROL, 0);
	meson_stb_fifo_stop(fifo);
	/* a late interrupt can still queue the work, which then finds no input */
	WRITE_ONCE(fifo->input, NULL);
	cancel_delayed_work_sync(&fifo->work);

	stb->cores_busy &= ~BIT(in->core);
	if (in->s2p >= 0)
		stb->s2p_busy &= ~BIT(in->s2p);
	in->fifo = NULL;

	mutex_unlock(&stb->lock);
}

/*
 * Record @num PIDs of a recording input, or the whole stream if @num is
 * negative. Switching the recorder from the whole stream back to the slots
 * can drop the packet in transit, so once it records the whole stream, it
 * does so until the capture stops.
 */
static int meson_stb_set_pids(struct meson_stb_input *in, const u16 *pids,
			      int num)
{
	struct meson_stb_demux *stb = in->stb;
	int ret = 0;

	mutex_lock(&stb->lock);

	if (in->record_all)
		goto unlock;

	if (num < 0) {
		stb_update(stb, in->core, DEMUX_CONTROL,
			   TS_RECORDER_BEFORE_FILTER, TS_RECORDER_BEFORE_FILTER);
		in->record_all = true;
	} else {
		ret = meson_stb_write_slots(in, pids, num);
	}

unlock:
	mutex_unlock(&stb->lock);
	return ret;
}

static irqreturn_t meson_stb_fifo_irq(int irq, void *data)
{
	struct meson_stb_fifo *fifo = data;
	u32 done, wptr, lost;

	/*
	 * One pulse per chunk written, there is no status to acknowledge.
	 * The FIFO now writes the chunk after the last completed one; if it
	 * is a few chunks further on, interrupts were lost. Only this handler
	 * writes the count while the capture runs.
	 */
	done = atomic_read(&fifo->chunks_done) + 1;
	wptr = fifo_read(fifo, ASYNC_FIFO_REG0) - lower_32_bits(fifo->ring_dma);
	lost = (wptr / STB_CHUNK_SIZE - done) % STB_NUM_CHUNKS;
	if (wptr < STB_RING_SIZE && lost < STB_NUM_CHUNKS / 2) {
		fifo->lost_irqs += lost;
		done += lost;
	}
	atomic_set(&fifo->chunks_done, done);
	mod_delayed_work(system_highpri_wq, &fifo->work, 0);

	return IRQ_HANDLED;
}

/* Pass bytes @start to @end of the ring to the software demux */
static void meson_stb_fifo_pass(struct meson_stb_fifo *fifo,
				struct meson_stb_input *in, unsigned int start,
				unsigned int end)
{
	struct device *dev = fifo->stb->dev;
	dma_addr_t dma = fifo->ring_dma + start;

	dma_sync_single_for_cpu(dev, dma, end - start, DMA_FROM_DEVICE);
	/*
	 * The data is not aligned to packets; dvb_dmx_swfilter() finds the
	 * packets. While writes to dvr0 feed the demux, the input is dropped.
	 */
	if (in->demux.dmx.frontend == &in->hw_frontend)
		dvb_dmx_swfilter(&in->demux, fifo->ring + start, end - start);
	dma_sync_single_for_device(dev, dma, end - start, DMA_FROM_DEVICE);
}

static void meson_stb_fifo_work(struct work_struct *work)
{
	struct meson_stb_fifo *fifo =
		container_of(to_delayed_work(work), struct meson_stb_fifo, work);
	struct meson_stb_input *in = READ_ONCE(fifo->input);
	u32 done = atomic_read(&fifo->chunks_done);
	unsigned int offset, wptr, end;

	if (!in)
		return;

	/* the FIFO wrapped over chunks not read yet: keep the newest ones */
	if (done - fifo->chunks_read > STB_NUM_CHUNKS - 1) {
		fifo->overruns++;
		fifo->chunks_read = done - (STB_NUM_CHUNKS - 1);
		fifo->chunk_bytes_read = 0;
	}

	for (; fifo->chunks_read != done; fifo->chunks_read++) {
		offset = (fifo->chunks_read % STB_NUM_CHUNKS) * STB_CHUNK_SIZE;
		meson_stb_fifo_pass(fifo, in, offset + fifo->chunk_bytes_read,
				    offset + STB_CHUNK_SIZE);
		fifo->chunk_bytes_read = 0;
	}

	/*
	 * The chunk being written, up to the burst before the write address.
	 * If the FIFO has already moved on to the next chunk, the interrupt
	 * of this one follows.
	 */
	offset = (done % STB_NUM_CHUNKS) * STB_CHUNK_SIZE;
	wptr = fifo_read(fifo, ASYNC_FIFO_REG0) - lower_32_bits(fifo->ring_dma);
	if (wptr >= offset + ASYNC_FIFO_UNIT && wptr < offset + STB_CHUNK_SIZE) {
		end = ALIGN_DOWN(wptr - offset, ASYNC_FIFO_UNIT) -
		      ASYNC_FIFO_UNIT;
		if (end > fifo->chunk_bytes_read) {
			meson_stb_fifo_pass(fifo, in,
					    offset + fifo->chunk_bytes_read,
					    offset + end);
			fifo->chunk_bytes_read = end;
		}
	}

	queue_delayed_work(system_highpri_wq, &fifo->work, STB_POLL_INTERVAL);
}

static int meson_stb_start_feed(struct dvb_demux_feed *feed)
{
	struct meson_stb_input *in = feed->demux->priv;
	u16 pids[STB_PID_SLOTS];
	int num, ret;

	/* the input is recorded while any feed runs */
	if (in->port >= 0) {
		num = meson_stb_feed_pids(in, feed, NULL, pids);
		if (in->feeds == 0)
			ret = meson_stb_capture_start(in, pids, num);
		else
			ret = meson_stb_set_pids(in, pids, num);
		if (ret)
			return ret;
	}
	in->feeds++;

	return 0;
}

static int meson_stb_stop_feed(struct dvb_demux_feed *feed)
{
	struct meson_stb_input *in = feed->demux->priv;
	u16 pids[STB_PID_SLOTS];
	int num;

	if (in->port < 0) {
		in->feeds--;
		return 0;
	}

	if (--in->feeds == 0) {
		meson_stb_capture_stop(in);
		return 0;
	}

	num = meson_stb_feed_pids(in, NULL, feed, pids);
	return meson_stb_set_pids(in, pids, num);
}

/*
 * Find the TS inputs: every port with an endpoint whose remote device is
 * enabled. Get the frontend connected to each.
 */
static int meson_stb_find_inputs(struct meson_stb_demux *stb)
{
	struct fwnode_handle *fwnode = dev_fwnode(stb->dev);
	unsigned int port;
	int ret;

	stb->inputs = devm_kcalloc(stb->dev, stb->data->num_inputs,
				   sizeof(*stb->inputs), GFP_KERNEL);
	if (!stb->inputs)
		return -ENOMEM;

	for (port = 0; port < stb->data->num_inputs; port++) {
		struct fwnode_handle *ep __free(fwnode_handle) =
			fwnode_graph_get_endpoint_by_id(fwnode, port, 0,
							FWNODE_GRAPH_ENDPOINT_NEXT);
		struct meson_stb_input *in = &stb->inputs[stb->num_inputs];

		if (!ep)
			continue;

		ret = dvb_fwnode_ts_endpoint_parse(ep, &in->ts);
		if (ret)
			return dev_err_probe(stb->dev, ret,
					     "invalid endpoint of TS input %u\n",
					     port);

		in->fe = devm_dvb_frontend_get(stb->dev, ep);
		if (IS_ERR(in->fe))
			return dev_err_probe(stb->dev, PTR_ERR(in->fe),
					     "no frontend on TS input %u\n",
					     port);

		in->port = port;
		in->index = stb->num_inputs++;
	}

	/* without a connected input, a demux takes writes to dvr0 */
	if (!stb->num_inputs) {
		dev_info(stb->dev, "no TS input connected\n");
		stb->inputs[0].port = -1;
		stb->num_inputs = 1;
	}

	return 0;
}

static void meson_stb_free_ring(void *data)
{
	struct meson_stb_fifo *fifo = data;

	dma_free_noncoherent(fifo->stb->dev, STB_RING_SIZE, fifo->ring,
			     fifo->ring_dma, DMA_FROM_DEVICE);
}

static void meson_stb_cancel_work(void *data)
{
	struct meson_stb_fifo *fifo = data;

	cancel_delayed_work_sync(&fifo->work);
}

static int meson_stb_get_fifo(struct platform_device *pdev,
			      struct meson_stb_demux *stb, unsigned int id)
{
	struct meson_stb_fifo *fifo = &stb->fifos[id];
	struct device *dev = stb->dev;
	char name[20];
	int irq, ret;

	fifo->stb = stb;
	fifo->id = id;
	INIT_DELAYED_WORK(&fifo->work, meson_stb_fifo_work);
	snprintf(name, sizeof(name), "asyncfifo%u", id);

	fifo->base = devm_platform_ioremap_resource_byname(pdev, name);
	if (IS_ERR(fifo->base))
		return PTR_ERR(fifo->base);

	fifo->rst = devm_reset_control_get_exclusive(dev, name);
	if (IS_ERR(fifo->rst))
		return dev_err_probe(dev, PTR_ERR(fifo->rst),
				     "failed to get the reset of %s\n", name);

	irq = platform_get_irq_byname(pdev, name);
	if (irq < 0)
		return irq;

	fifo->ring = dma_alloc_noncoherent(dev, STB_RING_SIZE, &fifo->ring_dma,
					   DMA_FROM_DEVICE, GFP_KERNEL);
	if (!fifo->ring)
		return -ENOMEM;

	ret = devm_add_action_or_reset(dev, meson_stb_free_ring, fifo);
	if (ret)
		return ret;

	/* released after the interrupt and before the ring */
	ret = devm_add_action_or_reset(dev, meson_stb_cancel_work, fifo);
	if (ret)
		return ret;

	return devm_request_irq(dev, irq, meson_stb_fifo_irq, 0,
				dev_name(dev), fifo);
}

/*
 * Register the demux, dvr and frontend devices of an input. Called for the
 * inputs in order: the adapter gives each device type the lowest free
 * number, so the devices of input N are demux N, dvr N and frontend N.
 */
static int meson_stb_input_register(struct meson_stb_input *in)
{
	struct dvb_adapter *adapter = &in->stb->adapter;
	struct dvb_demux *demux = &in->demux;
	struct dmx_demux *dmx = &demux->dmx;
	int ret;

	demux->priv = in;
	demux->filternum = 256;
	demux->feednum = 256;
	demux->start_feed = meson_stb_start_feed;
	demux->stop_feed = meson_stb_stop_feed;
	dmx->capabilities = DMX_TS_FILTERING | DMX_SECTION_FILTERING |
			    DMX_MEMORY_BASED_FILTERING;
	ret = dvb_dmx_init(demux);
	if (ret)
		return ret;

	in->dmxdev.filternum = 256;
	in->dmxdev.demux = dmx;
	in->dmxdev.capabilities = 0;
	ret = dvb_dmxdev_init(&in->dmxdev, adapter);
	if (ret)
		goto err_release_dmx;

	in->hw_frontend.source = DMX_FRONTEND_0;
	ret = dmx->add_frontend(dmx, &in->hw_frontend);
	if (ret)
		goto err_release_dmxdev;

	/* writes to dvr0 feed the software demux through this one */
	in->mem_frontend.source = DMX_MEMORY_FE;
	ret = dmx->add_frontend(dmx, &in->mem_frontend);
	if (ret)
		goto err_remove_hw_frontend;

	ret = dmx->connect_frontend(dmx, &in->hw_frontend);
	if (ret)
		goto err_remove_mem_frontend;

	if (in->fe) {
		in->fe->id = in->index;
		ret = dvb_register_frontend(adapter, in->fe);
		if (ret)
			goto err_disconnect_frontend;
	}

	return 0;

err_disconnect_frontend:
	dmx->disconnect_frontend(dmx);
err_remove_mem_frontend:
	dmx->remove_frontend(dmx, &in->mem_frontend);
err_remove_hw_frontend:
	dmx->remove_frontend(dmx, &in->hw_frontend);
err_release_dmxdev:
	dvb_dmxdev_release(&in->dmxdev);
err_release_dmx:
	dvb_dmx_release(demux);
	return ret;
}

static void meson_stb_input_unregister(struct meson_stb_input *in)
{
	struct dmx_demux *dmx = &in->demux.dmx;

	if (in->fe)
		dvb_unregister_frontend(in->fe);
	dmx->disconnect_frontend(dmx);
	dmx->remove_frontend(dmx, &in->mem_frontend);
	dmx->remove_frontend(dmx, &in->hw_frontend);
	dvb_dmxdev_release(&in->dmxdev);
	dvb_dmx_release(&in->demux);
}

static void meson_stb_status_input(struct seq_file *s,
				   struct meson_stb_input *in)
{
	struct meson_stb_demux *stb = in->stb;
	unsigned int i;

	if (in->port < 0) {
		seq_printf(s, "demux%u: no TS input\n", in->index);
		return;
	}

	seq_printf(s, "TS%d: %s, frontend%u, demux%u, dvr%u, feeds %u",
		   in->port, in->ts.bus_width == 1 ? "serial" : "parallel",
		   in->index, in->index, in->index, in->feeds);
	if (!in->fifo) {
		seq_puts(s, ", idle\n");
		return;
	}

	seq_printf(s, ", core %d", in->core);
	if (in->s2p >= 0)
		seq_printf(s, ", S2P%d", in->s2p);
	seq_printf(s, ", async FIFO %u\n", in->fifo->id);
	if (in->record_all) {
		seq_puts(s, "  PIDs: whole stream\n");
	} else {
		seq_puts(s, "  PIDs:");
		for (i = 0; i < STB_PID_SLOTS; i++)
			if (in->slot_pid[i] != STB_PID_NONE)
				seq_printf(s, " %u:0x%04x", i, in->slot_pid[i]);
		seq_putc(s, '\n');
	}
	seq_printf(s, "  FEC_INPUT_CONTROL: 0x%08x\n",
		   stb_read(stb, in->core, FEC_INPUT_CONTROL));
	seq_printf(s, "  FEC_INPUT_DATA: 0x%08x\n",
		   stb_read(stb, in->core, FEC_INPUT_DATA));
	seq_printf(s, "  DEMUX_CONTROL: 0x%08x\n",
		   stb_read(stb, in->core, DEMUX_CONTROL));
	seq_printf(s, "  STB_INT_STATUS: 0x%08x\n",
		   stb_read(stb, in->core, STB_INT_STATUS));
	seq_printf(s, "  input packets: %u\n",
		   (u32)FIELD_GET(INPUT_PACKET_COUNT,
				  stb_read(stb, in->core, DEMUX_PACKET_COUNT)));
}

static void meson_stb_status_fifo(struct seq_file *s,
				  struct meson_stb_fifo *fifo)
{
	u32 reg1 = fifo_read(fifo, ASYNC_FIFO_REG1);
	u32 reg2 = fifo_read(fifo, ASYNC_FIFO_REG2);

	seq_printf(s, "async FIFO %u:", fifo->id);
	if (fifo->input)
		seq_printf(s, " TS%d\n", fifo->input->port);
	else
		seq_puts(s, " idle\n");
	seq_printf(s, "  chunks written: %u\n", atomic_read(&fifo->chunks_done));
	seq_printf(s, "  chunks read: %u\n", fifo->chunks_read);
	seq_printf(s, "  overruns: %u\n", fifo->overruns);
	seq_printf(s, "  lost interrupts: %u\n", fifo->lost_irqs);
	seq_printf(s, "  REG0: 0x%08x\n", fifo_read(fifo, ASYNC_FIFO_REG0));
	seq_printf(s, "  REG1: 0x%08x%s\n", reg1,
		   reg1 & ASYNC_FIFO_ERR ? " (error)" : "");
	seq_printf(s, "  REG2: 0x%08x%s, fill count %u\n", reg2,
		   reg2 & ASYNC_FIFO_FULL ? " (full)" : "",
		   (u32)FIELD_GET(ASYNC_FIFO_FILL_CNT, reg2));
}

static int meson_stb_status_show(struct seq_file *s, void *data)
{
	struct meson_stb_demux *stb = s->private;
	unsigned int i;

	mutex_lock(&stb->lock);
	for (i = 0; i < stb->num_inputs; i++)
		meson_stb_status_input(s, &stb->inputs[i]);
	for (i = 0; i < stb->data->num_fifos; i++)
		meson_stb_status_fifo(s, &stb->fifos[i]);
	mutex_unlock(&stb->lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(meson_stb_status);

static int meson_stb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct reset_control *rst;
	struct meson_stb_demux *stb;
	struct clk_bulk_data *clks;
	unsigned int i;
	int ret;

	stb = devm_kzalloc(dev, sizeof(*stb), GFP_KERNEL);
	if (!stb)
		return -ENOMEM;

	stb->dev = dev;
	stb->data = device_get_match_data(dev);
	platform_set_drvdata(pdev, stb);

	if (stb->data->num_fifos > ARRAY_SIZE(meson_stb_fifo_source) ||
	    stb->data->num_fifos > stb->data->num_cores)
		return -EINVAL;

	ret = devm_mutex_init(dev, &stb->lock);
	if (ret)
		return ret;

	stb->stb = devm_platform_ioremap_resource_byname(pdev, "stb");
	if (IS_ERR(stb->stb))
		return PTR_ERR(stb->stb);

	/* the async FIFOs take a 32-bit address */
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	ret = devm_clk_bulk_get_all_enabled(dev, &clks);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to enable the clocks\n");

	rst = devm_reset_control_get_exclusive(dev, "demux");
	if (IS_ERR(rst))
		return dev_err_probe(dev, PTR_ERR(rst),
				     "failed to get the demux reset\n");

	ret = meson_stb_find_inputs(stb);
	if (ret)
		return ret;

	stb->fifos = devm_kcalloc(dev, stb->data->num_fifos,
				  sizeof(*stb->fifos), GFP_KERNEL);
	if (!stb->fifos)
		return -ENOMEM;

	for (i = 0; i < stb->data->num_fifos; i++) {
		ret = meson_stb_get_fifo(pdev, stb, i);
		if (ret)
			return dev_err_probe(dev, ret,
					     "failed to set up async FIFO %u\n",
					     i);
	}

	ret = meson_stb_hw_init(stb, rst);
	if (ret)
		return dev_err_probe(dev, ret, "failed to initialize\n");

	ret = dvb_register_adapter(&stb->adapter, "Amlogic STB demux",
				   THIS_MODULE, dev, adapter_nr);
	if (ret < 0)
		return dev_err_probe(dev, ret,
				     "failed to register the adapter\n");

	for (i = 0; i < stb->num_inputs; i++) {
		stb->inputs[i].stb = stb;
		ret = meson_stb_input_register(&stb->inputs[i]);
		if (ret) {
			while (i--)
				meson_stb_input_unregister(&stb->inputs[i]);
			dvb_unregister_adapter(&stb->adapter);
			return dev_err_probe(dev, ret,
					     "failed to register the demux\n");
		}
	}

	stb->debugfs = debugfs_create_dir(dev_name(dev), NULL);
	debugfs_create_file("status", 0444, stb->debugfs, stb,
			    &meson_stb_status_fops);

	return 0;
}

static void meson_stb_remove(struct platform_device *pdev)
{
	struct meson_stb_demux *stb = platform_get_drvdata(pdev);
	unsigned int i;

	debugfs_remove_recursive(stb->debugfs);
	for (i = 0; i < stb->num_inputs; i++)
		meson_stb_input_unregister(&stb->inputs[i]);
	dvb_unregister_adapter(&stb->adapter);
}

/* GX: TS_TOP_CONFIG1 is documented only from G12A on */
static const struct meson_stb_data meson_gxbb_stb_data = {
	.num_inputs = 3,
	.num_cores = 3,
	.num_s2p = 2,
	.num_fifos = 2,
};

static const struct meson_stb_data meson_g12a_stb_data = {
	.num_inputs = 3,
	.num_cores = 3,
	.num_s2p = 2,
	.num_fifos = 2,
	.has_ts_top_config1 = true,
};

static const struct of_device_id meson_stb_of_match[] = {
	{ .compatible = "amlogic,meson-gxbb-demux",
	  .data = &meson_gxbb_stb_data },
	{ .compatible = "amlogic,meson-g12a-demux",
	  .data = &meson_g12a_stb_data },
	{ }
};
MODULE_DEVICE_TABLE(of, meson_stb_of_match);

static struct platform_driver meson_stb_driver = {
	.probe = meson_stb_probe,
	.remove = meson_stb_remove,
	.driver = {
		.name = "meson-stb-demux",
		.of_match_table = meson_stb_of_match,
	},
};
module_platform_driver(meson_stb_driver);

MODULE_DESCRIPTION("Amlogic STB transport stream demultiplexer");
MODULE_AUTHOR("Marek Czerski <m.czerski@ap-tech.pl>");
MODULE_LICENSE("GPL");
