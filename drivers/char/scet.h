
#ifndef SCET_H
#define SCET_H

#include <linux/ioctl.h>

#define DEVELOP 1
/* 
 * The major device number. We can't rely on dynamic 
 * registration any more, because ioctls need to know 
 * it. 
 */
#define MAJOR_NUM 100
#define TIMER_WIDTH 0x06

/* 
 * Set the time of the SCET
 */
#define IOCTL_SET_SCET_TIME _IOR(MAJOR_NUM, 0, unsigned char *)

/* 
 * Get the time of the SCET
 * This always return 6 unsigned char
 * Note: prepare enought bufferspace in userspace before calling. 
 */
#define IOCTL_GET_SCET_TIME _IOR(MAJOR_NUM, 1, unsigned char *)

/* 
 * Set the run status of the SCET
 * 0 for stop the timer
 * 1 to start the timer
 */
#define IOCTL_SET_SCET_OPERATION _IOWR(MAJOR_NUM, 2, int)

/* 
 * Get the run status of the SCET
 * 0 = Stoped
 * 1 = Running
 * 2 = Freezing + Stoped
 * 3 = Freezing + Running
 */
#define IOCTL_GET_SCET_STATUS _IOR(MAJOR_NUM, 3, int *)

/* 
 * The name of the device file 
 */
#define DEVICE_FILE_NAME "scet_dev"

// Register offsets
#define SCET_TB0 0x0		// Timer byte 0
#define SCET_TB1 0x1		// Timer byte 1
#define SCET_TB2 0x2		// Timer byte 2
#define SCET_TB3 0x3		// Timer byte 3
#define SCET_TB4 0x4		// Timer byte 4
#define SCET_TB5 0x5		// Timer byte 5
#define SCET_TCTRL 0x6		// Timer control

// Timer control bit masks
#define SCET_TCTRL_TIMER_RUNNING 0x1
#define SCET_TCTRL_OUTPUT_FROZEN 0x2
#define SCET_TCTRL_TIMER_STOP 0x0

struct scet_cd {
	void __iomem *addr_reg;
	u8 timer_freeze;
	u8 timer_run;
	u8 current_time[TIMER_WIDTH];
};

static inline void scet_write(struct scet_cd *s_scet_cd, int reg, u8 val)
{
	void __iomem *addr_reg = s_scet_cd->addr_reg;

	((volatile unsigned char *)addr_reg)[reg] = val;
}

static inline u8 scet_read(struct scet_cd *s_scet_cd, int reg)
{
	u8 temp;
	void __iomem *addr_reg = s_scet_cd->addr_reg;

	temp = ((volatile unsigned char *)addr_reg)[reg];
	return temp;
}

#endif
