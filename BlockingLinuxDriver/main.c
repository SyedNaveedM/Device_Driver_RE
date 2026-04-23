#include "device_file.h"
#include <linux/init.h>
#include <linux/module.h>

MODULE_DESCRIPTION("Blocking Linux driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Syed Naveed");

static int simple_driver_init(void)
{
    pr_notice("Driver: Init\n");
    return register_device();
}

static void simple_driver_exit(void)
{
    pr_notice("Driver: Exit\n");
    unregister_device();
}

module_init(simple_driver_init);
module_exit(simple_driver_exit);