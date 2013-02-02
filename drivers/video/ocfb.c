/*
 * OpenCores VGA/LCD 2.0 core frame buffer driver
 *
 * Author: Stefan Kristiansson, stefan.kristiansson@saunalahti.fi
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

/*
 * Rewritten from the OpenCores frame buffer driver
 * by Matjaz Breskvar (phoenix@bsemi.com) .
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>

/* VGA defines */
#define VGA_CTRL       0x000
#define VGA_STAT       0x004
#define VGA_HTIM       0x008
#define VGA_VTIM       0x00c
#define VGA_HVLEN      0x010
#define VGA_VBARA      0x014
#define VGA_PALETTE    0x800

#define VGA_CTRL_VEN   0x00000001 /* Video Enable */
#define VGA_CTRL_HIE   0x00000002 /* HSync Interrupt Enable */
#define VGA_CTRL_PC    0x00000800 /* 8-bit Pseudo Color Enable*/
#define VGA_CTRL_CD8   0x00000000 /* Color Depth 8 */
#define VGA_CTRL_CD16  0x00000200 /* Color Depth 16 */
#define VGA_CTRL_CD24  0x00000400 /* Color Depth 24 */
#define VGA_CTRL_CD32  0x00000600 /* Color Depth 32 */
#define VGA_CTRL_VBL1  0x00000000 /* Burst Length 1 */
#define VGA_CTRL_VBL2  0x00000080 /* Burst Length 2 */
#define VGA_CTRL_VBL4  0x00000100 /* Burst Length 4 */
#define VGA_CTRL_VBL8  0x00000180 /* Burst Length 8 */

#define PALETTE_SIZE   256

#define OCFB_NAME     "OC VGA/LCD"

static char *mode_option = NULL;

static const struct fb_videomode default_mode = {
	.name = NULL,
	.refresh = 72,
	.xres = 640,
	.yres = 480,
	.pixclock = 25000,
	.left_margin = 16,
	.right_margin = 48,
	.upper_margin = 11,
	.lower_margin = 31,
	.hsync_len = 96,
	.vsync_len = 2,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED
};

struct ocfb_dev {
	struct fb_info info;
	/* Physical and virtual addresses of control regs */
	phys_addr_t    regs_phys;
	int            regs_phys_size;
	void __iomem  *regs;
	/* Physical and virtual addresses of framebuffer */
	phys_addr_t    fb_phys;
	void __iomem  *fb_virt;
	u32            pseudo_palette[PALETTE_SIZE];
};

struct ocfb_par {
	void __iomem *pal_adr;
};

static struct ocfb_par ocfb_par_priv;

static struct fb_var_screeninfo ocfb_var;
static struct fb_fix_screeninfo ocfb_fix;

#ifndef MODULE
static int __init ocfb_setup(char *options)
{
	char *curr_opt;

	if (!options || !*options)
		return 0;

	while ((curr_opt = strsep(&options, ",")) != NULL) {
		if (!*curr_opt)
			continue;
		mode_option = curr_opt;
	}

	return 0;
}
#endif

static inline u32 ocfb_readreg(void __iomem *base, loff_t offset)
{
	return ioread32be(base + offset);
}

static inline void ocfb_writereg(void __iomem *base, loff_t offset, u32 data)
{
	iowrite32be(data, base + offset);
}

static int ocfb_setupfb(struct ocfb_dev *fbdev)
{
	unsigned long             bpp_config;
	struct fb_var_screeninfo *var   = &fbdev->info.var;
	void __iomem             *regs  = fbdev->regs;
	u32                       hlen, vlen;

	/* Horizontal timings */
	ocfb_writereg(regs, VGA_HTIM,
		      ((var->hsync_len    - 1) << 24) |
		      ((var->right_margin - 1) << 16) |
		      ( var->xres         - 1));

	/* Vertical timings */
	ocfb_writereg(regs, VGA_VTIM,
		      ((var->vsync_len    - 1) << 24) |
		      ((var->lower_margin - 1) << 16) |
		      ( var->yres         - 1));

	/* Total length of frame */
	hlen = var->left_margin  + var->right_margin +
	       var->hsync_len    + var->xres;

	vlen = var->upper_margin + var->lower_margin +
	       var->vsync_len    + var->yres;

	ocfb_writereg(regs, VGA_HVLEN,
		      ((hlen - 1) << 16) |
		      ( vlen - 1));

	/* Register framebuffer address */
	ocfb_writereg(regs, VGA_VBARA, fbdev->fb_phys);

	bpp_config = VGA_CTRL_CD8;
	switch (var->bits_per_pixel) {
	case 8:
		if (!var->grayscale)
		    bpp_config |= VGA_CTRL_PC;  /* enable palette */
		break;
	case 16: bpp_config |= VGA_CTRL_CD16; break;
	case 24: bpp_config |= VGA_CTRL_CD24; break;
	case 32: bpp_config |= VGA_CTRL_CD32; break;
	default:
		printk(KERN_ERR "ocfb: no bpp specified\n");
		break;
	}

	/* maximum (8) VBL (video memory burst length)*/
	bpp_config |= VGA_CTRL_VBL8;

	printk(KERN_INFO "ocfb: enabling framebuffer (%s)\n",
	       mode_option);

	/* Enable VGA */
	ocfb_writereg(regs, VGA_CTRL,
		     (VGA_CTRL_VEN |  bpp_config));
	return 0;
}

static int ocfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			  unsigned blue, unsigned transp,
			  struct fb_info *info)
{
	struct ocfb_par *par = (struct ocfb_par *)info->par;
	u32 color;

	if (regno >= info->cmap.len) {
		printk(KERN_ERR "ocfb_setcolreg: regno >= cmap.len\n");
		return(1);
	}

	if (info->var.grayscale) {
		/* grayscale = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue = (red * 77 + green * 151 + blue * 28) >> 8;
	}

	red    >>= (16 - info->var.red.length);
	green  >>= (16 - info->var.green.length);
	blue   >>= (16 - info->var.blue.length);
	transp >>= (16 - info->var.transp.length);

	if (info->var.bits_per_pixel == 8 && !info->var.grayscale) {
		regno <<= 2;
		color = (red << 16) | (green << 8) | blue;
		ocfb_writereg(par->pal_adr, regno, color);
	} else {
		((u32 *)(info->pseudo_palette))[regno] =
			(red    << info->var.red.offset)   |
			(green  << info->var.green.offset) |
			(blue   << info->var.blue.offset)  |
			(transp << info->var.transp.offset);
	}
	return 0;
}
static int ocfb_init_fix(struct ocfb_dev *fbdev)
{
	struct fb_var_screeninfo *var = &fbdev->info.var;
	struct fb_fix_screeninfo *fix = &fbdev->info.fix;

	strcpy(fix->id, OCFB_NAME);

	fix->smem_len    = var->xres * var->yres * var->bits_per_pixel/8;
	fix->line_length = var->xres * var->bits_per_pixel/8;
	fix->type        = FB_TYPE_PACKED_PIXELS;

	if (var->bits_per_pixel == 8 && !var->grayscale)
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	else
		fix->visual = FB_VISUAL_TRUECOLOR;

	return 0;
}

static int ocfb_init_var(struct ocfb_dev *fbdev)
{
	struct fb_var_screeninfo *var = &fbdev->info.var;

	var->accel_flags  = FB_ACCEL_NONE;
	var->activate     = FB_ACTIVATE_NOW;
	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres;

	switch (var->bits_per_pixel) {
	case 8:
		var->transp.offset = 0;  var->transp.length = 0;
		var->red.offset    = 0;  var->red.length    = 8;
		var->green.offset  = 0;  var->green.length  = 8;
		var->blue.offset   = 0;  var->blue.length   = 8;
		break;
	case 16:
		var->transp.offset = 0;  var->transp.length = 0;
		var->red.offset    = 11; var->red.length    = 5;
		var->green.offset  = 5;  var->green.length  = 6;
		var->blue.offset   = 0;  var->blue.length   = 5;
		break;
	case 24:
		var->transp.offset = 0;  var->transp.length = 0;
		var->red.offset    = 16; var->red.length    = 8;
		var->green.offset  = 8;  var->green.length  = 8;
		var->blue.offset   = 0;  var->blue.length   = 8;
		break;
	case 32:
		var->transp.offset = 24; var->transp.length = 8;
		var->red.offset    = 16; var->red.length    = 8;
		var->green.offset  = 8;  var->green.length  = 8;
		var->blue.offset   = 0;  var->blue.length   = 8;
		break;
	}
	return 0;
}

static struct fb_ops ocfb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg   = ocfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static int ocfb_probe(struct platform_device *pdev)
{
	int                  ret = 0;
	struct ocfb_dev     *fbdev;
	struct ocfb_par     *par = &ocfb_par_priv;
	struct resource     *res;
	struct resource     *mmio;
	int                  fbsize;

	fbdev = kzalloc(sizeof(*fbdev), GFP_KERNEL);
	if (!fbdev) {
		ret = -ENOMEM;
		goto err;
	}

	platform_set_drvdata(pdev, fbdev);

	fbdev->info.fbops  = &ocfb_ops;
	fbdev->info.var    = ocfb_var;
	fbdev->info.fix    = ocfb_fix;
	fbdev->info.device = &pdev->dev;
	fbdev->info.par    = par;

	/* Video mode setup */
	if (!fb_find_mode(&fbdev->info.var, &fbdev->info, mode_option,
			  NULL, 0, &default_mode, 16)) {
		dev_err(&pdev->dev, "No valid video modes found\n");
		ret = -EINVAL;
		goto err_free;
	}
	ocfb_init_var(fbdev);
	ocfb_init_fix(fbdev);

	/* Request I/O resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "I/O resource request failed\n");
		ret = -ENXIO;
		goto err_free;
	}
	fbdev->regs_phys      = res->start;
	fbdev->regs_phys_size = resource_size(res);
	mmio = devm_request_mem_region(&pdev->dev, res->start,
				       resource_size(res), res->name);
	if (!mmio) {
		dev_err(&pdev->dev, "I/O memory space request failed\n");
		ret = -ENXIO;
		goto err_free;
	}
	fbdev->regs = devm_ioremap_nocache(&pdev->dev, mmio->start,
					   resource_size(mmio));
	if (!fbdev->regs) {
		dev_err(&pdev->dev, "I/O memory remap request failed\n");
		ret = -ENXIO;
		goto err_iomap;
	}
	par->pal_adr = (fbdev->regs + VGA_PALETTE);
	fbsize = fbdev->info.fix.smem_len;

#if defined(CONFIG_FB_OC_SHMEM)
	/* Allocate framebuffer memory */
	fbdev->fb_virt = dma_alloc_coherent(&pdev->dev, PAGE_ALIGN(fbsize),
					    &fbdev->fb_phys, GFP_KERNEL);
	if (!fbdev->fb_virt) {
		dev_err(&pdev->dev, "Frame buffer memory allocation failed\n");
		ret = -ENOMEM;
		goto err_dma;
	}
#else
	/* obtain framebuffer memory space */
	//if (!res->sibling) {
	//	dev_err(&pdev->dev, "cannot obtain framebuffer memory space\n");
	//	ret = -ENXIO;
	//	goto err_mem;
	//}
	//res = res->sibling;

	//if (fbsize > resource_size(res)) {
	//	dev_err(&pdev->dev, "requested framebuffer memory is to large\n");
	//	ret = -ENXIO;
	//	goto err_mem;
	//}
	res->start = 0xe0000000;

	mmio = devm_request_mem_region(&pdev->dev, res->start,
			fbsize, res->name);
	if (!mmio) {
		dev_err(&pdev->dev, "cannot request framebuffer memory space\n");
		ret = -ENXIO;
		goto err_mem;
	}

	fbdev->fb_phys = mmio->start;

	fbdev->fb_virt = devm_ioremap_nocache(&pdev->dev, mmio->start,
			resource_size(mmio));

	if (!fbdev->fb_virt) {
		dev_err(&pdev->dev, "cannot remap framebuffer memory space\n");
		ret = -ENXIO;
		goto err_mem_iomap;
	}
#endif
	fbdev->info.fix.smem_start = fbdev->fb_phys;
	fbdev->info.screen_base    = (void __iomem*)fbdev->fb_virt;
	fbdev->info.pseudo_palette = fbdev->pseudo_palette;

	/* Clear framebuffer */
	memset_io((void __iomem *)fbdev->fb_virt, 0, fbsize);

	/* Setup and enable the framebuffer */
	ocfb_setupfb(fbdev);

	/* Allocate color map */
	ret = fb_alloc_cmap(&fbdev->info.cmap, PALETTE_SIZE, 0);
	if (ret) {
		dev_err(&pdev->dev, "Color map allocation failed\n");
		goto err_cmap;
	}

	/* Register framebuffer */
	ret = register_framebuffer(&fbdev->info);
	if (ret) {
		dev_err(&pdev->dev, "Framebuffer registration failed\n");
		goto err_regfb;
	}
	return 0;

err_regfb:
	fb_dealloc_cmap(&fbdev->info.cmap);
err_cmap:
	dma_free_coherent(&pdev->dev, PAGE_ALIGN(fbsize),
			  fbdev->fb_virt, fbdev->fb_phys);
#if !defined(CONFIG_FB_OC_SHMEM)
	iounmap(fbdev->fb_virt);
err_mem_iomap:
	devm_release_region(&pdev->dev, mmio->start, resource_size(mmio));
err_mem:
	iounmap(fbdev->regs);
#else
err_dma:
	iounmap(fbdev->regs);
#endif
err_iomap:
	release_mem_region(fbdev->regs_phys, fbdev->regs_phys_size);
err_free:
	kfree(fbdev);
err:
	return ret;
}

static int ocfb_remove(struct platform_device *pdev)
{
	struct ocfb_dev *fbdev = platform_get_drvdata(pdev);

	unregister_framebuffer(&fbdev->info);
	fb_dealloc_cmap(&fbdev->info.cmap);
	dma_free_coherent(&pdev->dev, PAGE_ALIGN(fbdev->info.fix.smem_len),
			  fbdev->fb_virt, fbdev->fb_phys);

	/* Disable display */
	ocfb_writereg(fbdev, VGA_CTRL, 0);

	iounmap(fbdev->regs);
	release_mem_region(fbdev->regs_phys, fbdev->regs_phys_size);
	kfree(fbdev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id ocfb_match[] = {
	{ .compatible = "opencores,ocfb", },
	{},
};
MODULE_DEVICE_TABLE(of, ocfb_match);

static struct platform_driver ocfb_driver = {
	.probe  = ocfb_probe,
	.remove	= ocfb_remove,
	.driver = {
		.name = "ocfb_fb",
		.of_match_table = ocfb_match,
	}
};

/*
 * Init and exit routines
 */
static int __init ocfb_init(void)
{
#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("ocfb", &option))
		return -ENODEV;
	ocfb_setup(option);
#endif
	return platform_driver_register(&ocfb_driver);
}

static void __exit ocfb_exit(void)
{
	platform_driver_unregister(&ocfb_driver);
}

module_init(ocfb_init);
module_exit(ocfb_exit);

#ifdef MODULE
MODULE_AUTHOR("(c) 2011 Stefan Kristiansson <stefan.kristiansson@saunalahti.fi>");
MODULE_DESCRIPTION("OpenCores VGA/LCD 2.0 frame buffer driver");
MODULE_LICENSE("GPL");
module_param(mode_option, charp, 0);
MODULE_PARM_DESC(mode_option, "Video mode ('<xres>x<yres>[-<bpp>][@refresh]')");
#endif
