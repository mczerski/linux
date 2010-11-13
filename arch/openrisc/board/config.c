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

#include <linux/device.h>
#include <linux/of_platform.h>

const struct of_device_id or32_bus_ids[] = {
        { .type = "soc", },
        { .compatible = "soc", },
        {},
};

static int __init or32_device_probe(void)
{
        of_platform_bus_probe(NULL, or32_bus_ids, NULL);
        return 0;
}
device_initcall(or32_device_probe);

