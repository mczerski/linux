#ifndef _OR32_BOARH_H
#define _OR32_BOARH_H 

#ifdef CONFIG_OR32_HIGHLAND
#include <asm/01-highland/highland.h>
#endif

/* System clock frequecy */
#define SYS_CLK		(CONFIG_OR32_SYS_CLK*1000000)
#define SYS_CLK_PERIOD  (1000/CONFIG_OR32_SYS_CLK)

/* Memory organization */
#define SRAM_BASE_ADD   0x00000000
#define FLASH_BASE_ADD  0xf0000000

/* Devices base address */
#define UART_BASE_ADD   0x90000000
#define MC_BASE_ADD     0x93000000
#define CRT_BASE_ADD    0x97000000
#define FBMEM_BASE_ADD  0xa8000000
#define ETH_BASE_ADD    0x92000000
#define KBD_BASE_ADD	0x94000000

/* Define this if you want to use I and/or D MMU */

#define DMMU_SET_NB     CONFIG_OR32_DTLB_ENTRIES
#define IMMU_SET_NB     CONFIG_OR32_ITLB_ENTRIES

/* Uart definitions */
#define UART_DLL        0       /* Out: Divisor Latch Low (DLAB=1) */
#define UART_DLM        1       /* Out: Divisor Latch High (DLAB=1) */

#define OR32_CONSOLE_BAUD  115200
#define UART_DEVISOR       SYS_CLK/(16*OR32_CONSOLE_BAUD)

/* Define ethernet MAC address */
#define MACADDR0	0x00
#define MACADDR1	0x01
#define MACADDR2	0x02
#define MACADDR3	0x03
#define MACADDR4	0x04
#define MACADDR5	0x05

#define N_CE        (8)

#ifdef CONFIG_OR32_MC_INIT
#define MC_CSR      (0x00)
#define MC_POC      (0x04)
#define MC_BA_MASK  (0x08)
#define MC_CSC(i)   (0x10 + (i) * 8)
#define MC_TMS(i)   (0x14 + (i) * 8)

/* memory controler initialization constants */
#  include "mc2.h"
#endif

/* sdram organization */
#  define MC_OSR_VAL              0x7e000033
#  define MC_CCR_4_VAL_DISABLED   0x00ef0004
#  define MC_CCR_4_VAL_ENABLED    (MC_CCR_4_VAL_DISABLED | 0xc0000000)

#endif /* _OR32_BOARH_H */

