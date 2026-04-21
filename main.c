#include "device_file.h"
#include <linux/init.h>
#include <linux/module.h>

MODULE_DESCRIPTION("Blocking Linux driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Syed Naveed");

static int simple_driver_init(void)
{
    int result;

    pr_notice("Driver: Init\n");

    result = register_device();
    if (result)
        return result;

    result = start_worker_thread();
    if (result)
    {
        unregister_device();
        return result;
    }

    return 0;
}

static void simple_driver_exit(void)
{
    pr_notice("Driver: Exit\n");

    stop_worker_thread();
    unregister_device();
}

module_init(simple_driver_init);
module_exit(simple_driver_exit);