/*
 * OpenCores framebuffer
 *
 * by Matjaz Breskvar (phoenix@bsemi.com)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/dio.h>
#include <asm/io.h>

static struct fb_info info;
static struct fb_ops ocfb_ops;

#define REG32(add) (*(volatile unsigned long *)(add))
#define REG16(add) (*(volatile unsigned short *)(add))
#define REG8(add) (*(volatile unsigned char *)(add))

/* VGA defines */
#define VGA_BASE      0x97000000
#define VGA_CTRL      0x000
#define VGA_STAT      0x004
#define VGA_HTIM      0x008
#define VGA_VTIM      0x00c
#define VGA_HVLEN     0x010
#define VGA_VBARA     0x014

static u32 pseudo_palette[16];

static struct fb_fix_screeninfo ocfb_rgb8_fix __initdata = {
	.id		= "OC VGA/LCD",
	.smem_len	= (640*480),
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_PSEUDOCOLOR,
	.line_length	= 640,
//	.accel		= FB_ACCEL_NONE,
};

static struct fb_var_screeninfo ocfb_rgb8_var __initdata = {
	.xres		= 640,
	.yres		= 480,
	.xres_virtual	= 640,
	.yres_virtual	= 480,
	.bits_per_pixel	= 8,
	.grayscale      = 1,
	.red		= { 0,8,1},	/* R */
	.green		= { 0,8,1},	/* G */
	.blue		= { 0,8,1},	/* B */
	.activate	= FB_ACTIVATE_NOW,
	.height		= 274,
	.width		= 195,	/* 14" monitor */
	.accel_flags	= FB_ACCEL_NONE,
	.vmode		= FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo ocfb_rgb16_fix __initdata = {
	.id		= "OC VGA/LCD",
	.smem_len	= (640*480*2),
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_TRUECOLOR,
// FB_VISUAL_TRUECOLOR, FB_VISUAL_STATIC_PSEUDOCOLOR
	.line_length	= 640*2,
};

static struct fb_var_screeninfo ocfb_rgb16_var __initdata = {
	.xres		= 640,
	.yres		= 480,
	.xres_virtual	= 640,
	.yres_virtual	= 480,
	.bits_per_pixel	= 16,
	.red	        = { .offset = 11, .length = 5, },
	.green   	= { .offset = 5,  .length = 6, },
	.blue   	= { .offset = 0,  .length = 5, },
	.transp         = { .offset = 0,  .length = 0, },
};

extern unsigned long fb_mem_start;


static int ocfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			  unsigned blue, unsigned transp,
			  struct fb_info *info)
{
	if (regno >= info->cmap.len) {
		printk("ocfb_setcolreg: regno >= cmap.len\n");
		return(1);
	}

	switch (info->var.bits_per_pixel) {
	case 8:
		/* write color into hw */
		/*
		fb_writew(regno << 8, HD64461_CPTWAR);
		fb_writew(red >> 10, HD64461_CPTWDR);
		fb_writew(green >> 10, HD64461_CPTWDR);
		fb_writew(blue >> 10, HD64461_CPTWDR);
		*/
		break;
	case 16:
		((u32 *) (info->pseudo_palette))[regno] =
		    ((red & 0xf800)) |
		    ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);
		break;
	}
	return 0;

}


void debug_write_pattern(void *fb_shmem_virt, int xsize, int ysize);
void init_and_enable(int xsize, int ysize, int bpp);
void debug_bw_tests(void);

/*
 *  Initialization
 */
int __init ocfb_init(void)
{
	void *fb_shmem_virt;

	printk("\n\n");
	printk("OpenCores VGA/LCD 2.0 core init\n");
	printk("\n\n");

	/* if we have framebuffer in main memory it always 
         * starts after max_low_pfn (see arch/or32/kernel/setup.c)
         */
	printk("ioremap(fb_mem_start 0x%lx, size 0x%x)\n",
	       fb_mem_start, CONFIG_FB_OC_SHMEM_SIZE);
	fb_shmem_virt = ioremap_nocache(fb_mem_start, CONFIG_FB_OC_SHMEM_SIZE);
	if (fb_shmem_virt == NULL) {
		printk("ocfb: ioremap of framebuffer failed\n");
		return -1;
	}

#if 1
	debug_write_pattern(fb_shmem_virt, 640, 480);
	init_and_enable(640, 480, 16);
#endif

#if 0
	debug_write_pattern(fb_shmem_virt, 800, 600);
	init_and_enable(800, 600, 8);
#endif

	info.par = fb_shmem_virt;
	info.flags = FBINFO_FLAG_DEFAULT,


	/*
	 *	Let there be consoles..
	 */
	info.fbops = &ocfb_ops;
	info.var   = ocfb_rgb16_var;
	info.fix   = ocfb_rgb16_fix;

	ocfb_rgb16_fix.smem_start = (unsigned long)fb_shmem_virt;
	info.screen_base    = fb_shmem_virt;

	info.pseudo_palette = pseudo_palette;
	info.flags = FBINFO_FLAG_DEFAULT;

	/* This has to been done !!! */	
	fb_alloc_cmap(&info.cmap, 256, 0);
	
	if (register_framebuffer(&info) < 0)
		return -EINVAL;
	printk(KERN_INFO "fb%d: %s frame buffer device\n", info.node,
	       info.fix.id);

#if 0
	debug_bw_tests();
#endif

	return 0;
}

static void __exit ocfb_cleanup(void)
{
	/*
	 *  If your driver supports multiple boards, you should unregister and
	 *  clean up all instances.
	 */
	
//	unregister_framebuffer(info);
	/* ... */
}

/* --------------------------------------------[ frame buffer operations ]-- */


/*
 *  Frame buffer operations
 */

static struct fb_ops ocfb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg   = ocfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_cursor	= soft_cursor,
};

/* ------------------------------------------------------------------------- */

#ifdef MODULE
module_init(ocfb_init);
module_exit(ocfb_cleanup);
#endif

MODULE_LICENSE("GPL");

/* ------------------------------------------------------------------------- */

void init_and_enable(int xsize, int ysize, int bpp)
{
	unsigned long bpp_config;

	printk("HL: enabling VGA pads\n");
        /* enable VGA pads by setting SYS MUXes */
        REG32(0xb8070010) = 0;
        REG32(0xb8070014) = 0;
        REG32(0xb8070018) = 0;
	
	if ((xsize == 800) && (ysize == 600)) {
		printk("HL: setting VGA timings: 800x600 @75Hz\n");
		
		/* VESA: 800x600@60Hz 
		 */
		
		/* pixel clock 37.5 Mhz --> vertical frequency   56.55  Hz
		 *                      --> horizontal frequency 35.51 kHz
		 */
		
		/*
		 *  VERTICAL TIMINGS
		 *  sync pulse  :   4
		 *  front porch :   1
		 *  active frame: 600
		 */
		REG32(VGA_BASE+VGA_VTIM)  = ((  4 - 1) << 24) | 
			                    ((  1 - 1) << 16) | 
			                     (600 - 1);
		
		/*  HORIZONTAL TIMINGS
		 *  sync pulse  : 128
		 *  front porch :  40
		 *  active frame: 800
		 */
		REG32(VGA_BASE+VGA_HTIM)  = ((128 - 1) << 24) | 
			                    (( 40 - 1) << 16) | 
			                     (800 - 1);
		
		/* TOTAL
		 * horizontal total: 1056
		 * vertical total  : 628
		 */
		REG32(VGA_BASE+VGA_HVLEN) = ((1056 - 1) << 16) | 
			                     ( 628 - 1);
		REG32(VGA_BASE+VGA_VBARA) = fb_mem_start;

	} else if ((xsize == 640) && (ysize == 480)) {
		printk("HL: setting VGA timings: 640x480 @60Hz\n");
		
		/* VESA: 640x480@60Hz 
		 */
		
		/* pixel clock 25 Mhz --> vertical frequency   59.64 Hz
		 *                    --> horizontal frequency 31.25 kHz
		 */
		
		/*
		 *  VERTICAL TIMINGS
		 *  sync pulse  :   2
		 *  front porch :  11
		 *  active frame: 480
		 */
		REG32(VGA_BASE+VGA_VTIM)  = ((  2 - 1) << 24) | 
		                            (( 11 - 1) << 16) | 
		                             (480 - 1);

		/* HORIZONTAL TIMINGS
		 * sync pulse  :  96
		 * front porch :  16
		 * active frame: 640
		 */
		REG32(VGA_BASE+VGA_HTIM)  = (( 96 - 1) << 24) | 
			                    (( 16 - 1) << 16) | 
                                             (640 - 1);
	
		/* TOTAL
		 * horizontal total: 800
		 * vertical total  : 524
		 */
		REG32(VGA_BASE+VGA_HVLEN) = ((800 - 1) << 16) | 
			                     (524 - 1);
		
		REG32(VGA_BASE+VGA_VBARA) = fb_mem_start;

	} else {

		printk("HL: error no VGA timings specified\n");
	}


	bpp_config = 0;
	if (bpp==8) {
		bpp_config |= 0x0; 
	} else if (bpp==16) {
		bpp_config |= 0x200; 	
	} else if (bpp==24) {
		bpp_config |= 0x400; 	
	} else if (bpp==32) {
		bpp_config |= 0x600; 	
	} else {
		printk("HL: no bpp specified\n");
	}
	

	/* maximum (8) VBL (video memory burst length)*/
	bpp_config |= 0x180;
    

	printk("HL: enabling framebuffer (bpp==%d)\n", bpp);
	/* VGA enable */
	REG32(VGA_BASE+VGA_CTRL) = 0x00000101 | bpp_config;
}


void debug_write_pattern(void *fb_shmem_virt, int xsize, int ysize)
{
	
	char *fb_mem;
	int v,h;
	
	fb_mem = (char *)fb_shmem_virt;
	
	for (h=0;h<xsize;h++) {
		for (v=0;v<ysize;v++) {
			if (h < 255)
				fb_mem[v*xsize + h]=h;
			else
				fb_mem[v*xsize + h]=100;
		}
	}
	
	/* last 30 lines almost black */
	for (h=0;h<xsize;h++)
		for (v=400;v<ysize;v++)
			fb_mem[v*xsize + h]=30;
	
	/* last 2 lines white */
	for (h=0;h<xsize;h++)
		for (v=478;v<ysize;v++)
			fb_mem[v*xsize + h]=255;
	
	
}	

void debug_bw_tests()
{
	
	unsigned long i;
	unsigned long flags, tmp;
	unsigned long addr=0xc0000000;
	
	
	printk("going into loop 2 x 10^7 cycles: IRQs on\n");
	for (i=0;i<20000000;i++)
		__asm__ __volatile__("l.nop");
	
	
	local_irq_save(flags);
	printk("going into loop 2 x 10^7 cycles: IRQs off\n");
	for (i=0;i<20000000;i++)
		__asm__ __volatile__("l.nop");
	local_irq_restore(flags);
	
	local_irq_save(flags);
	/* now to really screw him */
	printk("going into loop 2 x 10^7 cycles: IRQs off, loads of loads\n");
	for (i=0;i<5000000;i++) {
		__asm__ __volatile__("l.lbz %0,0x0000001(%1)" : "=r"(tmp) : "r"(addr));
		__asm__ __volatile__("l.lhz %0,0x1000002(%1)" : "=r"(tmp) : "r"(addr));
		__asm__ __volatile__("l.lwz %0,0x1800004(%1)" : "=r"(tmp) : "r"(addr));
		
		__asm__ __volatile__("l.lbz %0,0x0000201(%1)" : "=r"(tmp) : "r"(addr));
		__asm__ __volatile__("l.lhz %0,0x1000602(%1)" : "=r"(tmp) : "r"(addr));
		__asm__ __volatile__("l.lwz %0,0x1800104(%1)" : "=r"(tmp) : "r"(addr));
		
		__asm__ __volatile__("l.lbz %0,0x0000301(%1)" : "=r"(tmp) : "r"(addr));
		__asm__ __volatile__("l.lhz %0,0x1000702(%1)" : "=r"(tmp) : "r"(addr));
		__asm__ __volatile__("l.lwz %0,0x1800204(%1)" : "=r"(tmp) : "r"(addr));
	}
	local_irq_restore(flags);
	
#if 0
	{
		unsigned long *tick=0xc0000500;
		
		/* triggers a bug */	
		local_irq_save(flags);
		printk("going into loop 2 x 10^7 cycles: IRQs off, tick interrupt\n");
		/* l.rfe at tick timer */
		*tick = 0x24000000;
		/* tick timer enable */
		mtspr(SPR_SR, mfspr(SPR_SR) & !SPR_SR_TEE);
		for (i=0;i<20000000;i++) {
			__asm__ __volatile__("l.lbz %0,0x0000001(%1)" : "=r"(tmp) : "r"(addr));
			__asm__ __volatile__("l.lhz %0,0x1002002(%1)" : "=r"(tmp) : "r"(addr));
			__asm__ __volatile__("l.lwz %0,0x1804004(%1)" : "=r"(tmp) : "r"(addr));
			
			__asm__ __volatile__("l.lbz %0,0x0000201(%1)" : "=r"(tmp) : "r"(addr));
			__asm__ __volatile__("l.lhz %0,0x1002602(%1)" : "=r"(tmp) : "r"(addr));
			__asm__ __volatile__("l.lwz %0,0x1804104(%1)" : "=r"(tmp) : "r"(addr));
			
			__asm__ __volatile__("l.lbz %0,0x0000301(%1)" : "=r"(tmp) : "r"(addr));
			__asm__ __volatile__("l.lhz %0,0x1002702(%1)" : "=r"(tmp) : "r"(addr));
			__asm__ __volatile__("l.lwz %0,0x1804204(%1)" : "=r"(tmp) : "r"(addr));
		}
		local_irq_restore(flags);
	}
#endif
}
