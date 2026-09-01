/*
 * scull.h -- definitions for the scull char module
 *
 * Based on the "scull" example driver from Linux Device Drivers, 3rd
 * Edition, Chapter 3 ("Char Drivers"), by Alessandro Rubini, Jonathan
 * Corbet, and Greg Kroah-Hartman, O'Reilly & Associates.
 *
 * This implements only scull0-scull3: four global, persistent memory
 * devices. The other personalities described in the book (scullpipe,
 * scullsingle, scullpriv, sculluid, scullwuid) belong to Chapter 6 and
 * are intentionally not part of this chapter-3 implementation.
 */

#ifndef _SCULL_H_
#define _SCULL_H_

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>

#undef PDEBUG
#ifdef SCULL_DEBUG
#  ifdef __KERNEL__
#    define PDEBUG(fmt, args...) printk(KERN_DEBUG "scull: " fmt, ##args)
#  else
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

/*
 * Macros to help debugging
 */

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0   /* dynamic major by default */
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 4    /* scull0 through scull3 */
#endif

/*
 * The bare device is a variable-length region of memory.
 * Use a linked list of indirect blocks.
 *
 * "scull_dev->data" points to an array of pointers, each
 * pointer refers to a memory area of SCULL_QUANTUM bytes.
 *
 * The array (quantum-set) is SCULL_QSET long.
 */
#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000
#endif

#ifndef SCULL_QSET
#define SCULL_QSET    1000
#endif

/*
 * Representation of scull quantum sets.
 */
struct scull_qset {
	void **data;
	struct scull_qset *next;
};

struct scull_dev {
	struct scull_qset *data;  /* Pointer to first quantum set */
	int quantum;              /* the current quantum size */
	int qset;                 /* the current array size */
	unsigned long size;       /* amount of data stored here */
	unsigned int access_key;  /* used by sculluid and scullpriv */
	struct semaphore sem;     /* mutual exclusion semaphore */
	struct cdev cdev;         /* Char device structure */
};

/*
 * Split minors in two parts
 */
#define TYPE(minor)   (((minor) >> 4) & 0xf)  /* high nibble */
#define NUM(minor)    ((minor) & 0xf)         /* low nibble */

/*
 * The different configurable parameters
 */
extern int scull_major;
extern int scull_minor;
extern int scull_nr_devs;
extern int scull_quantum;
extern int scull_qset;

/*
 * Prototypes for shared functions
 */
int scull_trim(struct scull_dev *dev);

int scull_open(struct inode *inode, struct file *filp);
int scull_release(struct inode *inode, struct file *filp);
ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
loff_t scull_llseek(struct file *filp, loff_t off, int whence);

#endif /* _SCULL_H_ */
