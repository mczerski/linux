
struct cpuinfo {
	u32 icache_size;
	u32 icache_block_size;

	u32 dcache_size;
	u32 dcache_block_size;	
};

extern struct cpuinfo cpuinfo;
