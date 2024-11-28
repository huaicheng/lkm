/*
 * kernel module for reserving physical memory
 *
 * Usage: (1). Set CMA kernel parameter in grub properly: e.g., "cma=124GB".
 * Suppose Node 0 has 128GB DRAM, then the range of available memory we can use
 * this module to achieve is [4GB, 128GB]. Thus, we should aggressively set
 * "cma=" to reserve as much physical memory for CMA as possible, to ensure a
 * small-enough lower bound for the workload with smallest memory footprint to
 * place on Node 0. This step doesn't take away the physical memory from the OS,
 * but rather, it allows the memory to be reserved later on for DMA so as to
 * remove it from the OS buddy allocator.  (2). Once the server boots up with
 * CMA reserved, load this kernel, and it will provide an interface for us to
 * reserve certain amount of memory from CMA region. For a target node 0 size
 * that we want to setup for a workload, we just need to calculate how much
 * physical memory to reserve, and do so through this module's interface:
 *
 * Once cmahog.ko module is loaded, the module creates a file "/dev/cmahog"
 * - Write to this file to reserve memory, e.g., "echo 1024 | sudo tee
 *   /dev/cmahog" will reserve 1024MB from Node 0. You can do multiple such
 *   operations to fine-tune the reserved amount of memory
 * - Writing zero (echo 0 | sudo tee /dev/cmahog) will clear all the
 *   reservations, bring Node 0's available memory to the default (eg 128GB)
 * - Reading from the file (sudo cat /dev/cmahog) undo the most recent
 *   write/reservation operation, for fine-turning
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

struct cma_hog {
	struct list_head list;
	size_t size;
	dma_addr_t dma;
	void *virt;
};

static struct device *cma_dev;
static LIST_HEAD(cma_hogs);
static DEFINE_SPINLOCK(cma_lock);

/* Push a hog/reservation request to list */
static ssize_t hog_push(struct cma_hog *h)
{
    spin_lock(&cma_lock);
    list_add(&h->list, &cma_hogs);
    spin_unlock(&cma_lock);

    return 0;
}

/* Pop the most recent hog/reservation request from list */
static ssize_t hog_pop(void)
{
    struct cma_hog *h = NULL;
    spin_lock(&cma_lock);
    if (!list_empty(&cma_hogs)) {
        h = list_first_entry(&cma_hogs, struct cma_hog, list);
        list_del(&h->list);
    }
    spin_unlock(&cma_lock);

    if (!h) {
        return -EIDRM;
    }

    dma_free_coherent(cma_dev, h->size, h->virt, h->dma);
    _dev_info(cma_dev, "CMAHOG free: CM virt: %p dma: %p size:%zuMB\n", h->virt, (void *)h->dma, h->size / SZ_1M);
    kfree(h);

    return 0;
}

static ssize_t hog_clear(void)
{
    struct cma_hog *h = NULL;

    spin_lock(&cma_lock);
    while (!list_empty(&cma_hogs)) {
        h = list_first_entry(&cma_hogs, struct cma_hog, list);
        list_del(&h->list);

        if (!h) {
            dev_err(cma_dev, "CMAHOG bug: invalid hog request");
            //return -EIDRM;
            continue;
        }

        dma_free_coherent(cma_dev, h->size, h->virt, h->dma);
        _dev_info(cma_dev, "CMAHOG free: CM virt: %p dma: %p size:%zuMB\n", h->virt, (void *)h->dma, h->size / SZ_1M);
        kfree(h);
    }
    spin_unlock(&cma_lock);

    return 0;
}

/*
 * any read request will free the last (most recent) hog request, eg. cat /dev/cmahog
 */
static ssize_t cmahog_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    hog_pop();
    return 0;
}

/*
 * any write request will alloc a new coherent memory, eg.
 * echo 1024 > /dev/cmahog will reserve 1024MB by CMA
 */
static ssize_t cmahog_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct cma_hog *h;
    unsigned long size;
    int ret;

    ret = kstrtoul_from_user(buf, count, 0, &size);
    if (ret)
        return ret;

    /* Remove all hogs */
    if (!size) {
        pr_info("CMGHOG: clear");
        hog_clear();
        return count;
    }

    if (size > ~(size_t)0 / SZ_1M)
        return -EOVERFLOW;

    h = kmalloc(sizeof(struct cma_hog), GFP_KERNEL);
    if (!h)
        return -ENOMEM;

    h->size = size * SZ_1M;
    h->virt = dma_alloc_coherent(cma_dev, h->size, &h->dma, GFP_KERNEL);

    if (!h->virt) {
        dev_err(cma_dev, "CMAHOG: No memory in CMA area\n");
        kfree(h);
        return -ENOSPC;
    }

    _dev_info(cma_dev, "CMAHOG alloc: virt: %p dma: %p size: %zuMB\n", h->virt, (void *)h->dma, h->size / SZ_1M);
    hog_push(h);

    return count;
}

static const struct file_operations cmahog_fops = {
    .owner = THIS_MODULE,
    .read  = cmahog_read,
    .write = cmahog_write,
};

static struct miscdevice cmahog_misc = {
    .name = "cmahog",
    .minor = MISC_DYNAMIC_MINOR,
    .fops = &cmahog_fops,
};

static int __init cmahog_init(void)
{
    int ret = misc_register(&cmahog_misc);

    if (unlikely(ret)) {
        pr_err("CMAHOG: Failed to register cmahog misc device!\n");
        return ret;
    }
    cma_dev = cmahog_misc.this_device;
    cma_dev->coherent_dma_mask = ~0;
    pr_info("CMAHOG: /dev/cmahog registered\n");

    return 0;
}

static void __exit cmahog_exit(void)
{
    hog_clear();

    pr_info("Deregistering misc device: %s\n", cmahog_misc.name);
    misc_deregister(&cmahog_misc);
}

module_init(cmahog_init);
module_exit(cmahog_exit);

MODULE_LICENSE("GPLv2");
MODULE_AUTHOR("MoatLab");
MODULE_DESCRIPTION("Kernel module to reserve physical memory from Node 0 in a flexible way");
MODULE_ALIAS("cmahog");
MODULE_VERSION("0.1");
