/*
 * ocsdc.c
 *
 * Copyright (C) 2013 Marek Czerski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Written by Marek Czerski <ma.czerski@gmail.com>
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mmc/host.h>
#include <linux/io.h>

// Register space
#define OCSDC_ARGUMENT           0x00
#define OCSDC_COMMAND            0x04
#define OCSDC_RESPONSE_1         0x08
#define OCSDC_RESPONSE_2         0x0c
#define OCSDC_RESPONSE_3         0x10
#define OCSDC_RESPONSE_4         0x14
#define OCSDC_CONTROL 			 0x1C
#define OCSDC_TIMEOUT            0x20
#define OCSDC_CLOCK_DIVIDER      0x24
#define OCSDC_SOFTWARE_RESET     0x28
#define OCSDC_POWER_CONTROL      0x2C
#define OCSDC_CAPABILITY         0x30
#define OCSDC_CMD_INT_STATUS     0x34
#define OCSDC_CMD_INT_ENABLE     0x38
#define OCSDC_DAT_INT_STATUS     0x3C
#define OCSDC_DAT_INT_ENABLE     0x40
#define OCSDC_BLOCK_SIZE         0x44
#define OCSDC_BLOCK_COUNT        0x48
#define OCSDC_DST_SRC_ADDR       0x60

//command register
#define OCSDC_COMMAND_NO_RESP		0x0
#define OCSDC_COMMAND_RESP_48		0x1
#define OCSDC_COMMAND_RESP_136		0x2
#define OCSDC_COMMAND_BUSY_CHECK	0x4
#define OCSDC_COMMAND_CRC_CHECK		0x8
#define OCSDC_COMMAND_INDEX_CHECK	0x10
#define OCSDC_COMMAND_DATA_READ		0x20
#define OCSDC_COMMAND_DATA_WRITE	0x40
#define OCSDC_COMMAND_INDEX(x) ((x) << 8)

// OCSDC_CMD_INT_STATUS bits
#define OCSDC_CMD_INT_STATUS_CC   0x0001
#define OCSDC_CMD_INT_STATUS_EI   0x0002
#define OCSDC_CMD_INT_STATUS_CTE  0x0004
#define OCSDC_CMD_INT_STATUS_CCRC 0x0008
#define OCSDC_CMD_INT_STATUS_CIE  0x0010

// SDCMSC_DAT_INT_STATUS
#define SDCMSC_DAT_INT_STATUS_TRS 0x01
#define SDCMSC_DAT_INT_STATUS_CRC 0x02
#define SDCMSC_DAT_INT_STATUS_OV  0x04


struct ocsdc_dev {
	void __iomem *iobase;
	unsigned int clk_freq;
};

static inline uint32_t ocsdc_read(struct ocsdc_dev * dev, int offset)
{
#ifdef CONFIG_WISHBONE_BUS_BIG_ENDIAN
	return ioread32be(dev->iobase + offset);
#else
	return ioread32(dev->iobase + offset);
#endif
}

static inline void ocsdc_write(struct ocsdc_dev * dev, int offset, uint32_t data)
{
#ifdef CONFIG_WISHBONE_BUS_BIG_ENDIAN
	iowrite32be(data, dev->iobase + offset);
#else
	iowrite32(data, dev->iobase + offset);
#endif
}

static u32 ocsdc_get_voltage(struct ocsdc_dev * dev) {
	u32 v = ocsdc_read(dev, OCSDC_POWER_CONTROL);
	u32 voltage = 0;
	if (v >= 1650 && v <= 1950)
		voltage |= MMC_VDD_165_195;
	if (v >= 2000 && v <= 2100)
		voltage |= MMC_VDD_20_21;
	if (v >= 2100 && v <= 2200)
		voltage |= MMC_VDD_21_22;
	if (v >= 2200 && v <= 2300)
		voltage |= MMC_VDD_22_23;
	if (v >= 2300 && v <= 2400)
		voltage |= MMC_VDD_23_24;
	if (v >= 2400 && v <= 2500)
		voltage |= MMC_VDD_24_25;
	if (v >= 2500 && v <= 2600)
		voltage |= MMC_VDD_25_26;
	if (v >= 2600 && v <= 2700)
		voltage |= MMC_VDD_26_27;
	if (v >= 2700 && v <= 2800)
		voltage |= MMC_VDD_27_28;
	if (v >= 2800 && v <= 2900)
		voltage |= MMC_VDD_28_29;
	if (v >= 2900 && v <= 3000)
		voltage |= MMC_VDD_29_30;
	if (v >= 3000 && v <= 3100)
		voltage |= MMC_VDD_30_31;
	if (v >= 3100 && v <= 3200)
		voltage |= MMC_VDD_31_32;
	if (v >= 3200 && v <= 3300)
		voltage |= MMC_VDD_32_33;
	if (v >= 3300 && v <= 3400)
		voltage |= MMC_VDD_33_34;
	if (v >= 3400 && v <= 3500)
		voltage |= MMC_VDD_34_35;
	if (v >= 3500 && v <= 3600)
		voltage |= MMC_VDD_35_36;
	return voltage;
}

/* Set clock divider value based on the required clock in HZ */
static void ocsdc_set_clock(struct ocsdc_dev * dev, unsigned int clock)
{
	int clk_div = dev->clk_freq / (2 * clock) - 1;
	if (clk_div < 0)
		clk_div = 0;

	printk("ocsdc_set_clock %d, div %d\n", clock, clk_div);
	//software reset
	ocsdc_write(dev, OCSDC_SOFTWARE_RESET, 1);
	//set clock devider
	ocsdc_write(dev, OCSDC_CLOCK_DIVIDER, clk_div);
	//clear software reset
	ocsdc_write(dev, OCSDC_SOFTWARE_RESET, 0);
}

/* Initialize ocsdc controller */
static int ocsdc_init(struct ocsdc_dev * dev)
{

	//set timeout
	ocsdc_write(dev, OCSDC_TIMEOUT, 0x7FFF);
	//disable all interrupts
	ocsdc_write(dev, OCSDC_CMD_INT_ENABLE, 0);
	ocsdc_write(dev, OCSDC_DAT_INT_ENABLE, 0);
	//clear all interrupts
	ocsdc_write(dev, OCSDC_CMD_INT_STATUS, 0);
	ocsdc_write(dev, OCSDC_DAT_INT_STATUS, 0);
	//set clock to maximum
	ocsdc_set_clock(dev, dev->clk_freq/2);

	return 0;
}

static void ocsdc_set_buswidth(struct ocsdc_dev * dev, unsigned char width) {
	if (width == MMC_BUS_WIDTH_4)
		ocsdc_write(dev, OCSDC_CONTROL, 1);
	else if (width == MMC_BUS_WIDTH_1)
		ocsdc_write(dev, OCSDC_CONTROL, 0);
	else
		printk("ocsdc_set_buswidth %x\n", (unsigned)width);
}

static uint32_t ocsdc_prepare_cmd(struct mmc_request *mrq) {
	uint32_t command = OCSDC_COMMAND_INDEX(mrq->cmd->opcode);
	if (mrq->cmd->flags & MMC_RSP_PRESENT) {
		if (mrq->cmd->flags & MMC_RSP_136)
			command |= OCSDC_COMMAND_RESP_136;
		else {
			command |= OCSDC_COMMAND_RESP_48;
		}
	}
	if (mrq->cmd->flags & MMC_RSP_BUSY)
		command |= OCSDC_COMMAND_BUSY_CHECK;
	if (mrq->cmd->flags & MMC_RSP_CRC)
		command |= OCSDC_COMMAND_CRC_CHECK;
	if (mrq->cmd->flags & MMC_RSP_OPCODE)
		command |= OCSDC_COMMAND_INDEX_CHECK;

	if (mrq->data && ((mrq->data->flags & MMC_DATA_READ) || ((mrq->data->flags & MMC_DATA_WRITE))) && mrq->data->blocks) {
		if (mrq->data->flags & MMC_DATA_READ)
			command |= OCSDC_COMMAND_DATA_READ;
		if (mrq->data->flags & MMC_DATA_WRITE)
			command |= OCSDC_COMMAND_DATA_WRITE;
//		ocsdc_setup_data_xfer(dev, cmd, data);
	}
	return command;
}

static void ocsdc_cmd_finish(struct ocsdc_dev * dev, struct mmc_command *cmd) {

	while (1) {
		int int_stat = ocsdc_read(dev, OCSDC_CMD_INT_STATUS);
		//debug("ocsdc_finish: cmd %d, status %x\n", cmd->cmdidx, r2);
		if (int_stat & OCSDC_CMD_INT_STATUS_EI) {
			//clear interrupts
			ocsdc_write(dev, OCSDC_CMD_INT_STATUS, 0);
			if (int_stat & OCSDC_CMD_INT_STATUS_CTE)
				cmd->error = -ETIMEDOUT;
			else if (int_stat & OCSDC_CMD_INT_STATUS_CCRC)
				cmd->error = -EILSEQ;
			else if (int_stat & OCSDC_CMD_INT_STATUS_CIE)
				cmd->error = -EILSEQ;
			printk("ocsdc_cmd_finish: cmd %d, status %x\n", cmd->opcode, int_stat);
			break;
		}
		else if (int_stat & OCSDC_CMD_INT_STATUS_CC) {
			//clear interrupts
			ocsdc_write(dev, OCSDC_CMD_INT_STATUS, 0);
			//get response
			cmd->resp[0] = ocsdc_read(dev, OCSDC_RESPONSE_1);
			if (cmd->flags & MMC_RSP_136) {
				cmd->resp[1] = ocsdc_read(dev, OCSDC_RESPONSE_2);
				cmd->resp[2] = ocsdc_read(dev, OCSDC_RESPONSE_3);
				cmd->resp[3] = ocsdc_read(dev, OCSDC_RESPONSE_4);
			}
			printk("ocsdc_cmd_finish:  %d ok\n", cmd->opcode);
			break;
		}
		cpu_relax();
		//else if (!(int_stat & OCSDC_CMD_INT_STATUS_CIE)) {
		//	debug("ocsdc_finish: cmd %d no exec %x\n", cmd->cmdidx, r2);
		//}
	}
	return;
}

static void ocsdc_request(struct mmc_host *mmc, struct mmc_request *mrq) {
	unsigned int command;
	struct ocsdc_dev * dev = mmc_priv(mmc);

	printk("ocsdc_request\n");

	printk("sbc %p, cmd %p, data %p, stop %p\n", mrq->sbc, mrq->cmd, mrq->data, mrq->stop);

	if (mrq->data)
		goto ERROR;

	command = ocsdc_prepare_cmd(mrq);

	printk("ocsdc_send_cmd %04x\n", command);

	ocsdc_write(dev, OCSDC_COMMAND, command);
	ocsdc_write(dev, OCSDC_ARGUMENT, mrq->cmd->arg);

	ocsdc_cmd_finish(dev, mrq->cmd);

	mmc_request_done(mmc, mrq);

	return;

ERROR:
	mrq->cmd->error = -EIO;
	mmc_request_done(mmc, mrq);
}

static void ocsdc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios) {
	struct ocsdc_dev * dev = mmc_priv(mmc);

	printk("ocsdc_set_ios\n");

	if (ios->power_mode != MMC_POWER_ON)
		return;

	if (ios->clock)
		ocsdc_set_clock(dev, ios->clock);

	printk("ios->vdd %x\n", (unsigned)ios->vdd);
	printk("ios->power_mode %x\n", (unsigned)ios->power_mode);

	ocsdc_set_buswidth(dev, ios->bus_width);

	printk("ios->timing %x\n", (unsigned)ios->timing);
	printk("ios->signal_voltage %x\n", (unsigned)ios->signal_voltage);
	printk("ios->drv_type %x\n", (unsigned)ios->drv_type);
}

static void ocsdc_enable_sdio_irq(struct mmc_host *mmc, int enable) {
	printk("ocsdc_enable_sdio_irq\n");
}

static const struct mmc_host_ops ocsdc_ops = {
	.request	= ocsdc_request,
	.set_ios	= ocsdc_set_ios,
	.get_cd		= NULL,
	.get_ro		= NULL,
	.enable_sdio_irq = ocsdc_enable_sdio_irq
};

static int __init ocsdc_probe(struct platform_device *pdev)
{
	int ret;
	struct mmc_host * mmc;
	struct ocsdc_dev * dev;
	struct resource *res;
	struct resource *mmio;

	mmc = mmc_alloc_host(sizeof(struct ocsdc_dev), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto MMC_ALLOC_HOST_FAIL;
	}

	dev = mmc_priv(mmc);
	dev->clk_freq = 50000000;

	/* obtain I/O memory space */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "cannot obtain I/O memory space\n");
		ret = -ENXIO;
		goto IORESOURCE_MEM_FAIL;
	}

	mmio = devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), res->name);
	if (!mmio) {
		dev_err(&pdev->dev, "cannot request I/O memory space\n");
		ret = -ENXIO;
		goto IORESOURCE_MEM_FAIL;
	}

	dev->iobase = devm_ioremap_nocache(&pdev->dev, mmio->start, resource_size(mmio));
	if (!dev->iobase) {
		dev_err(&pdev->dev, "cannot remap I/O memory space\n");
		ret = -ENXIO;
		goto IOREMAP_FAIL;
	}

	ocsdc_init(dev);

	mmc->ops = &ocsdc_ops;
	mmc->f_min = dev->clk_freq/6;
	mmc->f_max = dev->clk_freq/2;
	mmc->caps = MMC_CAP_4_BIT_DATA;
	mmc->caps2 = 0;
	mmc->max_segs = 1;
	mmc->max_blk_size = (1 << 11);
	mmc->max_blk_count = (1 << 16) - 1;
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;
	mmc->max_seg_size = mmc->max_req_size;
	mmc->ocr_avail = ocsdc_get_voltage(dev);

	ret = mmc_add_host(mmc);
	if (ret < 0)
		goto MMC_ADD_HOST_FAIL;

	platform_set_drvdata(pdev, mmc);

	printk("ocsdc_probe\n");

	return 0;

MMC_ADD_HOST_FAIL:
	devm_iounmap(&pdev->dev, dev->iobase);
IOREMAP_FAIL:
	devm_release_region(&pdev->dev, mmio->start, resource_size(mmio));
IORESOURCE_MEM_FAIL:
	mmc_free_host(mmc);

MMC_ALLOC_HOST_FAIL:
	return ret;
}

static int __exit ocsdc_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	if (mmc) {
		struct ocsdc_dev * dev;

		mmc_remove_host(mmc);

		dev = mmc_priv(mmc);
		devm_iounmap(&pdev->dev, dev->iobase);
		mmc_free_host(mmc);
	}

	printk("ocsdc_remove\n");
	return 0;
}

#ifdef CONFIG_PM
static int ocsdc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return -ENOSYS;
}

static int ocsdc_resume(struct platform_device *pdev)
{
	return -ENOSYS;
}
#else
# define ocsdc_suspend NULL
# define ocsdc_resume  NULL
#endif

static struct of_device_id ocsdc_match[] = {
	{ .compatible = "opencores,ocsdc" },
	{},
};
MODULE_DEVICE_TABLE(of, ocsdc_match);

static struct platform_driver ocsdc_driver = {
	.probe   = ocsdc_probe,
	.remove  = ocsdc_remove,
	.suspend = ocsdc_suspend,
	.resume  = ocsdc_resume,
	.driver  = {
		.name = "ocsdc",
		.owner = THIS_MODULE,
		.of_match_table = ocsdc_match,
	},
};

module_platform_driver(ocsdc_driver);

MODULE_AUTHOR("Marek Czerski");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Wishbone SD Card Controller IP Core driver");

