#ifndef _OR32_BOARH_H
#define _OR32_BOARH_H 

/* System clock frequecy */
#define SYS_CLK		(CONFIG_OR32_SYS_CLK*1000000)

/* Devices base address */
//#define UART_BASE_ADD   0x90000000

/* Define this if you want to use I and/or D MMU */

#define DMMU_SET_NB     CONFIG_OR32_DTLB_ENTRIES
#define IMMU_SET_NB     CONFIG_OR32_ITLB_ENTRIES

//#define OR32_CONSOLE_BAUD  115200
//#define UART_DEVISOR       SYS_CLK/(16*OR32_CONSOLE_BAUD)

#endif /* _OR32_BOARH_H */

