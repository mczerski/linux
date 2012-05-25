/*
 * spi_opencores.c -- OpenCores SPI controller driver
 *
 * Author: Jonas Bonn <jonas@southpole.se>
 * Copyright (C) 2010 South Pole AB
 *
 * Derived from orion_spi.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <asm/unaligned.h>
#include <asm/cpuinfo.h>

#define DRIVER_NAME			"oc_spi_simple"

#define OCSPI_NUM_CHIPSELECTS		8 
#define OCSPI_WAIT_RDY_MAX_LOOP		2000 /* in usec */

#define OCSPI_REG_SPCR			0x0
#define OCSPI_REG_SPSR			0x1
#define OCSPI_REG_SPDR			0x2
#define OCSPI_REG_SPER			0x3
#define OCSPI_REG_SSR			0x4

#define OCSPI_SPCR_SPIE			(1 << 7)
#define OCSPI_SPCR_SPE			(1 << 6)
#define OCSPI_SPCR_MSTR			(1 << 4)
#define OCSPI_SPCR_CPOL			(1 << 3)
#define OCSPI_SPCR_CPHA			(1 << 2)
#define OCSPI_SPCR_SPR			0x03

#define OCSPI_SPSR_SPIF			(1 << 7)
#define OCSPI_SPSR_WCOL			(1 << 6)
#define OCSPI_SPSR_WFFULL		(1 << 3)
#define OCSPI_SPSR_WFEMPTY		(1 << 2)
#define OCSPI_SPSR_RFFUL		(1 << 1)
#define OCSPI_SPSR_RFEMPTY		(1 << 0)

#define OCSPI_SPER_ICNT			0xc0
#define OCSPI_SPER_ESPR			0x03

struct ocspi {
	struct work_struct	work;

	/* Lock access to transfer list.	*/
	spinlock_t		lock;

	struct list_head	msg_queue;
	struct spi_master	*master;
	void __iomem		*base;
	unsigned int		max_speed;
	unsigned int		min_speed;
};

static struct workqueue_struct *ocspi_wq;

static inline u8
ocspi_read(struct ocspi* ocspi, unsigned int reg) {
	return ioread8(ocspi->base + reg);
}

static inline void
ocspi_write(struct ocspi* ocspi, unsigned int reg, u8 value) {
	iowrite8(value, ocspi->base + reg);
}

static int
ocspi_set_transfer_size(struct ocspi *ocspi, unsigned int size)
{
	if (size != 8 && size != 16 && size != 24 && size != 32) {
		printk("Bad transfer size: %d\n", size);
		return -EINVAL;
	}

	/* Nothing to do */
	/* If we were using interrupts we would probably set the 
	 * ICNT bits of the SPER register here... but we're not!
	 */

	return 0;
}

static inline void
ocspi_set_baudrate_bits(u8* spcr, u8* sper, unsigned int speed)
{
	int i;

	for (i = 0; i <= 11; i++) {
		if ((cpuinfo.clock_frequency >> (1+i)) <= speed)
			break;
	}

	pr_debug("Established baudrate %d, wanted %d (i=%d)", cpuinfo.clock_frequency >> (1+i), speed, i);

	/* The register values for some cases are weird... fix here */
	switch (i) {
		case 2:
			i = 4;
			break;
		case 3:
			i = 2;
			break;
		case 4:
			i = 3;
			break;
		default:
			break;
	}

	pr_debug(" (new i=%d)\n", i);

	*spcr |= (i & OCSPI_SPCR_SPR);
	*sper |= ((i >> 2) & OCSPI_SPER_ESPR);
}

static inline void
ocspi_set_mode_bits(u8* spcr, int mode)
{
	if (mode & SPI_CPHA) {
		*spcr |= OCSPI_SPCR_CPHA;	
	} else {
		*spcr &= ~OCSPI_SPCR_CPHA;
	}

	if (mode & SPI_CPOL) {
		*spcr |= OCSPI_SPCR_CPOL;
	} else {
		*spcr &= ~OCSPI_SPCR_CPOL;
	}
}

/*
 * Called only when no transfer is active on the bus... this may
 * touch registers.
 */
static int
ocspi_setup_transfer(struct spi_device *spi, struct spi_transfer *t)
{
	struct ocspi *ocspi;
	unsigned int speed = spi->max_speed_hz;
	unsigned int bits_per_word = spi->bits_per_word;
	int rc;
	u8 spcr, sper;

	ocspi = spi_master_get_devdata(spi->master);

	if ((t != NULL) && t->speed_hz)
		speed = t->speed_hz;

	if ((t != NULL) && t->bits_per_word)
		bits_per_word = t->bits_per_word;

	spcr = ocspi_read(ocspi, OCSPI_REG_SPCR);
	sper = ocspi_read(ocspi, OCSPI_REG_SPER);

	ocspi_set_baudrate_bits(&spcr, &sper, speed);
	ocspi_set_mode_bits(&spcr, spi->mode);

	ocspi_write(ocspi, OCSPI_REG_SPCR, spcr);
	ocspi_write(ocspi, OCSPI_REG_SPER, sper);


	return ocspi_set_transfer_size(ocspi, bits_per_word);
}

static inline void
ocspi_set_cs(struct ocspi *ocspi, int mask)
{
	ocspi_write(ocspi, OCSPI_REG_SSR, mask);
}

static inline int
ocspi_wait_till_ready(struct ocspi *ocspi)
{
	int i;
	u8 reg;

	for (i = 0; i < OCSPI_WAIT_RDY_MAX_LOOP; i++) {
		reg = ocspi_read(ocspi, OCSPI_REG_SPSR);
		if (!(reg & OCSPI_SPSR_RFEMPTY))
			return 1;
		else
			usleep_range(1,1);
			//udelay(1);
	}

	return -1;
}
#if 0
static inline int
ocspi_write_read(struct spi_device *spi,
			  const u8 **tx_buf, u8 **rx_buf, int length)
{
	struct ocspi *ocspi;
	u8 v;

	ocspi = spi_master_get_devdata(spi->master);

	/* If there's any garbage left over in the read buffer, delete it */
	while (!ocspi_read(ocspi, OCSPI_REG_SPSR) & OCSPI_SPSR_RDEMPTY) {
		ocspi_read(ocspi, OCSPI_REG_SPDR);
	}

	count = length;

	/* Write four bytes at a time and read them back */
	do {
		int i;
		i = 0
		while (count > 0 && i != 4) {
			ocspi_write(ocspi, OCSPI_REG_SPDR, *(*tx_buf)++);
			count--;
			i++;
		}

		if (ocspi_wait_till_ready(ocspi) < 0) {
			dev_err(&spi->dev, "TXS timed out\n");
			return -1;
		}		

		if (rx_buf && *rx_buf) {
			while (!ocspi_read(ocspi, OCSPI_REG_SPSR) & OCSPI_SPSR_RDEMPTY) {
				*(*rx_buf)++ = ocspi_read(ocspi, OCSPI_REG_SPDR);
		} else {
			while (!ocspi_read(ocspi, OCSPI_REG_SPSR) & OCSPI_SPSR_RDEMPTY) {
				ocspi_read(ocspi, OCSPI_REG_SPDR);
		}
	} while (count > 0);

	return length;
}
#endif
static inline int
ocspi_write_read_8bit(struct spi_device *spi,
			  const u8 **tx_buf, u8 **rx_buf)
{
	struct ocspi *ocspi;
	u8 v;

	ocspi = spi_master_get_devdata(spi->master);

	if (tx_buf && *tx_buf) {
		ocspi_write(ocspi, OCSPI_REG_SPDR, *(*tx_buf)++);
	} else {
		ocspi_write(ocspi, OCSPI_REG_SPDR, 0);
	}

	if (ocspi_wait_till_ready(ocspi) < 0) {
		dev_err(&spi->dev, "TXS timed out\n");
		return -1;
	}

	if (rx_buf && *rx_buf) {
		*(*rx_buf)++ = ocspi_read(ocspi, OCSPI_REG_SPDR);
	} else {
		ocspi_read(ocspi, OCSPI_REG_SPDR);
	}

	return 1;
}

static inline int
ocspi_write_read_16bit(struct spi_device *spi,
			   const u8 **tx_buf, u8 **rx_buf)
{
#if 0
	u8 __iomem *datareg;
	struct ocspi *ocspi;

	ocspi = spi_master_get_devdata(spi->master);
	datareg = spi_reg(ocspi, OCSPI_REG_SPDR);

	if (tx_buf && *tx_buf) {
		iowrite8(*(*tx_buf)++, datareg);
		iowrite8(*(*tx_buf)++, datareg);
	} else {
		iowrite8(0, datareg);
		iowrite8(0, datareg);
	}

	if (ocspi_wait_till_ready(ocspi) < 0) {
		dev_err(&spi->dev, "TXS timed out\n");
		return -1;
	}

	if (rx_buf && *rx_buf) {
		*(*rx_buf)++ = ioread8(datareg);
		*(*rx_buf)++ = ioread8(datareg);
	} else {
		ioread8(datareg);
		ioread8(datareg);
	}
#endif
	return 1;
}

static unsigned int
ocspi_write_read(struct spi_device *spi, struct spi_transfer *xfer)
{
        struct ocspi *ocspi;
        unsigned int count;
        int word_len;

        ocspi = spi_master_get_devdata(spi->master);
        word_len = spi->bits_per_word;
        count = xfer->len;

//	printk("Writing transfer: length = %d\n", count);

        if (word_len == 8) {
                const u8 *tx = xfer->tx_buf;
                u8 *rx = xfer->rx_buf;

                do {
                        if (ocspi_write_read_8bit(spi, &tx, &rx) < 0)
                                goto out;
                        count--;
                } while (count);
        } else if (word_len == 16) {
                const u8 *tx = xfer->tx_buf;
                u8 *rx = xfer->rx_buf;

                do {
                        if (ocspi_write_read_16bit(spi, &tx, &rx) < 0)
                                goto out;
                        count -= 2;
                } while (count);
        }

out:
        return xfer->len - count;
}

#if 0
static irqreturn_t
ocspi_interrupt(int irq, void *dev_id)
{
	/* Disable interrupt */
	ocspi_clrbits(ocspi, OCSPI_REG_CTRL, OCSPI_CTRL_IE);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t
ocspi_interrupt_thread(int irq, void *dev_id)
{
	struct ocspi *ocspi = (struct ocspi*) dev_id;

	if (current_xfer) {
		current_xfer->rx_buf 


	}

	spin_lock_irq(&ocspi->lock);
	if (list_empty(&ocspi->msq_queue)) {
		/* No more messages to send */
		spin_unlock_irq(&ocspi->lock);
		return IRQ_HANDLED;
	}

	msg = container_of(ocspi->msq_queue.next, struct spi_message, queue);

	

	/* Reenable interrupt */
	ocspi_setbits(ocspi, OCSPI_REG_CTRL, OCSPI_CTRL_IE);

	return IRQ_HANDLED;
}
#endif

static void ocspi_work(struct work_struct *work)
{
	struct ocspi *ocspi =
		container_of(work, struct ocspi, work);

	spin_lock_irq(&ocspi->lock);
	while (!list_empty(&ocspi->msg_queue)) {
		struct spi_message *m;
		struct spi_device *spi;
		struct spi_transfer *t = NULL;
		int par_override = 0;
		int status = 0;
		int cs_active = 0;

		m = container_of(ocspi->msg_queue.next, struct spi_message,
				 queue);

		list_del_init(&m->queue);
		spin_unlock_irq(&ocspi->lock);

		spi = m->spi;

		/* Load defaults */
		status = ocspi_setup_transfer(spi, NULL);

		if (status < 0)
			goto msg_done;

		list_for_each_entry(t, &m->transfers, transfer_list) {
			if (par_override || t->speed_hz || t->bits_per_word) {
				par_override = 1;
				status = ocspi_setup_transfer(spi, t);
				if (status < 0)
					break;
				if (!t->speed_hz && !t->bits_per_word)
					par_override = 0;
			}

			if (!cs_active) {
				ocspi_set_cs(ocspi, (1 << spi->chip_select));
				cs_active = 1;
			}

			if (t->len)
				m->actual_length +=
					ocspi_write_read(spi, t);

			if (t->delay_usecs)
				udelay(t->delay_usecs);

			if (t->cs_change) {
				ocspi_set_cs(ocspi, 0);
				cs_active = 0;
			}
		}

msg_done:
		if (cs_active)
			ocspi_set_cs(ocspi, 0);

		m->status = status;
		m->complete(m->context);

		spin_lock_irq(&ocspi->lock);
	}

	spin_unlock_irq(&ocspi->lock);
}

static int __init ocspi_reset(struct ocspi *ocspi)
{
	/* Verify that the CS is deasserted */
	ocspi_set_cs(ocspi, 0);

	/* Disable controller */
	ocspi_write(ocspi, OCSPI_REG_SPCR, OCSPI_SPCR_MSTR);
	/* Enable controller */
	ocspi_write(ocspi, OCSPI_REG_SPCR, OCSPI_SPCR_SPE | OCSPI_SPCR_MSTR);

	return 0;
}

/*
 * The setup function configures the spi_device for communcation via
 * this controller.  This function may be called at any time and should
 * not touch registers.
 */

static int ocspi_setup(struct spi_device *spi)
{
	struct ocspi *ocspi;

	ocspi = spi_master_get_devdata(spi->master);

	if ((spi->max_speed_hz == 0)
			|| (spi->max_speed_hz > ocspi->max_speed))
		spi->max_speed_hz = ocspi->max_speed;

	if (spi->max_speed_hz < ocspi->min_speed) {
		dev_err(&spi->dev, "setup: requested speed too low %d Hz\n",
			spi->max_speed_hz);
		return -EINVAL;
	}

	/*
	 * baudrate & width will be set by ocspi_setup_transfer
	 */
	return 0;
}

/*
 * The transfer function enqueues a message for sending to the
 * spi_device.  May be called at any time and should thus not touch
 * registers.
 */

static int ocspi_transfer(struct spi_device *spi, struct spi_message *m)
{
	struct ocspi *ocspi;
	struct spi_transfer *t = NULL;
	unsigned long flags;

	m->actual_length = 0;
	m->status = 0;

	/* reject invalid messages and transfers */
	if (list_empty(&m->transfers) || !m->complete) {
		return -EINVAL;
	}

	ocspi = spi_master_get_devdata(spi->master);

	list_for_each_entry(t, &m->transfers, transfer_list) {
		unsigned int bits_per_word = spi->bits_per_word;

		if (t->tx_buf == NULL && t->rx_buf == NULL && t->len) {
			dev_err(&spi->dev,
				"message rejected : "
				"invalid transfer data buffers\n");
			goto msg_rejected;
		}

                if ((t != NULL) && t->bits_per_word)
                        bits_per_word = t->bits_per_word;

                if ((bits_per_word != 8) && (bits_per_word != 16)) {
                        dev_err(&spi->dev,
                                "message rejected : "
                                "invalid transfer bits_per_word (%d bits)\n",
                                bits_per_word);
                        goto msg_rejected;
                }

                /*make sure buffer length is even when working in 16 bit mode*/
                if ((t != NULL) && (t->bits_per_word == 16) && (t->len & 1)) {
                        dev_err(&spi->dev,
                                "message rejected : "
                                "odd data length (%d) while in 16 bit mode\n",
                                t->len);
                        goto msg_rejected;
                }

		if (t->speed_hz && t->speed_hz < ocspi->min_speed) {
			dev_err(&spi->dev,
				"message rejected : "
				"device min speed (%d Hz) exceeds "
				"required transfer speed (%d Hz)\n",
				ocspi->min_speed, t->speed_hz);
			goto msg_rejected;
		}
	}

	spin_lock_irqsave(&ocspi->lock, flags);
	list_add_tail(&m->queue, &ocspi->msg_queue);
	queue_work(ocspi_wq, &ocspi->work);
	spin_unlock_irqrestore(&ocspi->lock, flags);

	return 0;
msg_rejected:
	/* Message rejected and not queued */
	m->status = -EINVAL;
	if (m->complete)
		m->complete(m->context);
	return -EINVAL;
}

static int __devinit ocspi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct ocspi *spi;
	struct resource *r;
	int status = 0;

	master = spi_alloc_master(&pdev->dev, sizeof *spi);
	if (master == NULL) {
		dev_dbg(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}

	if (pdev->id != -1)
		master->bus_num = pdev->id;

	master->bus_num = -1;

	/* we support only mode 0 for now, and no options... 
	 * but we can support CPHA setting -- to be implemented 
	 */
	master->mode_bits = SPI_MODE_3;

	master->setup = ocspi_setup;
	master->transfer = ocspi_transfer;
	master->num_chipselect = OCSPI_NUM_CHIPSELECTS;
#ifdef CONFIG_OF
	master->dev.of_node = pdev->dev.of_node;
#endif

	dev_set_drvdata(&pdev->dev, master);

	spi = spi_master_get_devdata(master);
	spi->master = master;

	spi->max_speed = cpuinfo.clock_frequency >> 1;
	spi->min_speed = cpuinfo.clock_frequency >> 12;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		status = -ENODEV;
		goto out;
	}

	if (!devm_request_mem_region(&pdev->dev, r->start, resource_size(r),
		                     dev_name(&pdev->dev))) {
		status = -EBUSY;
		goto out;
	}
	spi->base = devm_ioremap_nocache(&pdev->dev, r->start, resource_size(r));

	INIT_WORK(&spi->work, ocspi_work);

	spin_lock_init(&spi->lock);
	INIT_LIST_HEAD(&spi->msg_queue);

	ocspi_reset(spi);

	status = spi_register_master(master);

	printk(KERN_INFO "OpenCores Simple SPI controller (c) 2010 South Pole AB\n");

	return status;

out:
	spi_master_put(master);
	return status;
}

static int __devexit ocspi_remove(struct platform_device *pdev)
{
	struct spi_master *master;
	struct ocspi *spi;

	master = dev_get_drvdata(&pdev->dev);
	spi = spi_master_get_devdata(master);

	cancel_work_sync(&spi->work);

	spi_unregister_master(master);

	return 0;
}

static struct of_device_id ocspi_match[] = {
        {
                .compatible = "opencores,spi-simple",
        },
        {},
};
MODULE_DEVICE_TABLE(of, ocspi_match);

MODULE_ALIAS("platform:" DRIVER_NAME);

static struct platform_driver ocspi_driver = {
	.probe = ocspi_probe,
	.remove = __devexit_p(ocspi_remove),
/*	.suspend = ocspi_suspend,
	.resume = ocspi_resume,	
*/
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = ocspi_match
	}
};

static int __init ocspi_init(void)
{
	int status;

	ocspi_wq = create_singlethread_workqueue(
				ocspi_driver.driver.name);

	if (ocspi_wq == NULL)
		return -ENOMEM;

	status = platform_driver_register(&ocspi_driver);

	if (status) {
		destroy_workqueue(ocspi_wq);
	};

	return status;
}
module_init(ocspi_init);

static void __exit ocspi_exit(void)
{
	flush_workqueue(ocspi_wq);
	platform_driver_unregister(&ocspi_driver);

	destroy_workqueue(ocspi_wq);
}
module_exit(ocspi_exit);

MODULE_DESCRIPTION("OpenCores Simple SPI driver");
MODULE_AUTHOR("Jonas Bonn <jonas@southpole.se>");
MODULE_LICENSE("GPL");
