/*
 *  linux/arch/or32/board/config.c
 *
 *  or32 version
 *    author(s): Simon Srot (srot@opencores.org)
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Based on m68knommu/platform/xx/config.c
 */

#include <stdarg.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/serial_8250.h>
#include <linux/if.h>
#include <net/ethoc.h>
#include <linux/of_platform.h>

#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/serial.h>
#include <asm/cpuinfo.h>

#if 0
#define UART_IRQ       2
#define UART_IOBASE    0x90000000

#define ETHOC_IRQ      4
#define ETHOC_IOBASE   0x92000000

static struct resource ethoc_resources[] = {
        {
                .start  = ETHOC_IOBASE,
                .end    = ETHOC_IOBASE + 0x53,
                .flags  = IORESOURCE_MEM,
        }, {
                .start  = ETHOC_IRQ,
                .end    = ETHOC_IRQ,
                .flags  = IORESOURCE_IRQ,
        }
};

static struct ethoc_platform_data ethoc_platdata = {
	.hwaddr = { 0, },
	.phy_id = -1
};

static struct platform_device ethoc_device = {
	.name			= "ethoc",
	.dev			= {
		.platform_data  = &ethoc_platdata
	},
        .resource       = ethoc_resources,
        .num_resources  = ARRAY_SIZE(ethoc_resources),
};

static struct plat_serial8250_port serial_platform_data[] = {
	{
		.mapbase	= UART_IOBASE,
		.irq		= UART_IRQ,
		.uartclk	= BASE_BAUD*16,
		.regshift	= 0,
		.iotype		= UPIO_MEM,
		.flags		= UPF_IOREMAP | UPF_BOOT_AUTOCONF,
	},
	{ },
};

static struct platform_device serial_device = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev			= {
		.platform_data	= serial_platform_data,
	},
};


static void __init or1200_register_platform_devices(void)
{
//	platform_device_register(&serial_device);
/*	platform_device_register(&ethoc_device);*/
}
arch_initcall(or1200_register_platform_devices);
#endif

const struct of_device_id or32_bus_ids[] = {
        { .type = "soc", },
        { .compatible = "soc", },
        {},
};

static int __init or32_device_probe(void)
{
	printk("JONASSSSSSSSSSSSSSSSSSSSSSS!!!!!");

        of_platform_bus_probe(NULL, or32_bus_ids, NULL);
/*        of_platform_reset_gpio_probe();*/
        return 0;
}
device_initcall(or32_device_probe);

