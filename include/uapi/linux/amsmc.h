/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Amlogic smart card interface, /dev/smcN
 *
 * This is the interface of the Amlogic vendor driver, kept as is so that
 * existing card servers (OSCam) work unchanged. read() and write() carry the
 * bytes exchanged with the card; neither blocks, poll() tells when data has
 * arrived or the last write has been sent.
 */

#ifndef _UAPI_LINUX_AMSMC_H
#define _UAPI_LINUX_AMSMC_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define AMSMC_MAX_ATR_LEN	33

enum {
	AMSMC_CARDOUT = 0,
	AMSMC_CARDIN = 1
};

/**
 * struct am_smc_atr - answer to reset
 * @atr: the ATR bytes, in direct convention
 * @atr_len: number of valid bytes in @atr
 */
struct am_smc_atr {
	char atr[AMSMC_MAX_ATR_LEN];
	int atr_len;
};

/**
 * struct am_smc_param - transmission parameters
 * @f: clock rate conversion integer F (Fi)
 * @d: baud rate adjustment integer D (Di)
 * @n: extra guard time N
 * @bwi: block waiting time integer (T=1)
 * @cwi: character waiting time integer (T=1)
 * @bgt: block guard time, in ETUs
 * @freq: card clock in kHz
 * @recv_invert: invert the received bits
 * @recv_lsb_msb: receive the most significant bit first
 * @recv_no_parity: do not check the parity of received characters
 * @recv_parity: received characters have odd parity
 * @xmit_invert: invert the sent bits
 * @xmit_lsb_msb: send the most significant bit first
 * @xmit_retries: repetitions of a character the card signals an error for
 * @xmit_repeat_dis: do not repeat characters the card signals an error for
 * @xmit_parity: send characters with odd parity
 */
struct am_smc_param {
	int f;
	int d;
	int n;
	int bwi;
	int cwi;
	int bgt;
	int freq;
	int recv_invert;
	int recv_lsb_msb;
	int recv_no_parity;
	int recv_parity;
	int xmit_invert;
	int xmit_lsb_msb;
	int xmit_retries;
	int xmit_repeat_dis;
	int xmit_parity;
};

#define AMSMC_IOC_MAGIC		'C'

#define AMSMC_IOC_RESET		_IOR(AMSMC_IOC_MAGIC, 0x00, struct am_smc_atr)
#define AMSMC_IOC_GET_STATUS	_IOR(AMSMC_IOC_MAGIC, 0x01, int)
#define AMSMC_IOC_ACTIVE	_IO(AMSMC_IOC_MAGIC, 0x02)
#define AMSMC_IOC_DEACTIVE	_IO(AMSMC_IOC_MAGIC, 0x03)
#define AMSMC_IOC_GET_PARAM	_IOR(AMSMC_IOC_MAGIC, 0x04, struct am_smc_param)
#define AMSMC_IOC_SET_PARAM	_IOW(AMSMC_IOC_MAGIC, 0x05, struct am_smc_param)
#define AMSMC_IOC_HOT_RESET	_IOR(AMSMC_IOC_MAGIC, 0x06, struct am_smc_atr)

#endif /* _UAPI_LINUX_AMSMC_H */
