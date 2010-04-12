#ifndef _OR32_TERMIOS_H
#define _OR32_TERMIOS_H

#include <asm/termbits.h>
#include <asm/ioctls.h>

#define TIOCM_MODEM_BITS       TIOCM_OUT2      /* IRDA support */

/* ioctl (fd, TIOCSERGETLSR, &result) where result may be as below */

/* line disciplines */
#define N_TTY		0
#define N_SLIP		1
#define N_MOUSE		2
#define N_PPP		3
#define N_STRIP		4
#define N_AX25		5
#define N_X25		6	/* X.25 async */
#define N_6PACK		7
#define N_MASC		8	/* Reserved for Mobitex module <kaz@cafe.net> */
#define N_R3964		9	/* Reserved for Simatic R3964 module */
#define N_PROFIBUS_FDL	10	/* Reserved for Profibus <Dave@mvhi.com> */
#define N_IRDA		11	/* Linux IR - http://irda.sourceforge.net/ */
#define N_SMSBLOCK	12	/* SMS block mode - for talking to GSM data cards about SMS messages */
#define N_HDLC		13	/* synchronous HDLC */
#define N_SYNC_PPP	14	/* synchronous PPP */
#define N_HCI		15  /* Bluetooth HCI UART */

#ifdef __KERNEL__

/*	intr=^C		quit=^\		erase=del	kill=^U
	eof=^D		vtime=\0	vmin=\1		sxtc=\0
	start=^Q	stop=^S		susp=^Z		eol=\0
	reprint=^R	discard=^U	werase=^W	lnext=^V
	eol2=\0
*/
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

#include <asm-generic/termios.h>

#endif	/* __KERNEL__ */

#endif	/* _OR32_TERMIOS_H */
