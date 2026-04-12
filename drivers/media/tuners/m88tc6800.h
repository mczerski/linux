#ifndef __M88TC6800_H__
#define __M88TC6800_H__

#include "media/dvb_frontend.h"


struct m88tc6800_config {
	u8 addr;
	u32 xtal;
	u8 xtal_cap;
	u8 mode;
	u8 int_lt;
	u8 int_im;
	u8 custom_cfg;
	u8 harmonic_imp;
	u8 dac_gain;
	u32 dac;
};



extern struct dvb_frontend *m88tc6800_attach(struct dvb_frontend *fe,
					    struct i2c_adapter *i2c,
					    struct m88tc6800_config *cfg);

#endif /* __M88TC6800_H__ */
