/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <linux/device.h>
#include <linux/of_platform.h>

const struct of_device_id openrisc_bus_ids[] = {
        { .type = "soc", },
        { .compatible = "soc", },
        {},
};

static int __init openrisc_device_probe(void)
{
        of_platform_bus_probe(NULL, openrisc_bus_ids, NULL);
        return 0;
}
device_initcall(openrisc_device_probe);

