/*
 * OpenCores PS/2 core driver
 * http://opencores.org/project,ps2core
 *
 * Author: Stefan Kristiansson, stefan.kristiansson@saunalahti.fi
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/serio.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/of.h>

/* Register defines */
#define OCPS2_DATA      0x00
#define OCPS2_CTRSTS    0x01

/* Control/Status bit masks */
#define OCPS2_BUSY      0x01 /* Busy */
#define OCPS2_IBF       0x02 /* Input Buffer Full */
#define OCPS2_OBF       0x04 /* Output Buffer Full */
#define OCPS2_ERR       0x08 /* Parity or Frame Error */
#define OCPS2_RX_IRQ_EN 0x40 /* Rx interrupt enable */
#define OCPS2_TX_IRQ_EN 0x80 /* Tx interrupt enable */

struct ocps2_data {
	struct serio  serio;
	int           irq;
	void __iomem *base;
	phys_addr_t   base_phys;
	int           reg_size;
};

static inline u8 ocps2_readreg(void __iomem *base, loff_t offset)
{
	return ioread8(base + offset);
}

static inline void ocps2_writereg(void __iomem *base, loff_t offset, u8 data)
{
	iowrite8(data, base + offset);
}

static irqreturn_t ocps2_interrupt(int irq, void *dev_id)
{
	struct ocps2_data *drvdata = dev_id;
	unsigned char      data;
	unsigned int       io_flags = 0;
	
	if (ocps2_readreg(drvdata->base, OCPS2_CTRSTS) & OCPS2_ERR)
		dev_warn(drvdata->serio.dev.parent, "frame/parity error\n");

	if (ocps2_readreg(drvdata->base, OCPS2_CTRSTS) & OCPS2_IBF) {
		/* Clear interrupt and error bits */
		ocps2_writereg(drvdata->base, OCPS2_CTRSTS, OCPS2_RX_IRQ_EN);
		data = ocps2_readreg(drvdata->base, OCPS2_DATA);
		serio_interrupt(&drvdata->serio, data, io_flags);
	}
	return IRQ_HANDLED;
}

/*
 * serio callbacks
 */
static int ocps2_serio_write(struct serio *io, unsigned char val)
{
	struct ocps2_data *drvdata = io->port_data;

	ocps2_writereg(drvdata->base, OCPS2_DATA, val);
	return 0;
}

static int ocps2_serio_open(struct serio *io)
{
	struct ocps2_data *drvdata = io->port_data;

	/* 
	 * enable rx interrupt 
	 * (this will also clear IBF and ERR)
	 */
	ocps2_writereg(drvdata->base, OCPS2_CTRSTS, OCPS2_RX_IRQ_EN);
	return 0;
}

static void ocps2_serio_close(struct serio *io)
{
	struct ocps2_data *drvdata = io->port_data;

	/* disable rx interrupt */
	ocps2_writereg(drvdata->base, OCPS2_CTRSTS, 0);
}

static int ocps2_probe(struct platform_device *pdev)
{
	struct resource   *res_iomem;
	struct ocps2_data *drvdata;
	int                err;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		err = -ENOMEM;
		goto err_free;
	}

	res_iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_iomem) {
		err = -ENXIO;
		goto err_free;
	}

	drvdata->irq = platform_get_irq(pdev, 0);
	if (drvdata->irq < 0) {
		err = -ENXIO;
		goto err_free;
	}

	if (!request_mem_region(res_iomem->start,
				resource_size(res_iomem), pdev->name)) {
		err = -ENXIO;
		goto err_free;
	}

	drvdata->base = ioremap_nocache(res_iomem->start,
					resource_size(res_iomem));
	drvdata->base_phys = res_iomem->start;
	drvdata->reg_size  = resource_size(res_iomem);

	if (!drvdata->base) {
		err = -ENXIO;
		goto err_release;
	}
	
	err = request_irq(drvdata->irq, ocps2_interrupt, 0, 
			  pdev->name, drvdata);
	if (err) 
		goto err_unmap;
	
	dev_info(&pdev->dev, "OpenCores PS/2 at 0x%08x, irq=%d\n",
		 (u32)res_iomem->start, drvdata->irq);

	drvdata->serio.id.type    = SERIO_8042;
	drvdata->serio.write      = ocps2_serio_write;
	drvdata->serio.open       = ocps2_serio_open;
	drvdata->serio.close      = ocps2_serio_close;
	drvdata->serio.port_data  = drvdata;
	drvdata->serio.dev.parent = &pdev->dev;
	strlcpy(drvdata->serio.name, dev_name(&pdev->dev),
		sizeof(drvdata->serio.name));
	strlcpy(drvdata->serio.phys, dev_name(&pdev->dev),
		sizeof(drvdata->serio.phys));
	
	serio_register_port(&drvdata->serio);
	platform_set_drvdata(pdev, drvdata);

	/* disable interrupts and clear ERR and IBF */
	ocps2_writereg(drvdata->base, OCPS2_CTRSTS, 0);
	
	return 0;
err_unmap:
	iounmap(drvdata->base);
err_release:
	release_mem_region(res_iomem->start, resource_size(res_iomem));
err_free:
	kfree(drvdata);
	return err;
}

static int ocps2_remove(struct platform_device *pdev)
{
	struct ocps2_data *drvdata = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	serio_unregister_port(&drvdata->serio);
	free_irq(drvdata->irq, drvdata);
	iounmap(drvdata->base);
	release_mem_region(drvdata->base_phys, drvdata->reg_size);
	kfree(drvdata);
	return 0;
}

static const struct of_device_id ocps2_of_match[] = {
	{ .compatible = "opencores,ocps2",},
	{ /* end of list*/},
};

static struct platform_driver ocps2_driver = {
	.probe  = ocps2_probe,
	.remove = ocps2_remove,
	.driver = {
		.name           = "opencores_ps2",
		.owner          = THIS_MODULE,
		.of_match_table = ocps2_of_match
	}
};

static int __init ocps2_init(void)
{
	return platform_driver_register(&ocps2_driver);
}

static void __exit ocps2_exit(void)
{
	platform_driver_unregister(&ocps2_driver);
}

module_init(ocps2_init);
module_exit(ocps2_exit);

MODULE_AUTHOR("(c) 2011 Stefan Kristiansson <stefan.kristiansson@saunalahti.fi>");
MODULE_DESCRIPTION("OpenCores PS/2 driver");
MODULE_LICENSE("GPL");
