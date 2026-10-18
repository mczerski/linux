// SPDX-License-Identifier: GPL-2.0-only
/*
 * Amlogic ISO 7816 smart card interface
 *
 * The interface generates the card clock and sends and receives the
 * characters on the card I/O line through two small FIFOs. The card RST
 * line, the activation of the card interface chip and the card presence
 * signal are GPIOs. The driver does no protocol handling: a card server in
 * user space resets the card, reads the ATR and exchanges T=0 or T=1 bytes
 * with it through /dev/smcN, with the interface of the Amlogic vendor
 * driver (include/uapi/linux/amsmc.h).
 *
 * Copyright (C) 2026 Marek Czerski <m.czerski@ap-tech.pl>
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/reset.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <uapi/linux/amsmc.h>

#define SMC_REG0			0x00
#define  SMC_START_ATR_EN		BIT(31)
#define  SMC_CARD_DETECT		BIT(30)
#define  SMC_IO_LEVEL			BIT(29)
#define  SMC_START_ATR			BIT(27)
#define  SMC_RST_LEVEL			BIT(26)
#define  SMC_CLK_OEN			BIT(25)
#define  SMC_CLK_EN			BIT(24)
#define  SMC_RECV_FIFO_THRESHOLD	GENMASK(23, 20)
#define  SMC_ENABLE			BIT(19)
#define  SMC_FIRST_ETU_OFFSET		GENMASK(18, 16)
#define  SMC_ETU_DIVIDER		GENMASK(15, 0)
#define SMC_REG1			0x04
#define  SMC_ETU_CLK_SEL		GENMASK(29, 28)
#define  SMC_ATR_HOLDOFF_EN		BIT(27)
#define  SMC_ATR_CLK_MUX		GENMASK(26, 24)
#define  SMC_ATR_HOLDOFF_TCNT		GENMASK(23, 16)
#define  SMC_ATR_FINAL_TCNT		GENMASK(15, 0)
#define SMC_REG2			0x08
#define  SMC_CLK_SEL			GENMASK(31, 30)
#define  SMC_RECV_RETRY_CNT		GENMASK(28, 26)
#define  SMC_IO_FILTER_SEL		GENMASK(25, 23)
#define  SMC_DET_FILTER_SEL		GENMASK(22, 20)
#define  SMC_CLK_TCNT			GENMASK(19, 12)
#define  SMC_PULSE_IRQ			BIT(11)
#define  SMC_RECV_NO_PARITY		BIT(10)
#define  SMC_RECV_PARITY		BIT(9)
#define  SMC_RECV_LSB_MSB		BIT(8)
#define  SMC_RECV_INVERT		BIT(7)
#define  SMC_XMIT_REPEAT_DIS		BIT(6)
#define  SMC_XMIT_RETRIES		GENMASK(5, 3)
#define  SMC_XMIT_PARITY		BIT(2)
#define  SMC_XMIT_LSB_MSB		BIT(1)
#define  SMC_XMIT_INVERT		BIT(0)
#define SMC_STATUS			0x0c
#define  SMC_SEND_FIFO_FULL		BIT(27)
#define  SMC_SEND_FIFO_EMPTY		BIT(26)
#define  SMC_RECV_FIFO_EMPTY		BIT(20)
/*
 * The low half of SMC_INTR holds the interrupt status bits, which are
 * cleared by writing 1, the high half the matching enable bits.
 */
#define SMC_INTR			0x10
#define  SMC_INT_STATUS			GENMASK(9, 0)
#define  SMC_INT_SEND_LAST_BYTE		BIT(1)
#define  SMC_INT_RECV_THRESHOLD		BIT(0)
#define  SMC_INT_ENABLE(x)		((x) << 16)
#define SMC_REG5			0x14
#define  SMC_ETU_MSR_EN			BIT(18)
#define  SMC_CWT_DETECT_EN		BIT(17)
#define  SMC_BWT_DETECT_EN		BIT(16)
#define  SMC_BWT_BASE_TIME		GENMASK(15, 0)
#define SMC_REG6			0x18
#define  SMC_BWI			GENMASK(23, 20)
#define  SMC_BGT			GENMASK(19, 12)
#define  SMC_CWI			GENMASK(11, 8)
#define  SMC_N				GENMASK(7, 0)
#define SMC_FIFO			0x1c

/* The ETU counter runs from the 24 MHz crystal (etu_clk_sel = 0) */
#define SMC_ETU_CLK_KHZ			24000

#define SMC_BUF_SIZE			1024

static const u8 smc_inverse[256] = {
	0xff, 0x7f, 0xbf, 0x3f, 0xdf, 0x5f, 0x9f, 0x1f,
	0xef, 0x6f, 0xaf, 0x2f, 0xcf, 0x4f, 0x8f, 0x0f,
	0xf7, 0x77, 0xb7, 0x37, 0xd7, 0x57, 0x97, 0x17,
	0xe7, 0x67, 0xa7, 0x27, 0xc7, 0x47, 0x87, 0x07,
	0xfb, 0x7b, 0xbb, 0x3b, 0xdb, 0x5b, 0x9b, 0x1b,
	0xeb, 0x6b, 0xab, 0x2b, 0xcb, 0x4b, 0x8b, 0x0b,
	0xf3, 0x73, 0xb3, 0x33, 0xd3, 0x53, 0x93, 0x13,
	0xe3, 0x63, 0xa3, 0x23, 0xc3, 0x43, 0x83, 0x03,
	0xfd, 0x7d, 0xbd, 0x3d, 0xdd, 0x5d, 0x9d, 0x1d,
	0xed, 0x6d, 0xad, 0x2d, 0xcd, 0x4d, 0x8d, 0x0d,
	0xf5, 0x75, 0xb5, 0x35, 0xd5, 0x55, 0x95, 0x15,
	0xe5, 0x65, 0xa5, 0x25, 0xc5, 0x45, 0x85, 0x05,
	0xf9, 0x79, 0xb9, 0x39, 0xd9, 0x59, 0x99, 0x19,
	0xe9, 0x69, 0xa9, 0x29, 0xc9, 0x49, 0x89, 0x09,
	0xf1, 0x71, 0xb1, 0x31, 0xd1, 0x51, 0x91, 0x11,
	0xe1, 0x61, 0xa1, 0x21, 0xc1, 0x41, 0x81, 0x01,
	0xfe, 0x7e, 0xbe, 0x3e, 0xde, 0x5e, 0x9e, 0x1e,
	0xee, 0x6e, 0xae, 0x2e, 0xce, 0x4e, 0x8e, 0x0e,
	0xf6, 0x76, 0xb6, 0x36, 0xd6, 0x56, 0x96, 0x16,
	0xe6, 0x66, 0xa6, 0x26, 0xc6, 0x46, 0x86, 0x06,
	0xfa, 0x7a, 0xba, 0x3a, 0xda, 0x5a, 0x9a, 0x1a,
	0xea, 0x6a, 0xaa, 0x2a, 0xca, 0x4a, 0x8a, 0x0a,
	0xf2, 0x72, 0xb2, 0x32, 0xd2, 0x52, 0x92, 0x12,
	0xe2, 0x62, 0xa2, 0x22, 0xc2, 0x42, 0x82, 0x02,
	0xfc, 0x7c, 0xbc, 0x3c, 0xdc, 0x5c, 0x9c, 0x1c,
	0xec, 0x6c, 0xac, 0x2c, 0xcc, 0x4c, 0x8c, 0x0c,
	0xf4, 0x74, 0xb4, 0x34, 0xd4, 0x54, 0x94, 0x14,
	0xe4, 0x64, 0xa4, 0x24, 0xc4, 0x44, 0x84, 0x04,
	0xf8, 0x78, 0xb8, 0x38, 0xd8, 0x58, 0x98, 0x18,
	0xe8, 0x68, 0xa8, 0x28, 0xc8, 0x48, 0x88, 0x08,
	0xf0, 0x70, 0xb0, 0x30, 0xd0, 0x50, 0x90, 0x10,
	0xe0, 0x60, 0xa0, 0x20, 0xc0, 0x40, 0x80, 0x00,
};

struct meson_smc {
	struct device *dev;
	void __iomem *base;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *detect_gpio;
	struct miscdevice misc;
	int id;
	/* rate of the card clock source, fclk_div4 (clk_sel = 0) */
	unsigned int src_khz;

	/* serialises the file operations */
	struct mutex lock;
	bool busy;
	bool active;
	/* the card uses the inverse convention, bytes are converted */
	bool inverse;
	struct am_smc_param param;
	struct am_smc_atr atr;
	/* a PPS request may follow the ATR, see meson_smc_pps_request() */
	bool pps_allowed;
	unsigned int pps_len;
	unsigned int pps_resp_len;
	u8 pps[6];
	u8 pps_resp[6];

	/* protects the FIFOs against the interrupt handler */
	spinlock_t slock;
	wait_queue_head_t wq;
	DECLARE_KFIFO(rx, u8, SMC_BUF_SIZE);
	DECLARE_KFIFO(tx, u8, SMC_BUF_SIZE);
	u8 buf[SMC_BUF_SIZE];
};

static DEFINE_IDA(meson_smc_ida);

/* ISO 7816-3 tables 7 and 8, 0 for RFU */
static const u16 smc_fi[16] = {
	372, 372, 558, 744, 1116, 1488, 1860, 0,
	0, 512, 768, 1024, 1536, 2048, 0, 0,
};

static const u8 smc_di[16] = {
	0, 1, 2, 4, 8, 16, 32, 64, 12, 20, 0, 0, 0, 0, 0, 0,
};

static const struct am_smc_param meson_smc_default_param = {
	.f = 372,
	.d = 1,
	.n = 0,
	.bwi = 4,
	.cwi = 13,
	.bgt = 22,
	.freq = 4000,
	.recv_no_parity = 1,
	.xmit_retries = 1,
	.xmit_repeat_dis = 1,
};

static u32 meson_smc_read(struct meson_smc *smc, unsigned int reg)
{
	return readl(smc->base + reg);
}

static void meson_smc_write(struct meson_smc *smc, unsigned int reg, u32 val)
{
	writel(val, smc->base + reg);
}

static void meson_smc_update_bits(struct meson_smc *smc, unsigned int reg,
				  u32 mask, u32 val)
{
	u32 tmp = meson_smc_read(smc, reg);

	meson_smc_write(smc, reg, (tmp & ~mask) | (val & mask));
}

static bool meson_smc_card_in(struct meson_smc *smc)
{
	return gpiod_get_value_cansleep(smc->detect_gpio) > 0;
}

/* Clock divider and ETU divider for the card clock and F/D of @p */
static int meson_smc_dividers(struct meson_smc *smc,
			      const struct am_smc_param *p, u32 *clk_tcnt,
			      u32 *etu_div)
{
	unsigned int tcnt, card_khz;
	u64 div;

	if (p->freq <= 0 || p->f <= 0 || p->d <= 0)
		return -EINVAL;

	tcnt = DIV_ROUND_UP(smc->src_khz, p->freq);
	if (tcnt < 1 || tcnt > FIELD_MAX(SMC_CLK_TCNT) + 1)
		return -EINVAL;
	card_khz = smc->src_khz / tcnt;

	/* one ETU is F/D card clock periods */
	div = DIV_ROUND_CLOSEST_ULL((u64)SMC_ETU_CLK_KHZ * p->f,
				    (u64)p->d * card_khz);
	if (div < 1 || div > FIELD_MAX(SMC_ETU_DIVIDER) + 1)
		return -EINVAL;

	*clk_tcnt = tcnt - 1;
	*etu_div = div - 1;
	return 0;
}

static u32 meson_smc_reg2(const struct am_smc_param *p, u32 clk_tcnt)
{
	return FIELD_PREP(SMC_CLK_SEL, 0) |
	       FIELD_PREP(SMC_IO_FILTER_SEL, 3) |
	       FIELD_PREP(SMC_DET_FILTER_SEL, 3) |
	       FIELD_PREP(SMC_CLK_TCNT, clk_tcnt) |
	       (p->recv_no_parity ? SMC_RECV_NO_PARITY : 0) |
	       (p->recv_parity ? SMC_RECV_PARITY : 0) |
	       (p->recv_lsb_msb ? SMC_RECV_LSB_MSB : 0) |
	       (p->recv_invert ? SMC_RECV_INVERT : 0) |
	       (p->xmit_repeat_dis ? SMC_XMIT_REPEAT_DIS : 0) |
	       FIELD_PREP(SMC_XMIT_RETRIES, p->xmit_retries) |
	       (p->xmit_parity ? SMC_XMIT_PARITY : 0) |
	       (p->xmit_lsb_msb ? SMC_XMIT_LSB_MSB : 0) |
	       (p->xmit_invert ? SMC_XMIT_INVERT : 0);
}

static u32 meson_smc_reg6(const struct am_smc_param *p)
{
	/* the block guard time counter counts from 2 */
	return FIELD_PREP(SMC_BWI, p->bwi) |
	       FIELD_PREP(SMC_BGT, max(p->bgt, 2) - 2) |
	       FIELD_PREP(SMC_CWI, p->cwi) |
	       FIELD_PREP(SMC_N, p->n);
}

/* Program the transmission parameters, the card clock keeps running */
static int meson_smc_set_param(struct meson_smc *smc,
			       const struct am_smc_param *p)
{
	u32 clk_tcnt, etu_div;
	int ret;

	ret = meson_smc_dividers(smc, p, &clk_tcnt, &etu_div);
	if (ret)
		return ret;

	smc->param = *p;
	meson_smc_update_bits(smc, SMC_REG0, SMC_ETU_DIVIDER,
			      FIELD_PREP(SMC_ETU_DIVIDER, etu_div));
	meson_smc_write(smc, SMC_REG2, meson_smc_reg2(p, clk_tcnt));
	meson_smc_update_bits(smc, SMC_REG5, SMC_BWT_BASE_TIME,
			      FIELD_PREP(SMC_BWT_BASE_TIME, 999));
	meson_smc_write(smc, SMC_REG6, meson_smc_reg6(p));
	return 0;
}

/* Program all registers for the start of a session, card clock stopped */
static int meson_smc_hw_setup(struct meson_smc *smc)
{
	u32 clk_tcnt, etu_div;
	int ret;

	ret = meson_smc_dividers(smc, &smc->param, &clk_tcnt, &etu_div);
	if (ret)
		return ret;

	meson_smc_write(smc, SMC_INTR, SMC_INT_STATUS);
	meson_smc_write(smc, SMC_REG0,
			SMC_ENABLE |
			FIELD_PREP(SMC_RECV_FIFO_THRESHOLD, 1) |
			FIELD_PREP(SMC_FIRST_ETU_OFFSET, 5) |
			FIELD_PREP(SMC_ETU_DIVIDER, etu_div));
	meson_smc_write(smc, SMC_REG1,
			SMC_ATR_HOLDOFF_EN |
			FIELD_PREP(SMC_ATR_CLK_MUX, 4) |
			FIELD_PREP(SMC_ATR_HOLDOFF_TCNT, 255) |
			FIELD_PREP(SMC_ATR_FINAL_TCNT, 40000));
	meson_smc_write(smc, SMC_REG2, meson_smc_reg2(&smc->param, clk_tcnt));
	meson_smc_write(smc, SMC_REG5, FIELD_PREP(SMC_BWT_BASE_TIME, 999));
	meson_smc_write(smc, SMC_REG6, meson_smc_reg6(&smc->param));
	return 0;
}

static void meson_smc_flush(struct meson_smc *smc)
{
	unsigned long flags;

	spin_lock_irqsave(&smc->slock, flags);
	kfifo_reset(&smc->rx);
	kfifo_reset(&smc->tx);
	while (!(meson_smc_read(smc, SMC_STATUS) & SMC_RECV_FIFO_EMPTY))
		meson_smc_read(smc, SMC_FIFO);
	spin_unlock_irqrestore(&smc->slock, flags);
}

static void meson_smc_deactivate(struct meson_smc *smc)
{
	if (!smc->active)
		return;

	/* ISO 7816-3 6.4: RST low, CLK stopped, then VCC off */
	meson_smc_write(smc, SMC_INTR, SMC_INT_STATUS);
	gpiod_set_value_cansleep(smc->reset_gpio, 1);
	meson_smc_update_bits(smc, SMC_REG0,
			      SMC_START_ATR_EN | SMC_START_ATR | SMC_CLK_EN |
			      SMC_RST_LEVEL, 0);
	usleep_range(200, 250);
	gpiod_set_value_cansleep(smc->enable_gpio, 0);
	smc->active = false;
}

static int meson_smc_activate(struct meson_smc *smc)
{
	int ret;

	if (smc->active)
		return 0;

	gpiod_set_value_cansleep(smc->reset_gpio, 1);
	gpiod_set_value_cansleep(smc->enable_gpio, 1);
	usleep_range(200, 250);
	ret = meson_smc_hw_setup(smc);
	if (ret)
		return ret;

	smc->active = true;
	return 0;
}

static void meson_smc_convert(struct meson_smc *smc, u8 *buf, unsigned int len)
{
	unsigned int i;

	if (!smc->inverse)
		return;
	for (i = 0; i < len; i++)
		buf[i] = smc_inverse[buf[i]];
}

/* Wait for @len received bytes and append them to the ATR */
static int meson_smc_atr_get(struct meson_smc *smc, unsigned int len,
			     unsigned int timeout_ms)
{
	struct am_smc_atr *atr = &smc->atr;
	u8 *dst = (u8 *)atr->atr + atr->atr_len;

	if (atr->atr_len + len > AMSMC_MAX_ATR_LEN)
		return -EIO;

	if (!wait_event_timeout(smc->wq, kfifo_len(&smc->rx) >= len,
				msecs_to_jiffies(timeout_ms)))
		return -ETIMEDOUT;

	if (kfifo_out(&smc->rx, dst, len) != len)
		return -EIO;
	meson_smc_convert(smc, dst, len);
	atr->atr_len += len;
	return 0;
}

/* Switch to Fi and Di of a TA1 or PPS1 byte */
static void meson_smc_set_fd(struct meson_smc *smc, u8 fidi)
{
	struct am_smc_param p = smc->param;

	p.f = smc_fi[fidi >> 4];
	p.d = smc_di[fidi & 0x0f];
	if (!p.f || !p.d || meson_smc_set_param(smc, &p)) {
		dev_dbg(smc->dev, "unsupported Fi/Di 0x%02x\n", fidi);
		return;
	}
	dev_dbg(smc->dev, "F %d D %d\n", p.f, p.d);
}

/* Length of a PPS request or response from PPSS and PPS0, 0 if invalid */
static unsigned int meson_smc_pps_len(const u8 *pps)
{
	if (pps[0] != 0xff)
		return 0;
	return 3 + hweight8(pps[1] & 0x70);
}

/*
 * OSCam negotiates the transmission parameters itself, by PPS exchanged
 * through write() and read(), and does not tell the driver. Track the first
 * write after the ATR: if it is a PPS request, the response is checked as it
 * is read, and a confirmed PPS1 switches to its Fi and Di (ISO 7816-3 9).
 */
static void meson_smc_pps_request(struct meson_smc *smc, const u8 *buf,
				  size_t len)
{
	u8 pck = 0;
	size_t i;

	if (!smc->pps_allowed)
		return;
	smc->pps_allowed = false;

	if (len < 3 || len > sizeof(smc->pps) ||
	    meson_smc_pps_len(buf) != len)
		return;
	for (i = 0; i < len; i++)
		pck ^= buf[i];
	if (pck)
		return;

	memcpy(smc->pps, buf, len);
	smc->pps_len = len;
	smc->pps_resp_len = 0;
}

static void meson_smc_pps_response(struct meson_smc *smc, const u8 *buf,
				   size_t len)
{
	u8 *resp = smc->pps_resp;
	unsigned int want, i;
	u8 pck = 0;

	while (len && smc->pps_resp_len < sizeof(smc->pps_resp)) {
		resp[smc->pps_resp_len++] = *buf++;
		len--;
		if (smc->pps_resp_len < 2)
			continue;
		want = meson_smc_pps_len(resp);
		if (smc->pps_resp_len == want)
			break;
	}

	if (smc->pps_resp_len < 2)
		return;
	want = meson_smc_pps_len(resp);
	if (smc->pps_resp_len < want)
		return;

	/* complete or invalid: no more tracking */
	smc->pps_len = 0;
	if (!want || smc->pps_resp_len != want)
		return;
	for (i = 0; i < want; i++)
		pck ^= resp[i];
	/* same protocol, PPS1 echoed */
	if (pck || (resp[1] & 0x0f) != (smc->pps[1] & 0x0f) ||
	    !(resp[1] & 0x10) || !(smc->pps[1] & 0x10) ||
	    resp[2] != smc->pps[2])
		return;

	meson_smc_set_fd(smc, resp[2]);
}

/*
 * Read the ATR (ISO 7816-3 8.2). The receiver works in direct convention; a
 * card in inverse convention sends TS = 0x3f, which arrives as 0x03. From
 * then on all bytes in both directions are converted in software, as the
 * vendor driver does.
 */
static int meson_smc_read_atr(struct meson_smc *smc)
{
	u8 *atr = (u8 *)smc->atr.atr;
	unsigned int nhist, y, i = 1;
	bool only_t0 = true;
	int ta1 = -1, ta2 = -1;
	int ret;

	smc->atr.atr_len = 0;
	smc->inverse = false;
	ret = meson_smc_atr_get(smc, 2, 2000);
	if (ret)
		return ret;

	switch (atr[0]) {
	case 0x3b:
		break;
	case 0x03:
		smc->inverse = true;
		meson_smc_convert(smc, atr, 2);
		break;
	default:
		dev_dbg(smc->dev, "invalid TS 0x%02x\n", atr[0]);
		return -EIO;
	}

	/* T0 and each TDi announce the next TAi..TDi in their high nibble */
	nhist = atr[1] & 0x0f;
	y = atr[1] >> 4;
	while (y) {
		unsigned int ta = smc->atr.atr_len;

		ret = meson_smc_atr_get(smc, hweight8(y), 1000);
		if (ret)
			return ret;
		if ((y & BIT(0)) && i == 1)
			ta1 = atr[ta];
		if ((y & BIT(0)) && i == 2)
			ta2 = atr[ta];
		i++;
		if (!(y & BIT(3)))
			break;
		y = atr[smc->atr.atr_len - 1];
		if (y & 0x0f)
			only_t0 = false;
		y >>= 4;
	}

	/* historical bytes, then TCK unless only T=0 is indicated */
	ret = meson_smc_atr_get(smc, nhist + !only_t0, 2000);
	if (ret)
		return ret;

	dev_dbg(smc->dev, "ATR %*ph\n", smc->atr.atr_len, atr);

	/*
	 * Specific mode (TA2 present, b5 = 0: Fi and Di of TA1 apply): use
	 * TA1 right away. Otherwise the card server may negotiate with PPS.
	 */
	if (ta2 >= 0 && !(ta2 & 0x10)) {
		if (ta1 >= 0)
			meson_smc_set_fd(smc, ta1);
	} else {
		smc->pps_allowed = true;
	}
	return 0;
}

/*
 * Cold reset: deactivate the card, activate it again and read its ATR. Like
 * the vendor driver, every reset is a cold reset.
 */
static int meson_smc_reset(struct meson_smc *smc)
{
	int ret;

	if (!meson_smc_card_in(smc))
		return -ENODEV;

	/* the ATR comes with Fd and Dd */
	smc->param.f = meson_smc_default_param.f;
	smc->param.d = meson_smc_default_param.d;
	smc->pps_allowed = false;
	smc->pps_len = 0;

	meson_smc_deactivate(smc);
	usleep_range(200, 250);
	ret = meson_smc_activate(smc);
	if (ret)
		return ret;
	meson_smc_flush(smc);

	/* clock running, receiver off, RST low */
	meson_smc_update_bits(smc, SMC_REG0,
			      SMC_CLK_EN | SMC_ENABLE | SMC_RST_LEVEL,
			      SMC_CLK_EN);
	/* at least 400 clock cycles in reset (ISO 7816-3 6.2.2) */
	usleep_range(2000, 2050);

	meson_smc_write(smc, SMC_INTR,
			SMC_INT_ENABLE(SMC_INT_RECV_THRESHOLD) | SMC_INT_STATUS);
	meson_smc_update_bits(smc, SMC_REG0,
			      SMC_START_ATR_EN | SMC_START_ATR | SMC_ENABLE |
			      SMC_RST_LEVEL,
			      SMC_START_ATR_EN | SMC_START_ATR | SMC_ENABLE |
			      SMC_RST_LEVEL);
	gpiod_set_value_cansleep(smc->reset_gpio, 0);

	ret = meson_smc_read_atr(smc);

	meson_smc_update_bits(smc, SMC_REG0,
			      SMC_START_ATR_EN | SMC_START_ATR, 0);
	return ret;
}

/* Called with slock held */
static void meson_smc_fill_tx(struct meson_smc *smc)
{
	u8 c;

	while (!(meson_smc_read(smc, SMC_STATUS) & SMC_SEND_FIFO_FULL) &&
	       kfifo_get(&smc->tx, &c))
		meson_smc_write(smc, SMC_FIFO, c);
}

/* Called with slock held, returns true if bytes were received */
static bool meson_smc_drain_rx(struct meson_smc *smc)
{
	bool got = false;

	while (!(meson_smc_read(smc, SMC_STATUS) & SMC_RECV_FIFO_EMPTY)) {
		u8 c = meson_smc_read(smc, SMC_FIFO);

		if (!kfifo_put(&smc->rx, c))
			dev_warn_ratelimited(smc->dev, "receive buffer full\n");
		got = true;
	}
	return got;
}

static irqreturn_t meson_smc_irq(int irq, void *data)
{
	struct meson_smc *smc = data;
	u32 intr, enable, status;
	bool wake = false;

	spin_lock(&smc->slock);

	do {
		intr = meson_smc_read(smc, SMC_INTR);
		enable = intr & ~SMC_INT_STATUS;

		wake |= meson_smc_drain_rx(smc);

		if (enable & SMC_INT_ENABLE(SMC_INT_SEND_LAST_BYTE)) {
			meson_smc_fill_tx(smc);
			if (kfifo_is_empty(&smc->tx)) {
				enable &= ~SMC_INT_ENABLE(SMC_INT_SEND_LAST_BYTE);
				wake = true;
			}
		}

		/*
		 * The interrupt is a level (REG2 PULSE_IRQ = 0) until the
		 * status is cleared, which is done after serving the FIFOs.
		 * A byte received or a send FIFO run empty in between would
		 * be lost with the cleared status: check the FIFOs again.
		 */
		meson_smc_write(smc, SMC_INTR,
				enable | (intr & SMC_INT_STATUS));
		status = meson_smc_read(smc, SMC_STATUS);
	} while (!(status & SMC_RECV_FIFO_EMPTY) ||
		 (!kfifo_is_empty(&smc->tx) && (status & SMC_SEND_FIFO_EMPTY)));

	spin_unlock(&smc->slock);

	if (wake)
		wake_up(&smc->wq);

	return IRQ_HANDLED;
}

static struct meson_smc *to_meson_smc(struct file *file)
{
	struct miscdevice *misc = file->private_data;

	return container_of(misc, struct meson_smc, misc);
}

static int meson_smc_open(struct inode *inode, struct file *file)
{
	struct meson_smc *smc = to_meson_smc(file);
	int ret = 0;

	mutex_lock(&smc->lock);
	if (smc->busy)
		ret = -EBUSY;
	else
		smc->busy = true;
	mutex_unlock(&smc->lock);

	return ret;
}

static int meson_smc_release(struct inode *inode, struct file *file)
{
	struct meson_smc *smc = to_meson_smc(file);

	mutex_lock(&smc->lock);
	meson_smc_deactivate(smc);
	smc->busy = false;
	mutex_unlock(&smc->lock);

	return 0;
}

/* Never blocks: -EAGAIN when nothing has been received, as the vendor driver */
static ssize_t meson_smc_read_fop(struct file *file, char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	struct meson_smc *smc = to_meson_smc(file);
	unsigned int len;
	ssize_t ret;

	if (mutex_lock_interruptible(&smc->lock))
		return -ERESTARTSYS;

	if (!meson_smc_card_in(smc)) {
		ret = -ENODEV;
		goto out;
	}

	len = kfifo_out(&smc->rx, smc->buf, min(count, sizeof(smc->buf)));
	if (!len) {
		ret = -EAGAIN;
		goto out;
	}
	meson_smc_convert(smc, smc->buf, len);
	ret = copy_to_user(ubuf, smc->buf, len) ? -EFAULT : len;
	/* switch before the card server sends at the new rate */
	if (smc->pps_len)
		meson_smc_pps_response(smc, smc->buf, len);

out:
	mutex_unlock(&smc->lock);
	return ret;
}

/*
 * Queues the bytes and returns. Bytes of the previous write that still wait
 * for the send FIFO are waited for, or -EAGAIN with O_NONBLOCK (the vendor
 * driver always returns -EAGAIN; OSCam spins on it).
 */
static ssize_t meson_smc_write_fop(struct file *file, const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	struct meson_smc *smc = to_meson_smc(file);
	unsigned long flags;
	ssize_t ret;
	u32 intr;

	if (count > sizeof(smc->buf))
		return -EINVAL;
	if (!count)
		return 0;

	if (mutex_lock_interruptible(&smc->lock))
		return -ERESTARTSYS;

	while (!kfifo_is_empty(&smc->tx)) {
		mutex_unlock(&smc->lock);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible_timeout(smc->wq,
						       kfifo_is_empty(&smc->tx),
						       HZ);
		if (ret < 0)
			return ret;
		if (!ret)
			return -ETIMEDOUT;
		if (mutex_lock_interruptible(&smc->lock))
			return -ERESTARTSYS;
	}

	if (!meson_smc_card_in(smc)) {
		ret = -ENODEV;
		goto out;
	}
	if (copy_from_user(smc->buf, ubuf, count)) {
		ret = -EFAULT;
		goto out;
	}
	meson_smc_pps_request(smc, smc->buf, count);
	meson_smc_convert(smc, smc->buf, count);

	spin_lock_irqsave(&smc->slock, flags);
	kfifo_in(&smc->tx, smc->buf, count);
	meson_smc_fill_tx(smc);
	/* writing 0 to the status bits leaves them alone */
	intr = meson_smc_read(smc, SMC_INTR) & ~SMC_INT_STATUS;
	meson_smc_write(smc, SMC_INTR,
			intr | SMC_INT_ENABLE(SMC_INT_RECV_THRESHOLD |
					      SMC_INT_SEND_LAST_BYTE));
	spin_unlock_irqrestore(&smc->slock, flags);
	ret = count;

out:
	mutex_unlock(&smc->lock);
	return ret;
}

static __poll_t meson_smc_poll(struct file *file, poll_table *wait)
{
	struct meson_smc *smc = to_meson_smc(file);
	__poll_t mask = 0;

	poll_wait(file, &smc->wq, wait);

	if (!kfifo_is_empty(&smc->rx))
		mask |= EPOLLIN | EPOLLRDNORM;
	if (kfifo_is_empty(&smc->tx))
		mask |= EPOLLOUT | EPOLLWRNORM;
	if (!meson_smc_card_in(smc))
		mask |= EPOLLERR;

	return mask;
}

static long meson_smc_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	struct meson_smc *smc = to_meson_smc(file);
	void __user *argp = (void __user *)arg;
	struct am_smc_param param;
	long ret = 0;
	int status;

	if (cmd == AMSMC_IOC_GET_STATUS) {
		status = meson_smc_card_in(smc) ? AMSMC_CARDIN : AMSMC_CARDOUT;
		return put_user(status, (int __user *)argp);
	}

	if (cmd == AMSMC_IOC_SET_PARAM &&
	    copy_from_user(&param, argp, sizeof(param)))
		return -EFAULT;

	if (mutex_lock_interruptible(&smc->lock))
		return -ERESTARTSYS;

	switch (cmd) {
	case AMSMC_IOC_RESET:
		ret = meson_smc_reset(smc);
		if (!ret && copy_to_user(argp, &smc->atr, sizeof(smc->atr)))
			ret = -EFAULT;
		break;
	case AMSMC_IOC_ACTIVE:
		ret = meson_smc_activate(smc);
		break;
	case AMSMC_IOC_DEACTIVE:
		meson_smc_deactivate(smc);
		break;
	case AMSMC_IOC_GET_PARAM:
		if (copy_to_user(argp, &smc->param, sizeof(smc->param)))
			ret = -EFAULT;
		break;
	case AMSMC_IOC_SET_PARAM:
		ret = meson_smc_set_param(smc, &param);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&smc->lock);
	return ret;
}

static const struct file_operations meson_smc_fops = {
	.owner = THIS_MODULE,
	.open = meson_smc_open,
	.release = meson_smc_release,
	.read = meson_smc_read_fop,
	.write = meson_smc_write_fop,
	.poll = meson_smc_poll,
	.unlocked_ioctl = meson_smc_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
	.llseek = noop_llseek,
};

static void meson_smc_ida_free(void *data)
{
	struct meson_smc *smc = data;

	ida_free(&meson_smc_ida, smc->id);
}

static int meson_smc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct reset_control *rst;
	struct meson_smc *smc;
	struct clk *clk;
	int irq, ret;

	smc = devm_kzalloc(dev, sizeof(*smc), GFP_KERNEL);
	if (!smc)
		return -ENOMEM;
	smc->dev = dev;
	mutex_init(&smc->lock);
	spin_lock_init(&smc->slock);
	init_waitqueue_head(&smc->wq);
	INIT_KFIFO(smc->rx);
	INIT_KFIFO(smc->tx);
	smc->param = meson_smc_default_param;

	smc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(smc->base))
		return PTR_ERR(smc->base);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	clk = devm_clk_get_enabled(dev, "pclk");
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk), "failed to get pclk\n");
	clk = devm_clk_get_enabled(dev, "card");
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk),
				     "failed to get card clock\n");
	smc->src_khz = clk_get_rate(clk) / 1000;
	if (!smc->src_khz)
		return dev_err_probe(dev, -EINVAL, "card clock has no rate\n");

	rst = devm_reset_control_get_optional_exclusive(dev, NULL);
	if (IS_ERR(rst))
		return dev_err_probe(dev, PTR_ERR(rst), "failed to get reset\n");
	ret = reset_control_reset(rst);
	if (ret)
		return ret;

	smc->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(smc->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(smc->reset_gpio),
				     "failed to get reset GPIO\n");
	smc->enable_gpio = devm_gpiod_get_optional(dev, "enable",
						   GPIOD_OUT_LOW);
	if (IS_ERR(smc->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(smc->enable_gpio),
				     "failed to get enable GPIO\n");
	smc->detect_gpio = devm_gpiod_get(dev, "detect", GPIOD_IN);
	if (IS_ERR(smc->detect_gpio))
		return dev_err_probe(dev, PTR_ERR(smc->detect_gpio),
				     "failed to get detect GPIO\n");

	/* card clock stopped, all interrupts off */
	ret = meson_smc_hw_setup(smc);
	if (ret)
		return ret;
	meson_smc_write(smc, SMC_INTR, SMC_INT_STATUS);

	ret = devm_request_irq(dev, irq, meson_smc_irq, 0, dev_name(dev), smc);
	if (ret)
		return ret;

	ret = ida_alloc(&meson_smc_ida, GFP_KERNEL);
	if (ret < 0)
		return ret;
	smc->id = ret;
	ret = devm_add_action_or_reset(dev, meson_smc_ida_free, smc);
	if (ret)
		return ret;

	smc->misc.minor = MISC_DYNAMIC_MINOR;
	smc->misc.name = devm_kasprintf(dev, GFP_KERNEL, "smc%d", smc->id);
	if (!smc->misc.name)
		return -ENOMEM;
	smc->misc.fops = &meson_smc_fops;
	smc->misc.parent = dev;
	ret = misc_register(&smc->misc);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, smc);
	return 0;
}

static void meson_smc_remove(struct platform_device *pdev)
{
	struct meson_smc *smc = platform_get_drvdata(pdev);

	misc_deregister(&smc->misc);
	meson_smc_deactivate(smc);
}

static const struct of_device_id meson_smc_of_match[] = {
	{ .compatible = "amlogic,meson-gxbb-smartcard" },
	{ .compatible = "amlogic,meson-g12a-smartcard" },
	{ }
};
MODULE_DEVICE_TABLE(of, meson_smc_of_match);

static struct platform_driver meson_smc_driver = {
	.probe = meson_smc_probe,
	.remove = meson_smc_remove,
	.driver = {
		.name = "meson-smartcard",
		.of_match_table = meson_smc_of_match,
	},
};
module_platform_driver(meson_smc_driver);

MODULE_DESCRIPTION("Amlogic ISO 7816 smart card interface");
MODULE_AUTHOR("Marek Czerski <m.czerski@ap-tech.pl>");
MODULE_LICENSE("GPL");
