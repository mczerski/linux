#include "m88tc6800_priv.h"

int m88tc6800_set_reg_bits(struct m88tc6800_priv *priv, u8 reg, u8 val, u8 begin_bit, u8 end_bit)
{
	u8 val_mask = ((1 << (end_bit - begin_bit + 1)) - 1) << begin_bit;
	u8 reg_data;
	int ret;

	ret = m88tc6800_read_reg(priv, reg, &reg_data);
	if (ret)
		return ret;

	reg_data &= ~val_mask;
	val = (val << begin_bit) & val_mask;
	reg_data |= val;

  ret = m88tc6800_write_reg(priv, reg, reg_data);
  return ret;
}

void m88tc6800_preset(struct m88tc6800_priv *priv)
{
    u8 reg_data;

    m88tc6800_write_reg(priv, 0x0b, 0x6d);
    m88tc6800_write_reg(priv, 0x0c, 0xe3);
    m88tc6800_write_reg(priv, 0x0d, 0xc4);
    m88tc6800_read_reg(priv, 0x9a, &reg_data);
    m88tc6800_write_reg(priv, 0x9a, reg_data & 0xf);
    m88tc6800_write_reg(priv, 0x9d, 0x3f);
    m88tc6800_write_reg(priv, 0x31, 0x97);
    m88tc6800_write_reg(priv, 0xab, 0xd9);
    m88tc6800_write_reg(priv, 0xb0, 0x09);
    m88tc6800_write_reg(priv, 0xbd, 0x02);
    m88tc6800_write_reg(priv, 0xb9, 0x99);
    m88tc6800_write_reg(priv, 0x58, 0x11);
    if (priv->config.xtal == 24000) {
        m88tc6800_write_reg(priv, 0x39, 0x77);
        m88tc6800_write_reg(priv, 0x3a, 0x11);
        m88tc6800_write_reg(priv, 0x7e, 0x12);
    }
    else if (priv->config.xtal == 27000) {
        m88tc6800_write_reg(priv, 0x7e, 0x13);
    }
    m88tc6800_write_reg(priv, 0x80, 0x00);
    m88tc6800_write_reg(priv, 0x82, 0x00);
    m88tc6800_write_reg(priv, 0x10, 0x00);
    m88tc6800_write_reg(priv, 0x66, 0x00);
}

void m88tc6800_set_rf_frontend(struct m88tc6800_priv *priv, u32 freq_khz)
{
    m88tc6800_write_reg(priv, 0x88, 0x2b);
    m88tc6800_write_reg(priv, 0x89, 0x2b);
    m88tc6800_write_reg(priv, 0x8a, 0xab);
    m88tc6800_write_reg(priv, 0x39, 0x69);
    if (freq_khz <= 602000) {
        m88tc6800_write_reg(priv, 0x3a, 0x2d);
        m88tc6800_write_reg(priv, 0x8b, 0x55);
    }
    else {
        m88tc6800_write_reg(priv, 0x3a, 0x09);
        m88tc6800_write_reg(priv, 0x8b, 0x11);
    }
    m88tc6800_write_reg(priv, 0xfd, 0x1a);
    if (freq_khz > 112000) {
        if (freq_khz <= 334000) {
            m88tc6800_write_reg(priv, 0x7f, 0x04);
            m88tc6800_write_reg(priv, 0x92, 0x8c);
            m88tc6800_write_reg(priv, 0x87, 0xf0);
            m88tc6800_write_reg(priv, 0x90, 0x49);
        }
        else {
            m88tc6800_write_reg(priv, 0x7f, 0x00);
            m88tc6800_write_reg(priv, 0x92, 0x8c);
            m88tc6800_write_reg(priv, 0x87, 0xf0);
            if (freq_khz <= 451000) {
                m88tc6800_write_reg(priv, 0x90, 0x49);
            }
            else {
                m88tc6800_write_reg(priv, 0x90, 0x4a);
            }
        }
    }
    else {
        m88tc6800_write_reg(priv, 0x7f, 0x08);
        m88tc6800_write_reg(priv, 0x92, 0x8c);
        m88tc6800_write_reg(priv, 0x87, 0xf0);
        m88tc6800_write_reg(priv, 0x90, 0x49);
    }
    m88tc6800_write_reg(priv, 0xaa, 0x84);
    m88tc6800_write_reg(priv, 0xa9, 0xf6);
    m88tc6800_write_reg(priv, 0x98, 0xf8);
    if (freq_khz > 700000 && freq_khz <= 900000) {
        m88tc6800_write_reg(priv, 0x99, 0xa5);
    }
    else {
        m88tc6800_write_reg(priv, 0x99, 0xb6);
    }
    m88tc6800_write_reg(priv, 0x26, 0x11);
    if (freq_khz >= 366000 && freq_khz <= 374000) {
        m88tc6800_write_reg(priv, 0x94, 0x29);
    }
    else if ((freq_khz >= 480000 && freq_khz <= 492000) || (freq_khz >= 610000 && freq_khz <= 624000)) {
        m88tc6800_write_reg(priv, 0x94, 0x15);
    }
    else {
        m88tc6800_write_reg(priv, 0x94, 0x01);
    }
    m88tc6800_write_reg(priv, 0x39, 0x0e);
    m88tc6800_write_reg(priv, 0x3a, 0x04);
    m88tc6800_write_reg(priv, 0x39, 0x0f);
    m88tc6800_write_reg(priv, 0x3a, 0x04);
    m88tc6800_write_reg(priv, 0x39, 0x10);

    if ((freq_khz >= 366000 && freq_khz <= 374000) ||
            (freq_khz >= 480000 && freq_khz <= 492000) ||
            (freq_khz >= 610000 && freq_khz <= 624000) ||
            (freq_khz >= 800000 && freq_khz <= 820000)) {
        m88tc6800_write_reg(priv, 0x3a, 0x02);
    }
    else {
        m88tc6800_write_reg(priv, 0x3a, 0x01);
    }
}

u8 m88tc6800_get_mixer_type(struct m88tc6800_priv *priv, u32 freq_khz)
{
    if (freq_khz <= 334000)
        return M88TC6800_MIXER_DIV;

    /* Bands where an LO harmonic lands in-band and the harmonic-rejection
     * mixer is needed. These are the same four ranges that
     * m88tc6800_set_rf_frontend() retunes reg 0x94 and reg 0x3a for. */
    if ((freq_khz >= 366000 && freq_khz <= 374000) ||
            (freq_khz >= 480000 && freq_khz <= 492000) ||
            (freq_khz >= 610000 && freq_khz <= 624000) ||
            (freq_khz >= 800000 && freq_khz <= 820000))
        return M88TC6800_MIXER_HARMONIC;

    return M88TC6800_MIXER_NORMAL;
}

void m88tc6800_set_mixer(struct m88tc6800_priv *priv, u32 freq_khz, u8 mixer_type)
{
    /* Only M88TC6800_MIXER_DIV and M88TC6800_MIXER_HARMONIC are passed
     * through to the chip; anything else selects the normal mixer. */
    u8 mixer_sel = mixer_type;

    if (mixer_type == M88TC6800_MIXER_DIV) {
        /* Divider mixer: reg 0x2f carries a band code that tracks the LO
         * divider steps chosen by m88tc6800_set_lo() for the same band. */
        if (freq_khz < 125000)
            m88tc6800_write_reg(priv, 0x2f, 0x00);
        else if (freq_khz < 167000)
            m88tc6800_write_reg(priv, 0x2f, 0x10);
        else if (freq_khz < 250000)
            m88tc6800_write_reg(priv, 0x2f, 0x21);
        else if (freq_khz < 335000)
            m88tc6800_write_reg(priv, 0x2f, 0x32);
        else
            m88tc6800_write_reg(priv, 0x2f, 0x43);
    }
    else {
        if (mixer_type != M88TC6800_MIXER_HARMONIC)
            mixer_sel = M88TC6800_MIXER_NORMAL;
        m88tc6800_write_reg(priv, 0x2f, 0x00);
    }

    m88tc6800_set_reg_bits(priv, 0x0b, mixer_sel, 0, 1);

    /* Extra filtering around the 330..400 MHz harmonic trouble spot. */
    if (freq_khz >= 330000 && freq_khz <= 400000)
        m88tc6800_write_reg(priv, 0x32, 0x04);
    else
        m88tc6800_write_reg(priv, 0x32, 0x00);
}

/* One row of an LO divider table: applies to every frequency below
 * below_khz that no earlier row claimed. The last row uses U32_MAX as an
 * open-ended catch-all. */
struct m88tc6800_lo_band {
    u32 below_khz;
    u8 div_cfg;     /* reg 0x14: LO divider / buffer configuration */
    u8 lo_div;      /* LO divider ratio, Fvco = freq * lo_div */
};

/* Divider mixer below 500 MHz. */
static const struct m88tc6800_lo_band m88tc6800_lo_div_mixer[] = {
    {  63000, 0x8b, 0x80 },
    {  83000, 0x7a, 0x60 },
    { 125000, 0x0a, 0x40 },
    { 167000, 0x0d, 0x30 },
    { 250000, 0x09, 0x20 },
    { 335000, 0x0c, 0x18 },
    { 0xffffffff, 0x08, 0x10 },
};

/* Normal mixer, and the harmonic mixer below 250 MHz. Same divider ratios
 * as the table above, but a different buffer configuration and a lower
 * crossover at the bottom end (56 MHz instead of 63 MHz). */
static const struct m88tc6800_lo_band m88tc6800_lo_normal[] = {
    {  56000, 0x0b, 0x80 },
    {  83000, 0x7a, 0x60 },
    { 125000, 0x0a, 0x40 },
    { 167000, 0x79, 0x30 },
    { 250000, 0x09, 0x20 },
    { 335000, 0x78, 0x18 },
    { 500000, 0x08, 0x10 },
    { 667000, 0x74, 0x0c },
    { 0xffffffff, 0x04, 0x08 },
};

/* Harmonic mixer at 250 MHz and above. */
static const struct m88tc6800_lo_band m88tc6800_lo_harmonic[] = {
    { 333000, 0xf8, 0x18 },
    { 500000, 0x88, 0x10 },
    { 666000, 0x74, 0x0c },
    { 0xffffffff, 0x04, 0x08 },
};

void m88tc6800_set_lo(struct m88tc6800_priv *priv, u32 freq_khz, u8 mixer_type)
{
    const struct m88tc6800_lo_band *band;

    if (freq_khz < 500000 && mixer_type == M88TC6800_MIXER_DIV)
        band = m88tc6800_lo_div_mixer;
    else if (freq_khz < 250000 || mixer_type != M88TC6800_MIXER_HARMONIC)
        band = m88tc6800_lo_normal;
    else
        band = m88tc6800_lo_harmonic;

    while (freq_khz >= band->below_khz)
        band++;

    priv->lo_div = band->lo_div;
    priv->fvco_target = freq_khz * priv->lo_div;

    /* reg 0x13[5:3]: VCO gain (kvco) tuning, stepped at the 6.7 GHz
     * boundary of the tuning range. This is the direct register 0x13, not
     * the indexed sub-register 0x13 that m88tc6800_set_pll() writes the
     * charge pump code to. */
    if (priv->fvco_target < 6700000)
        m88tc6800_set_reg_bits(priv, 0x13, 4, 3, 5);
    else
        m88tc6800_set_reg_bits(priv, 0x13, 7, 3, 5);

    m88tc6800_write_reg(priv, 0x14, band->div_cfg);
}

/* ------------------------------------------------------------------ *
 * PLL
 *
 * The VCO runs at fvco = pll_ref * FDIV. FDIV is fixed point: a 14-bit
 * integer part (FDIV_N) and a 12-bit fraction (FDIV_F) fed to a
 * delta-sigma modulator. m88tc6800_set_lo() has already chosen the LO
 * divider, and so the target fvco in priv->fvco_target.
 *
 * Choosing the reference is a spur problem. With a 27 MHz crystal a
 * target close to a multiple of the crystal is better served by an 18 MHz
 * reference plus DSM spur optimisation than by the 54 MHz default; with a
 * 24 MHz crystal the 334..340 MHz LO band wants 24 MHz instead of 48 MHz.
 *
 * Everything here goes through the indexed bank at reg 0x4e/0x4f.
 * ------------------------------------------------------------------ */

/* A fraction this close to 0 or 1 (in units of 1/4096) is rounded to an
 * integer ratio instead of being dithered by the modulator. */
#define M88TC6800_SDM_MIN_FRAC	0x21

/* FDIV = fvco_target * 16 / pll_ref, split into an integer part and a 12-bit
 * fraction. The reference is pre-divided rather than the target multiplied
 * up so the intermediate stays inside 32 bits; scale compensates for
 * references that 16 does not divide evenly. */
static void m88tc6800_calc_fdiv(u32 fvco_target, u32 ref_step, u32 scale,
				u32 *fdiv_n, u32 *fdiv_f)
{
	u32 num = fvco_target * scale;

	*fdiv_n = num / ref_step;
	*fdiv_f = ((num % ref_step) << 12) / ref_step;
}

/* Pick the PLL reference and compute FDIV for it. Returns the reference in
 * kHz; *spur_opti is the DSM spur-optimisation level, which later divides
 * the charge pump code down (0 = none, 1 = halve, 2 = quarter). */
static u32 m88tc6800_select_pll_ref(struct m88tc6800_priv *priv, u8 *spur_opti,
				    u32 *fdiv_n, u32 *fdiv_f)
{
	u32 fvco_target = priv->fvco_target;
	u32 crystal = priv->config.xtal;
	u32 pll_ref;

	*spur_opti = 0;

	if (crystal == 24000) {
		u32 freq_lo_khz = fvco_target / priv->lo_div;

		if (freq_lo_khz >= 334001 && freq_lo_khz <= 339999)
			pll_ref = 24000;
		else
			pll_ref = 48000;
	}
	else if (crystal == 27000) {
		u32 nearest = ((fvco_target + 13500) / 27000) * 27000;
		u32 dist = fvco_target > nearest ? fvco_target - nearest : nearest - fvco_target;

		/* Right on a crystal multiple, or far enough away from one,
		 * the plain 54 MHz reference is fine. In between, drop to
		 * 18 MHz and let the modulator spread the spur. */
		if (dist > 10 && dist < 1010) {
			pll_ref = 18000;
			*spur_opti = 2;
		}
		else if (dist > 1020 && dist < 2020) {
			pll_ref = 18000;
			*spur_opti = 1;
		}
		else {
			pll_ref = 54000;
		}
	}
	else {
		pll_ref = crystal * 2;

		/* References that are not a whole multiple of 16 kHz need the
		 * divider computed at a finer scale to stay exact. */
		if (pll_ref == 9000 || pll_ref == 27000) {
			m88tc6800_calc_fdiv(fvco_target, pll_ref / 8, 2, fdiv_n, fdiv_f);
			return pll_ref;
		}
		if (pll_ref == 13500) {
			m88tc6800_calc_fdiv(fvco_target, pll_ref / 4, 4, fdiv_n, fdiv_f);
			return pll_ref;
		}
	}

	m88tc6800_calc_fdiv(fvco_target, pll_ref / 16, 1, fdiv_n, fdiv_f);
	return pll_ref;
}

void m88tc6800_set_pll(struct m88tc6800_priv *priv)
{
	u32 crystal = priv->config.xtal;
	u32 pll_ref, fdiv_n, fdiv_f, frac;
	u32 fdiv_n_hi, fdiv_n_lo;
	u32 fvco_khz, frac_khz;
	u32 icp_raw, icp_bits, icp_hi, icp_code;
	u8 fdiv_f_hi, fdiv_f_lo, fdiv_n_lo_bits, icp_clamped;
	u8 spur_opti;
	bool sdm_en;
	s32 icp;

	pll_ref = m88tc6800_select_pll_ref(priv, &spur_opti, &fdiv_n, &fdiv_f);

	/* FDIV_N is split 10 + 4 bits: the upper part across regs 0x17/0x18,
	 * the lower nibble into reg 0x19. */
	fdiv_n_hi = (fdiv_n >> 4) & 0x3ff;
	fdiv_n_lo = fdiv_n & 0xf;

	frac = fdiv_f & 0xfff;
	sdm_en = frac >= M88TC6800_SDM_MIN_FRAC &&
		 frac <= 0x1000 - M88TC6800_SDM_MIN_FRAC;

	if (sdm_en) {
		fdiv_f_lo = fdiv_f;				/* reg 0x1b */
		fdiv_f_hi = ((fdiv_f >> 8) & 0xf) | 0x30;	/* reg 0x1a */
		frac_khz = pll_ref * frac >> 16;
	}
	else {
		/* Round FDIV_N to nearest and hand the PLL an integer ratio. */
		if (fdiv_f & 0x800) {
			fdiv_n_hi = ((fdiv_n + 1) >> 4) & 0x3ff;
			fdiv_n_lo = (fdiv_n + 1) & 0xf;
		}
		fdiv_f_lo = 0x00;
		fdiv_f_hi = 0x30;
		frac_khz = 0;
	}

	/* VCO frequency the divider actually realises, which is what the
	 * charge pump current has to be matched to. */
	fvco_khz = pll_ref * fdiv_n_hi + frac_khz + (pll_ref * fdiv_n_lo >> 4);

	/* Charge pump code, linear in the VCO frequency: one step per
	 * 40625 kHz, crossing zero at 8 GHz. */
	icp = (32000000 - 4 * (s32)fvco_khz) / 40625;
	icp_bits = (u32)icp;
	icp_raw = icp_bits + 0x100;

	/* icp_raw is consumed at a different fixed-point scale per reference
	 * ratio; the truncating masks come from the original's shifts. */
	icp_hi = (icp_raw & 0x1fff) >> 5;
	icp_code = ((icp_hi & 1) + (icp_raw >> 6)) & 0xff;

	/* Prescaler setup, selected by how the reference relates to the
	 * crystal. Only the first ratio and the x2/3 one below are reachable
	 * with the 24 and 27 MHz crystals the driver supports; the /3 and /2
	 * cases are carried over from the original unchanged. */
	if (pll_ref == crystal) {
		icp_code = (icp_hi + ((icp_bits >> 4) & 1)) & 0xff;
		m88tc6800_write_reg(priv, 0x4e, 0x11);
		m88tc6800_write_reg(priv, 0x4f, 0x11);
		m88tc6800_write_reg(priv, 0x4e, 0x10);
		m88tc6800_write_reg(priv, 0x4f, 0x08);
		m88tc6800_write_reg(priv, 0x4e, 0x15);
		m88tc6800_write_reg(priv, 0x4f, 0x13);
	}
	else if (pll_ref == crystal * 2 / 3) {
		icp_code = (icp_code + icp_hi + ((icp_bits >> 4) & 1)) & 0xff;
		m88tc6800_write_reg(priv, 0x4e, 0x11);
		m88tc6800_write_reg(priv, 0x4f, 0x33);
		m88tc6800_write_reg(priv, 0x4e, 0x10);
		m88tc6800_write_reg(priv, 0x4f, 0x0b);
		m88tc6800_write_reg(priv, 0x4e, 0x15);
		m88tc6800_write_reg(priv, 0x4f, 0x43);
	}
	else if (pll_ref == crystal / 3) {
		u32 icp_hi4 = (icp_raw & 0xfff) >> 4;

		icp_code = (icp_hi + icp_hi4 + ((icp_bits >> 3) & 1) +
			    (icp_hi4 & 1)) & 0xff;
		m88tc6800_write_reg(priv, 0x4e, 0x11);
		m88tc6800_write_reg(priv, 0x4f, 0x33);
		m88tc6800_write_reg(priv, 0x4e, 0x10);
		m88tc6800_write_reg(priv, 0x4f, 0x08);
		m88tc6800_write_reg(priv, 0x4e, 0x15);
		m88tc6800_write_reg(priv, 0x4f, 0x63);
	}
	else if (pll_ref == crystal / 2) {
		icp_code = ((icp_raw >> 4) + ((icp_bits >> 3) & 1)) & 0xff;
		m88tc6800_write_reg(priv, 0x4e, 0x11);
		m88tc6800_write_reg(priv, 0x4f, 0x22);
		m88tc6800_write_reg(priv, 0x4e, 0x10);
		m88tc6800_write_reg(priv, 0x4f, 0x08);
		m88tc6800_write_reg(priv, 0x4e, 0x15);
		m88tc6800_write_reg(priv, 0x4f, 0x13);
	}
	else {
		m88tc6800_write_reg(priv, 0x4e, 0x11);
		m88tc6800_write_reg(priv, 0x4f, 0x11);
		m88tc6800_write_reg(priv, 0x4e, 0x10);
		m88tc6800_write_reg(priv, 0x4f, 0x0b);
		m88tc6800_write_reg(priv, 0x4e, 0x15);
		m88tc6800_write_reg(priv, 0x4f, 0x13);
	}

	/* Sub-register 0x12: ICP scaling, enabled only in fractional mode. */
	m88tc6800_write_reg(priv, 0x4e, 0x12);
	if (sdm_en) {
		icp_code = (icp_code - 1) & 0xff;
		m88tc6800_write_reg(priv, 0x4f, 0x21);
	}
	else {
		m88tc6800_write_reg(priv, 0x4f, 0x00);
	}

	/* Spur optimisation pays for the spread spectrum with loop gain. */
	if (spur_opti == 1)
		icp_code >>= 1;
	else if (spur_opti == 2)
		icp_code >>= 2;

	if (icp_code < 2)
		icp_code = 2;
	icp_clamped = icp_code > 14 ? 0xf : icp_code;

	m88tc6800_write_reg(priv, 0x4e, 0x0a);
	m88tc6800_write_reg(priv, 0x4f, 0x63);
	m88tc6800_write_reg(priv, 0x4e, 0x0f);
	m88tc6800_write_reg(priv, 0x4f, 0x3d);
	m88tc6800_write_reg(priv, 0x4e, 0x0b);
	m88tc6800_write_reg(priv, 0x4f, 0xa1);
	m88tc6800_write_reg(priv, 0x3d, 0xff);
	m88tc6800_write_reg(priv, 0x4e, 0x1c);
	m88tc6800_write_reg(priv, 0x4f, 0x7e);

	/* Sub-register 0x19: FDIV_N[3:0] in bits [6:3], modulator enable in
	 * bit 0, VCO calibration strobe in bit 2. Written twice to raise the
	 * strobe once the divider is in place. */
	fdiv_n_lo_bits = fdiv_n_lo << 3;
	m88tc6800_write_reg(priv, 0x4e, 0x19);
	m88tc6800_write_reg(priv, 0x4f, fdiv_n_lo_bits | (sdm_en ? 3 : 2));
	m88tc6800_write_reg(priv, 0x4e, 0x19);
	m88tc6800_write_reg(priv, 0x4f, fdiv_n_lo_bits | (sdm_en ? 7 : 6));

	m88tc6800_write_reg(priv, 0x4e, 0x17);
	m88tc6800_write_reg(priv, 0x4f, (fdiv_n_hi >> 8) | 0x0c);
	m88tc6800_write_reg(priv, 0x4e, 0x18);
	m88tc6800_write_reg(priv, 0x4f, fdiv_n_hi);
	m88tc6800_write_reg(priv, 0x4e, 0x1a);
	m88tc6800_write_reg(priv, 0x4f, fdiv_f_hi);
	m88tc6800_write_reg(priv, 0x4e, 0x1b);
	m88tc6800_write_reg(priv, 0x4f, fdiv_f_lo);
	m88tc6800_write_reg(priv, 0x4e, 0x0e);
	m88tc6800_write_reg(priv, 0x4f, 0x58);
	m88tc6800_write_reg(priv, 0x4e, 0x14);
	m88tc6800_write_reg(priv, 0x4f, 0x4c);
	m88tc6800_write_reg(priv, 0x4e, 0x13);
	m88tc6800_write_reg(priv, 0x4f, icp_clamped | 0x10);
}

void m88tc6800_set_bandwidth(struct m88tc6800_priv *priv, u32 bandwidth_hz)
{
    if (bandwidth_hz == 1700) {
        m88tc6800_write_reg(priv, 0x59, 0x09);
        m88tc6800_write_reg(priv, 0x29, 0xae);
        m88tc6800_write_reg(priv, 0x86, 0x3c);
    }
    else if (bandwidth_hz == 6000) {
        m88tc6800_write_reg(priv, 0x59, 0x0a);
        m88tc6800_write_reg(priv, 0x29, 0xaa);
        m88tc6800_write_reg(priv, 0x86, 0x12);
    }
    else if (bandwidth_hz == 7000) {
        m88tc6800_write_reg(priv, 0x59, 0x0b);
        m88tc6800_write_reg(priv, 0x29, 0xa8);
        m88tc6800_write_reg(priv, 0x86, 0x12);
    }
    else if (bandwidth_hz == 10000) {
        m88tc6800_write_reg(priv, 0x59, 0x0d);
        m88tc6800_write_reg(priv, 0x29, 0xa6);
        m88tc6800_write_reg(priv, 0x86, 0x12);
    }
    else {
        m88tc6800_write_reg(priv, 0x59, 0x0c);
        m88tc6800_write_reg(priv, 0x29, 0xa8);
        m88tc6800_write_reg(priv, 0x86, 0x12);
    }
}

/* Calibration channels. The ADC spur search is skipped entirely for the
 * first one. */
#define M88TC6800_CAL_FREQ_KHZ		997375
#define M88TC6800_CAL_FREQ_27M_KHZ	498377

/* Minimum distance a channel must keep from an ADC spur, in kHz. */
#define M88TC6800_SPUR_GUARD_KHZ	4001

/* One ADC clock candidate.
 *
 * The selector is written to indexed sub-register 6 *before* the candidate
 * is tested, so a rejected candidate still leaves its write in the I2C
 * stream. That is deliberate and is what the comparison against
 * original.c checks, so the writes must not be hoisted out of the search.
 */
struct m88tc6800_adc_clock {
	u8 sel;			/* indexed sub-register 6 selector */
	u32 spur_spacing_khz;	/* spur spacing to clear, 0 = last resort */
	u32 fadc_khz;
};

/* Candidates in preference order. The first entry is the nominal 108 MHz
 * clock; the rest trade a less convenient clock for a clear spectrum. */
static const struct m88tc6800_adc_clock m88tc6800_adc_clocks_24m[] = {
	{ 0x0d, 27000, 108000 },
	{ 0x12, 30000, 120000 },
	{ 0x16, 32400, 129600 },
	{ 0x0f, 28200, 112800 },
	{ 0x11,     0, 117600 },
};

static const struct m88tc6800_adc_clock m88tc6800_adc_clocks_27m[] = {
	{ 0x08, 27000, 108000 },
	{ 0x0c, 29700, 118800 },
	{ 0x10, 32400, 129600 },
	{ 0x0a, 28350, 113400 },
	{ 0x0b,     0, 116100 },
};

/* The ADC clock produces spurs at multiples of spur_spacing_khz. A channel
 * is clear when it stays at least M88TC6800_SPUR_GUARD_KHZ away from the
 * nearest multiple on either side.
 *
 * The subtraction is deliberately unsigned: a channel sitting below the
 * lower guard wraps to a large value and so fails the comparison, which is
 * exactly the rejection the hardware wants.
 */
static bool m88tc6800_adc_clock_is_clear(u32 freq_khz, u32 spur_spacing_khz)
{
	u32 offset_khz = freq_khz % spur_spacing_khz;
	u32 window_khz = spur_spacing_khz - 2 * M88TC6800_SPUR_GUARD_KHZ + 1;

	return offset_khz - M88TC6800_SPUR_GUARD_KHZ < window_khz;
}

/* Pick the ADC sampling clock, writing each candidate's selector as it is
 * tried, and return the chosen rate in kHz. */
static u32 m88tc6800_select_adc_clock(struct m88tc6800_priv *priv, u32 freq_khz)
{
	const struct m88tc6800_adc_clock *clocks;
	int i;

	if (priv->config.xtal == 24000)
		clocks = m88tc6800_adc_clocks_24m;
	else if (priv->config.xtal == 27000)
		clocks = m88tc6800_adc_clocks_27m;
	else
		return 108000;

	/* The preferred candidate's selector is written unconditionally. */
	m88tc6800_write_reg(priv, 0x4e, 0x06);
	m88tc6800_write_reg(priv, 0x4f, clocks[0].sel);

	if (freq_khz == M88TC6800_CAL_FREQ_KHZ)
		return 108000;

	for (i = 0; ; i++) {
		if (i > 0) {
			m88tc6800_write_reg(priv, 0x4e, 0x06);
			m88tc6800_write_reg(priv, 0x4f, clocks[i].sel);
		}
		if (clocks[i].spur_spacing_khz == 0 ||
		    m88tc6800_adc_clock_is_clear(freq_khz, clocks[i].spur_spacing_khz))
			break;
	}

	if (priv->config.xtal == 27000 && freq_khz == M88TC6800_CAL_FREQ_27M_KHZ) {
		m88tc6800_write_reg(priv, 0x4e, 0x06);
		m88tc6800_write_reg(priv, 0x4f, 0x04);
	}

	return clocks[i].fadc_khz;
}

void m88tc6800_set_dac(struct m88tc6800_priv *priv, u32 freq_khz, u32 bandwidth_khz)
{
	u32 dac_khz = priv->config.dac;
	u32 fadc_khz, fadc_hz;
	u32 nco_quot, nco_rem, nco_inc;
	u32 fc_code, fc_offset;
	u32 resamp_num, resamp_quot, resamp_rem;
	u8 flt_bit;

	/* reg 0x22: anti-alias filter corner (bit 6/7, picked from the DAC
	 * rate), output gain in bits [5:3], fixed low nibble. */
	flt_bit = dac_khz < 4501 ? 0x40 : 0x80;
	m88tc6800_write_reg(priv, 0x22, flt_bit + priv->config.dac_gain * 8 + 0x05);
	m88tc6800_set_reg_bits(priv, 0x20, 0x3, 0x5, 0x6);

	fadc_khz = m88tc6800_select_adc_clock(priv, freq_khz);
	fadc_hz = fadc_khz * 1000;

	/* regs 0xfa/0xfb: NCO phase increment, (f_dac / f_adc) * 2^16. Split
	 * into quotient and remainder so the intermediate stays in 32 bits. */
	nco_quot = (dac_khz * 512000) / fadc_hz;
	nco_rem = (dac_khz * 512000) % fadc_hz;
	nco_inc = (nco_quot * 32 + (nco_rem * 32) / fadc_hz) * 4;
	m88tc6800_write_reg(priv, 0xfa, nco_inc >> 8);
	m88tc6800_write_reg(priv, 0xfb, nco_inc);

	/* regs 0xe2/0xe3: low-pass cutoff code -- half the channel bandwidth
	 * less a 100 kHz margin, rescaled from the nominal 108 MHz ADC clock
	 * to the one actually selected above. */
	fc_code = ((bandwidth_khz / 2 - 100) * 108000) / fadc_khz;

	/* regs 0xe0/0xe1 and 0xe4/0xe5: resampler step (4096 - fc) / fc in
	 * 15-bit fixed point, as a quotient plus the division remainder. */
	resamp_num = (0x1000 - fc_code) * 0x8000;
	resamp_quot = resamp_num / fc_code;
	resamp_rem = resamp_num % fc_code;

	m88tc6800_write_reg(priv, 0xe0, (resamp_rem >> 8) & 0xf);
	m88tc6800_write_reg(priv, 0xe1, resamp_rem);
	m88tc6800_write_reg(priv, 0xe2, fc_code >> 8);
	m88tc6800_write_reg(priv, 0xe3, fc_code);
	m88tc6800_write_reg(priv, 0xe4, (resamp_quot & 0xffff) >> 8);
	m88tc6800_write_reg(priv, 0xe5, resamp_quot);

	m88tc6800_write_reg(priv, 0xef, 0x00);
	m88tc6800_write_reg(priv, 0xf0, 0x00);
	m88tc6800_write_reg(priv, 0xf1, 0x08);

	/* regs 0xf3/0xf4: cutoff expressed as a signed offset from mid-scale. */
	fc_offset = (fc_code << 15 >> 11) - 0x8000;
	m88tc6800_write_reg(priv, 0xf2, 0x00);
	m88tc6800_write_reg(priv, 0xf3, fc_offset >> 8);
	m88tc6800_write_reg(priv, 0xf4, fc_offset);
}

int m88tc6800_set_freq(struct m88tc6800_priv *priv, u32 freq_khz, u32 bandwidth_hz)
{
    u8 reg_data;

    m88tc6800_read_reg(priv, 0x3e, &reg_data);
    reg_data = (reg_data & 0x1e) >> 1;
    if (reg_data != 0) {
        m88tc6800_set_reg_bits(priv, 0x3c, 0, 7, 7);
        m88tc6800_set_reg_bits(priv, 0x3e, 0, 1, 4);
    }

    m88tc6800_read_reg(priv, 0x07, &reg_data);
    if (reg_data & 0x8) {
        m88tc6800_write_reg(priv, 0x07, 0x0d);
        m88tc6800_write_reg(priv, 0x11, 0x00);
    }
    else {
        m88tc6800_write_reg(priv, 0x0c, 0x63);
        m88tc6800_write_reg(priv, 0x07, 0x0d);
        m88tc6800_write_reg(priv, 0x0c, 0xe3);
        m88tc6800_write_reg(priv, 0x11, 0x00);
    }

    m88tc6800_read_reg(priv, 0x3c, &reg_data);
    m88tc6800_write_reg(priv, 0x45, 0x5d);
    m88tc6800_write_reg(priv, 0x04, 0x7f);
    m88tc6800_write_reg(priv, 0x05, 0xd8);
    m88tc6800_preset(priv);
    m88tc6800_set_rf_frontend(priv, freq_khz);
    m88tc6800_write_reg(priv, 0x05, 0xf8);
    m88tc6800_set_mixer(priv, freq_khz, m88tc6800_get_mixer_type(priv, freq_khz));
    m88tc6800_set_lo(priv, freq_khz, m88tc6800_get_mixer_type(priv, freq_khz));
    m88tc6800_set_pll(priv);
    m88tc6800_set_bandwidth(priv, bandwidth_hz);
    m88tc6800_write_reg(priv, 0x04, 0x00);
    m88tc6800_write_reg(priv, 0x05, 0x00);
    m88tc6800_read_reg(priv, 0x3e, &reg_data); // TODO: not needed
    m88tc6800_set_dac(priv, freq_khz, bandwidth_hz);
    m88tc6800_write_reg(priv, 0xc9, 0x05);
    m88tc6800_write_reg(priv, 0x40, 0x1a);
    m88tc6800_write_reg(priv, 0x41, 0x00);
    if (priv->config.xtal == 24000) {
        m88tc6800_write_reg(priv, 0x44, 0x22);
    }
    else if (priv->config.xtal == 27000) {
        m88tc6800_write_reg(priv, 0x44, 0x2a);
    }
    m88tc6800_write_reg(priv, 0x60, 0x34);
    m88tc6800_write_reg(priv, 0x05, 0x04);
    m88tc6800_write_reg(priv, 0xc2, 0x01);
    m88tc6800_write_reg(priv, 0x00, 0x01);
    m88tc6800_write_reg(priv, 0x00, 0x00);
    m88tc6800_write_reg(priv, 0xc2, 0x00);
    m88tc6800_write_reg(priv, 0x05, 0x00);
    m88tc6800_write_reg(priv, 0x39, 0x00);
    m88tc6800_write_reg(priv, 0x3a, 0x00);
    m88tc6800_write_reg(priv, 0x0d, 0xc4);
    m88tc6800_write_reg(priv, 0x3d, 0xfe);
    {
        /* regs 0x4a/0x4b hold the 12-bit channel counter latched by the
         * tuning above; reg 0x4c[6:5] is the range it was counted on and
         * says how far to shift it down. */
        u32 ccval_h, ccval_l, ccval, freq_ch_khz;

        m88tc6800_read_reg(priv, 0x4a, &reg_data);
        ccval_h = reg_data;
        m88tc6800_read_reg(priv, 0x4b, &reg_data);
        ccval_l = reg_data;
        m88tc6800_read_reg(priv, 0x4c, &reg_data);
        ccval = (((ccval_h & 0xf) << 8) + ccval_l) >> ((reg_data >> 5) & 3);

        /* The counter steps in thirds of the crystal frequency. */
        if (priv->config.xtal == 27000)
            freq_ch_khz = ccval * 9000;
        else
            freq_ch_khz = ccval * 8000;

        /* If what was actually tuned sits within 20 MHz of the wanted
         * channel, or of its third harmonic, enable the interference
         * mitigation in reg 0x10. Each pair of subtractions is unsigned,
         * so only the non-wrapping one can pass. */
        if (freq_ch_khz - freq_khz <= 20000 ||
                freq_khz - freq_ch_khz <= 20000 ||
                freq_ch_khz - 3 * freq_khz <= 20000 ||
                3 * freq_khz - freq_ch_khz <= 20000)
            m88tc6800_write_reg(priv, 0x10, 0x50);
    }
    m88tc6800_write_reg(priv, 0x66, 0x48);
    m88tc6800_write_reg(priv, 0xb0, 0x0b);
    m88tc6800_write_reg(priv, 0x60, 0x34);
    return 0;
}

static uint32_t xm_list_dBm[] = {
       0x0,           0x0,         0x25A,         0x3BA,
     0x4B4,         0x576,         0x614,         0x69A,
     0x70E,         0x774,         0x7D0,         0x823,
     0x86E,         0x8B4,         0x8F4,         0x930,
     0x968,         0x99D,         0x9CF,         0x9FE,
     0xA2A,         0xA54,         0xA7D,         0xAA3,
     0xAC8,         0xAEC,         0xB0E,         0xB2F,
     0xB4E,         0xB6D,         0xB8A,         0xBA7,
     0xBC2,         0xBDD,         0xBF7,         0xC10,
     0xC29,         0xC40,         0xC58,         0xC6E,
     0xC84,         0xC9A,         0xCAE,         0xCC3,
     0xCD7,         0xCEA,         0xCFE,         0xD10,
     0xD22,         0xD34,         0xD46,         0xD57,
     0xD68,         0xD79,         0xD89,         0xD99,
     0xDA8,         0xDB8,         0xDC7,         0xDD6,
     0xDE4,         0xDF3,         0xE01,         0xE0F,
     0xE1C,         0xE2A,         0xE37,         0xE44,
     0xE51,         0xE5E,         0xE6A,         0xE77,
     0xE83,         0xE8F,         0xE9A,         0xEA6,
     0xEB2,         0xEBD,         0xEC8,         0xED3,
     0xEDE,         0xEE9,         0xEF4,         0xEFE,
     0xF09,         0xF13,         0xF1D,         0xF27,
     0xF31,         0xF3B,         0xF44,         0xF4E,
     0xF58,         0xF61,         0xF6A,         0xF73,
     0xF7D,         0xF86,         0xF8E,         0xF97,
     0xFA0,         0xFA9,         0xFB1,         0xFBA,
     0xFC2,         0xFCA,         0xFD3,         0xFDB,
     0xFE3,         0xFEB,         0xFF3,         0xFFB,
    0x1002,        0x100A,        0x1012,        0x1019,
    0x1021,        0x1028,        0x1030,        0x1037,
    0x103E,        0x1046,        0x104D,        0x1054,
    0x105B,        0x1062,        0x1069,        0x1070,
    0x1076,        0x107D,        0x1084,        0x108B,
    0x1091,        0x1098,        0x109E,        0x10A5,
    0x10AB,        0x10B1,        0x10B8,        0x10BE,
    0x10C4,        0x10CA,        0x10D1,        0x10D7,
    0x10DD,        0x10E3,        0x10E9,        0x10EF,
    0x10F5,        0x10FA,        0x1100,        0x1106,
    0x110C,        0x1111,        0x1117,        0x111D,
    0x1122,        0x1128,        0x112D,        0x1133,
    0x1138,        0x113E,        0x1143,        0x1148,
    0x114E,        0x1153,        0x1158,        0x115D,
    0x1163,        0x1168,        0x116D,        0x1172,
    0x1177,        0x117C,        0x1181,        0x1186,
    0x118B,        0x1190,        0x1195,        0x119A,
    0x119F,        0x11A3,        0x11A8,        0x11AD,
    0x11B2,        0x11B6,        0x11BB,        0x11C0,
    0x11C4,        0x11C9,        0x11CE,        0x11D2,
    0x11D7,        0x11DB,        0x11E0,        0x11E4,
    0x11E9,        0x11ED,        0x11F1,        0x11F6,
    0x11FA,        0x11FE,        0x1203,        0x1207,
    0x120B,        0x1210,        0x1214,        0x1218,
    0x121C,        0x1220,        0x1224,        0x1229,
    0x122D,        0x1231,        0x1235,        0x1239,
    0x123D,        0x1241,        0x1245,        0x1249,
    0x124D,        0x1251,        0x1255,        0x1259,
    0x125C,        0x1260,        0x1264,        0x1268,
    0x126C,        0x1270,        0x1273,        0x1277,
    0x127B,        0x127F,        0x1282,        0x1286,
    0x128A,        0x128D,        0x1291,        0x1295,
    0x1298,        0x129C,        0x12A0,        0x12A3,
    0x12A7,        0x12AA,        0x12AE,        0x12B1,
    0x12B5,        0x12B8,        0x12BC,        0x12BF,
    0x12C3,        0x12C6,        0x12CA,        0x12CD
};

/* Front-end gain reference per band, in units of 0.1 dB. Added to the gain
 * setting read back from reg 0x63 to get the gain actually applied. */
struct m88tc6800_gain_band {
	u32 below_khz;
	s32 gain_ref_x10;
};

static const struct m88tc6800_gain_band m88tc6800_gain_bands[] = {
	{    110001, -315 },
	{    200001, -300 },
	{    334001, -280 },
	{    400001, -340 },
	{    600001, -320 },
	{    700001, -305 },
	{    800001, -300 },
	{    900001, -310 },
	{ 0xffffffff, -340 },
};

/* Read the RF level in dBm.
 *
 * reg 0xc8 is the power detector reading, which indexes xm_list_dBm[] to
 * give a detected power in 0.01 dB. Subtracting the gain the front end is
 * currently applying turns that into the level at the antenna. The gain is
 * reconstructed from reg 0x63 plus three corrections: a per-band
 * reference, the LNA/mixer configuration reported by reg 0x8b, and the
 * four harmonic bands where m88tc6800_set_rf_frontend() leaves the front
 * end detuned.
 *
 * Everything is carried in 0.1 dB units (the vendor's _x10 convention)
 * until the final division.
 */
int m88tc6800_get_rf_power(struct m88tc6800_priv *priv, s32 *strength)
{
	const struct m88tc6800_gain_band *band = m88tc6800_gain_bands;
	u32 freq_khz = priv->frequency_hz / 1000;
	s32 gain_x10, level;
	u8 gain, front_end_cfg, power_det;

	while (freq_khz >= band->below_khz)
		band++;

	m88tc6800_read_reg(priv, 0x63, &gain);
	m88tc6800_read_reg(priv, 0x8b, &front_end_cfg);
	front_end_cfg &= 0x77;

	gain_x10 = gain * 10 + band->gain_ref_x10;

	/* reg 0x8b holds the LNA/mixer drive set by m88tc6800_set_rf_frontend():
	 * 0x55 below 602 MHz, 0x11 above it. Above 900 MHz no correction is
	 * applied at all. */
	if (freq_khz < 900001) {
		if (front_end_cfg == 0x55)
			gain_x10 -= 20;
		else if (front_end_cfg == 0x11)
			gain_x10 -= freq_khz < 800001 ? 40 : 25;
	}

	/* The bands where the harmonic mixer is selected and reg 0x94 is
	 * retuned -- see m88tc6800_get_mixer_type(). */
	if (freq_khz >= 480000 && freq_khz <= 492000)
		gain_x10 += 50;
	else if (freq_khz >= 610000 && freq_khz <= 624000)
		gain_x10 += 60;
	else if (freq_khz >= 800000 && freq_khz <= 820000)
		gain_x10 += 10;
	else if (freq_khz >= 366001 && freq_khz <= 374000)
		gain_x10 += 60;

	m88tc6800_read_reg(priv, 0xc8, &power_det);

	/* xm_list_dBm[] is in 0.01 dB, 0x1a9 is the detector reference. */
	level = (s32)(xm_list_dBm[power_det] / 10 - 0x1a9 - (u32)gain_x10) / 10;

	*strength = level < -107 ? -107 : level;
	return 0;
}
