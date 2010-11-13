/*
 *  linux/arch/or32/kernel/setup.c
 *
 *  or32 version
 *    author(s): Matjaz Breskvar (phoenix@bsemi.com)
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 *  derived from cris, i386, m68k, ppc, sh ports.
 *
 *  changes:
 *  18. 11. 2003: Matjaz Breskvar (phoenix@bsemi.com)
 *    initial port to or32 architecture
 *
 */
 
/*
 * This file handles the architecture-dependent parts of initialization
 */
 
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/seq_file.h>
#include <linux/console.h>
 
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/smp.h>
#include <asm/pgtable.h>
#include <asm/types.h>
 
/*
 * Setup options
 */
 
extern int root_mountflags;
extern char _etext, _edata, _end;

#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
extern char _initrd_start, _initrd_end;
#endif
 
#define COMMAND_LINE_SIZE 256

#include <asm/machdep.h>

extern void config_BSP(char *command, int len);

void (*mach_tick)(void) = NULL;
int (*mach_set_clock_mmss) (unsigned long) = NULL;
void (*mach_sched_init) (void (*handler)(int, void *, struct pt_regs *)) = NULL;
unsigned long (*mach_gettimeoffset)(void) = NULL;
void (*mach_gettod)(int *year, int *mon, int *day, int *hour, int *min, int *sec) = NULL;
int (*mach_hwclk)(int, struct hwclk_time*) = NULL;
void (*mach_mksound)( unsigned int count, unsigned int ticks) = NULL;
void (*mach_reset)( void ) = NULL;
void (*mach_debug_init)(void) = NULL;


static char command_line[COMMAND_LINE_SIZE] = "root=/dev/ram";
char saved_command_line[COMMAND_LINE_SIZE];
extern const unsigned long text_start, edata; /* set by the linker script */ 
       
extern kdev_t or32_console_device(struct console *c);
extern int or32_console_setup(struct console *co, char *options);
extern void or32_console_write(struct console *co, const char *buf, unsigned int len);

static struct console or32_console = {
	name:   "ttyS",
	write:  or32_console_write,
	device: or32_console_device,
	setup:  or32_console_setup,
	flags:  CON_PRINTBUFFER,
	index:  -1,
};

void or32_console_init(void)
{
	register_console(&or32_console);
}

static unsigned long __init setup_memory(void)
{
	unsigned long bootmap_size, start_pfn, max_low_pfn;


#define PFN_UP(x)       (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)     ((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)     ((x) << PAGE_SHIFT)
	
	/* min_low_pfn points to the start of DRAM, start_pfn points
	 * to the first DRAM pages after the kernel, and max_low_pfn
	 * to the end of DRAM. Partial pages are not useful, so round it 
	 * down.
	 */ 
	start_pfn   = PFN_UP(__pa(&_end));
	max_low_pfn = PFN_DOWN(CONFIG_OR32_MEMORY_SIZE);
	min_low_pfn = PAGE_OFFSET >> PAGE_SHIFT;

	/* 
	 * Initialize the boot-time allocator (with low memory only)
	 */ 
	bootmap_size = init_bootmem(start_pfn, max_low_pfn);
	free_bootmem(PFN_PHYS(start_pfn), PFN_PHYS(max_low_pfn - start_pfn));
	reserve_bootmem(PFN_PHYS(start_pfn), bootmap_size);

	return(max_low_pfn);
}

static inline unsigned long extract_value_bits(unsigned long reg, 
					       short bit_nr, short width)
{
	return((reg >> bit_nr) & (0 << width));
}

static inline unsigned long extract_value(unsigned long reg, 
					  unsigned long mask)
{
	while (!(mask & 0x1)) {
		reg  = reg  >> 1;
		mask = mask >> 1;
	}
	return(mask & reg);
}

void __init detect_timer(void) 
{
	unsigned long cfg;

	cfg = mfspr(SPR_TTMR);
	printk("period 0x%x\n", SPR_TTMR & SPR_TTMR_PERIOD);
}

void __init detect_unit_config(unsigned long upr, unsigned long mask,
			       char *text, void (*func)(void))
{
        if (text != NULL)
		printk("%s", text);

	if ( upr & mask ) {
		if (func != NULL)
			func();
		else
			printk("present\n");
	}
	else
		printk("N/A\n");
}


void __init detect_soc_generic(unsigned long upr)
{
	detect_unit_config(upr, SPR_UPR_DCP,  "  dCACHE: ", NULL);
	detect_unit_config(upr, SPR_UPR_ICP,  "  iCACHE: ", NULL);
	detect_unit_config(upr, SPR_UPR_DMP,  "  dMMU\t: ", NULL);
	detect_unit_config(upr, SPR_UPR_IMP,  "  iMMU\t: ", NULL);
	detect_unit_config(upr, SPR_UPR_DUP,  "  debug : ", NULL);
	detect_unit_config(upr, SPR_UPR_PCUP, "  PerfC : ", NULL);
	detect_unit_config(upr, SPR_UPR_PMP,  "  PM    : ", NULL);
	detect_unit_config(upr, SPR_UPR_PICP, "  PIC   : ", NULL);
	detect_unit_config(upr, SPR_UPR_TTP,  "  TIMER : ", detect_timer);
	detect_unit_config(upr, SPR_UPR_CUST, "  CUs   : ", NULL);

/* add amount configured memory 
 */
}

void __init detect_soc_rev_0(unsigned long upr)
{
	
	printk("  WARNING, using default values !\n"
               "           (some early revision 0 processors don't have unit present register\n"
	       "            populated with all avaliable units)\n");

	printk("  dCACHE: assumed %4d Kb size, %2d bytes/line, %d way(s)", 
	       CONFIG_OR32_DC_SIZE, CONFIG_OR32_DC_LINE, 1);
	detect_unit_config(upr, SPR_UPR_DCP,  ", detected: ", NULL);

	printk("  iCACHE: assumed %4d Kb size, %2d bytes/line, %d way(s)", 
	       CONFIG_OR32_IC_SIZE, CONFIG_OR32_IC_LINE, 1);
	detect_unit_config(upr, SPR_UPR_ICP,  ", detected: ", NULL);

	printk("  dMMU\t: assumed %4d entries, %d way(s)", 
	       CONFIG_OR32_DTLB_ENTRIES, 1);
	detect_unit_config(upr, SPR_UPR_DMP,  ", detected: ", NULL);

	printk("  iMMU\t: assumed %4d entries, %d way(s)", 
	       CONFIG_OR32_ITLB_ENTRIES, 1);
	detect_unit_config(upr, SPR_UPR_IMP,  ", detected: ", NULL);
	detect_unit_config(upr, SPR_UPR_DUP,  "  debug : unknown (guess yes), detected: ", NULL);
	detect_unit_config(upr, SPR_UPR_PCUP, "  PerfC : unknown (guess no ), detected: ", NULL);
	detect_unit_config(upr, SPR_UPR_PMP,  "  PM    : unknown (guess yes), detected: ", NULL);
	detect_unit_config(upr, SPR_UPR_PICP, "  PIC   : unknown (guess yes), detected: ", NULL);
	detect_unit_config(upr, SPR_UPR_TTP,  "  TIMER : unknown (guess yes), detected: ", NULL);
	detect_unit_config(upr, SPR_UPR_CUST, "  CUs   : unknown (guess no ), detected: ", NULL);

/* add amount configured memory 
 */
}

void __init detect_soc(void)
{
	unsigned long upr, cfg, version, revision;

	upr = mfspr(SPR_UPR);
	if (upr &  SPR_UPR_UP)
		printk("Detecting Processor units:\n");
	else {
		printk("Unit Present Register not avaliable\n");
		return;
	}

	cfg = mfspr(SPR_VR);
	printk("  CPU\t: or32/OpenRISC-%x, revision %x, @%d MHz, %s\n", 
	       version=extract_value(cfg, SPR_VR_VER), 
	       revision=extract_value(cfg, SPR_VR_REV),
	       CONFIG_OR32_SYS_CLK,
	       (upr & (unsigned long)SPR_UPR_SRP) ? 
	       "with shadow registers" : "with no shadow registers");

	if ((version == 0x1200) & (revision == 0)) {
		detect_soc_rev_0(upr);
	} else {
		detect_soc_generic(upr);
	}
}



void __init setup_arch(char **cmdline_p)
{
	unsigned long max_low_pfn;

	config_BSP(&command_line[0], COMMAND_LINE_SIZE);
	
	/* register an initial console printing routine for printk's */
	or32_console_init();

	/* detect System on Chip parameters */
	detect_soc();

	/* process 1's initial memory region is the kernel code/data */
	init_mm.start_code = (unsigned long) &text_start;
	init_mm.end_code =   (unsigned long) &_etext;
	init_mm.end_data =   (unsigned long) &_edata;
	init_mm.brk =        (unsigned long) &_end;

#ifdef CONFIG_BLK_DEV_INITRD
	initrd_start = (unsigned long)&_initrd_start;
	initrd_end = (unsigned long)&_initrd_end;
#endif

        /* setup bootmem allocator */
        max_low_pfn = setup_memory();
		
	/* paging_init() sets up the MMU and marks all pages as reserved */
	paging_init();
	
	*cmdline_p = command_line;
	
	/* Save command line copy for /proc/cmdline */
	memcpy(saved_command_line, command_line, COMMAND_LINE_SIZE);
	saved_command_line[COMMAND_LINE_SIZE-1] = '\0';
	
	printk("Linux/or32 port 2003 OpenCores\n");
}

static int show_cpuinfo(struct seq_file *m, void *v)
{
	return seq_printf(m,
			  "cpu\t\t: or32\n"
			  "cpu revision\t: %lu\n"
			  "cpu model\t: %s\n"
			  "cache size\t: %d kB\n"
			  "fpu\t\t: %s\n"
			  "mmu\t\t: %s\n"
			  "ethernet\t: %s Mbps\n"
			  "token ring\t: %s\n"
			  "scsi\t\t: %s\n"
			  "ata\t\t: %s\n"
			  "usb\t\t: %s\n"
			  "bogomips\t: %lu.%02lu\n",
			  
			  1UL,
			  "Marvin",
			  4,
			  "no",
			  "yes",
			  "10",
			  "no",
			  "no",
			  "no",
			  "no",
			  (loops_per_jiffy * HZ) / 500000,
			  ((loops_per_jiffy * HZ) / 5000) % 100);
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	/* We only got one CPU... */
	return *pos < 1 ? (void *)1 : NULL;
}
 
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void c_stop(struct seq_file *m, void *v)
{
}

struct seq_operations cpuinfo_op = {
	start:  c_start,
	next:   c_next,
	stop:   c_stop,
	show:   show_cpuinfo,
};

void arch_gettod(int *year, int *mon, int *day, int *hour,
		                  int *min, int *sec)
{
   
	if (mach_gettod)
		mach_gettod(year, mon, day, hour, min, sec);
	else
		*year = *mon = *day = *hour = *min = *sec = 0;
}

