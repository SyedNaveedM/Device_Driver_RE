#include "device_file.h"
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/atomic.h>

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);


static atomic_t data_ready = ATOMIC_INIT(0);

static char kernel_buffer[1024];
static size_t data_size = 0;

static struct task_struct *worker_thread;

/* 0 = blocking, 1 = busy-wait */
static int mode = 0;

/* ---------------- WORKER THREAD ---------------- */

static int worker_fn(void *data)
{
    while (!kthread_should_stop())
    {
        pr_notice("Worker: Starting work cycle...\n");

        /*
         * Reset data_ready to 0 HERE, at the start of each
         * work cycle, BEFORE doing the long computation.
         *
         * Previously it was reset inside the reader, which created a
         * race: the reader could reset it after the worker had already
         * set it to 1 for the next cycle, causing the reader to spin
         * forever (or sleep forever) waiting for data that had already
         * been produced.
         */
        atomic_set(&data_ready, 0);

        /* Simulate a long computation (~200M iterations). */
        for (long i = 0; i < 200000000L; i++)
            cpu_relax();

        /* Produce the data. */
        strscpy(kernel_buffer, "This is delayed data from kernel\n",
                sizeof(kernel_buffer));
        data_size = strlen(kernel_buffer);

        /*
         * Ensure the buffer write is visible to other CPUs before we
         * set the flag.  smp_store_release / atomic_set with the right
         * barrier would be ideal; a plain smp_mb() is conservative but
         * correct here.
         */
        smp_mb();

        /* Set the flag and wake any sleepers. */
        atomic_set(&data_ready, 1);
        wake_up_interruptible(&wait_queue);

        /*
         * Small gap so the reader has time to consume the data before
         * the worker resets data_ready at the top of the next cycle.
         */
        msleep(100);
    }

    return 0;
}

/* ---------------- READ ---------------- */

static ssize_t device_file_read(
    struct file *file_ptr,
    char __user *user_buffer,
    size_t count,
    loff_t *position)
{
    pr_notice("Driver: Read called (mode=%d)\n", mode);

    if (mode == 1)
    {
        /*
         * BUSY-WAIT MODE
         *
         * Use atomic_read() so the compiler is forced to reload
         * data_ready from memory on every iteration.  Without this the
         * loop body may be optimised into an infinite loop that never
         * re-reads the variable.
         *
         * With the worker resetting data_ready at the top of its cycle
         * this loop now correctly spins while the worker
         * is computing and exits as soon as the worker signals.  This
         * will show ~100% CPU on one core, which is the expected and
         * correct behaviour for busy-wait.
         */
        pr_notice("Busy-wait mode\n");

        while (!atomic_read(&data_ready))
            cpu_relax();
    }
    else
    {
        /*
         * BLOCKING MODE
         *
         * wait_event_interruptible puts the task to sleep until
         * data_ready becomes 1.  CPU usage will be ~0% while waiting,
         * which is the correct behaviour for blocking I/O.
         */
        pr_notice("Blocking mode\n");

        if (wait_event_interruptible(wait_queue,
                                     atomic_read(&data_ready) == 1))
            return -ERESTARTSYS;
    }

    pr_notice("Driver: Sending data to user\n");

    if (*position >= data_size)
        return 0;

    if (*position + count > data_size)
        count = data_size - *position;

    if (copy_to_user(user_buffer, kernel_buffer + *position, count))
        return -EFAULT;

    *position += count;


    return count;
}

/* ---------------- WRITE (MODE SWITCH) ---------------- */

static ssize_t device_file_write(
    struct file *file_ptr,
    const char __user *user_buffer,
    size_t count,
    loff_t *position)
{
    char kbuf[2];

    if (count < 1)
        return -EINVAL;

    if (copy_from_user(kbuf, user_buffer, 1))
        return -EFAULT;

    if (kbuf[0] == '0')
    {
        mode = 0;
        pr_notice("Switched to BLOCKING mode\n");
    }
    else if (kbuf[0] == '1')
    {
        mode = 1;
        pr_notice("Switched to BUSY-WAIT mode\n");
    }
    else
    {
        pr_warn("Driver: unknown mode '%c', use '0' or '1'\n", kbuf[0]);
        return -EINVAL;
    }

    return count;
}

/* ---------------- FILE OPS ---------------- */

static const struct file_operations simple_driver_fops =
{
    .owner = THIS_MODULE,
    .read  = device_file_read,
    .write = device_file_write,
};

/* ---------------- DEVICE SETUP ---------------- */

static const char device_name[] = "blocking-driver";
static const char class_name[]  = "blocking-driver-class";

dev_t g_devno = 0;
struct cdev g_cdev;
static struct class  *g_class  = NULL;
static struct device *g_device = NULL;

int register_device(void)
{
    int result;

    pr_notice("Driver: register_device()\n");

    result = alloc_chrdev_region(&g_devno, 0, 1, device_name);
    if (result)
        return result;

    cdev_init(&g_cdev, &simple_driver_fops);
    g_cdev.owner = THIS_MODULE;

    result = cdev_add(&g_cdev, g_devno, 1);
    if (result)
        goto err_unregister;

    g_class = class_create(class_name);
    if (IS_ERR(g_class))
    {
        result = PTR_ERR(g_class);
        goto err_cdev;
    }

    g_device = device_create(g_class, NULL, g_devno, NULL, device_name);
    if (IS_ERR(g_device))
    {
        result = PTR_ERR(g_device);
        goto err_class;
    }

    /* Start the worker thread. */
    worker_thread = kthread_run(worker_fn, NULL, "worker_thread");
    if (IS_ERR(worker_thread))
    {
        result = PTR_ERR(worker_thread);
        worker_thread = NULL;
        goto err_device;
    }

    pr_notice("Driver: Registered (%d:%d)\n",
              MAJOR(g_devno), MINOR(g_devno));
    return 0;

err_device:
    device_destroy(g_class, g_devno);
err_class:
    class_destroy(g_class);
err_cdev:
    cdev_del(&g_cdev);
err_unregister:
    unregister_chrdev_region(g_devno, 1);
    return result;
}

void unregister_device(void)
{
    pr_notice("Driver: unregister_device()\n");

    if (worker_thread)
        kthread_stop(worker_thread);

    device_destroy(g_class, g_devno);
    class_destroy(g_class);
    cdev_del(&g_cdev);
    unregister_chrdev_region(g_devno, 1);
}