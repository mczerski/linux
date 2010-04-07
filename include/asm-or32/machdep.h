#ifdef __KERNEL__
#ifndef _OR32_MACHDEP_H
#define _OR32_MACHDEP_H


struct pt_regs;
struct pci_bus;	
struct pci_dev;
struct seq_file;

struct hwclk_time;
struct mktime;

extern void config_BSP(char *commandp, int size);
extern void (*mach_sched_init)(void);
extern void (*mach_tick)(void);
/* machine dependent timer functions */
extern unsigned long (*mach_gettimeoffset)(void);
extern void (*mach_gettod)(int *year, int *mon, int *day, int *hour, int *min, int *sec);
extern int (*mach_hwclk)(int, struct hwclk_time*);
extern int (*mach_set_clock_mmss)(unsigned long);
extern void (*mach_mksound)( unsigned int count, unsigned int ticks );
extern void (*mach_reset)( void );
extern void (*mach_debug_init)(void);

#endif /* _OR32_MACHDEP_H */
#endif /* __KERNEL__ */
