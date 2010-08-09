#ifndef __ASM_OPENRISC_CPUINFO_H
#define __ASM_OPENRISC_CPUINFO_H

struct cpuinfo {
	u32 clock_frequency;

	u32 icache_size;
	u32 icache_block_size;

	u32 dcache_size;
	u32 dcache_block_size;	
};

extern struct cpuinfo cpuinfo;

#endif /* __ASM_OPENRISC_CPUINFO_H */

