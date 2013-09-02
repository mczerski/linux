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

static int __init ocsdc_probe(struct platform_device *pdev)
{
	printk("ocsdc_probe\n");
	return 0;
}

static int __exit ocsdc_remove(struct platform_device *pdev)
{
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

