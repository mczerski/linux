#ifndef __M88TC6800_H__
#define __M88TC6800_H__

#include "media/dvb_frontend.h"

#define M88TC6800_DRIVER_VERSION "1.0.1"

struct m88tc6800_config {
	u8 addr;
	u32 xtal;
	u8 xtal_cap;
	u8 dac_gain;
	u32 dac;
};



extern struct dvb_frontend *m88tc6800_attach(struct dvb_frontend *fe,
					    struct i2c_adapter *i2c,
					    struct m88tc6800_config *cfg);

#endif /* __M88TC6800_H__ */
