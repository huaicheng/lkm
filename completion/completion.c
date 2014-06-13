#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h> /* printk() */

#include <linux/sched.h> /* current and everything */
#include <linux/fs.h>  /* everything... */
#include <linux/types.h> /* size_t */
#include <linux/completion.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("coperd <lhcwhu@gmail.com>");
MODULE_VERSION("v0.1");
MODULE_DESCRIPTION("an example of kernel completion interface");
MODULE_ALIAS("a simple completion example");

static int complete_major = 253; // the main device number

DECLARE_COMPLETION(work);

ssize_t complete_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    printk(KERN_DEBUG "process %i(%s) going to sleep\n", current->pid,
            current->comm);
    wait_for_completion(&work);
    printk(KERN_DEBUG "awoken %i(%s)\n", current->pid, current->comm);

    return 0;
}

ssize_t complete_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    printk(KERN_DEBUG "process %i(%s) awakening the readers...\n", current->pid, 
            current->comm);
    complete(&work);
    
    return count;
}

struct file_operations complete_fops = {
    .owner = THIS_MODULE,
    .read = complete_read,
    .write = complete_write,
};

int __init complete_init()
{
    int result;

    result = register_chrdev(complete_major, "complete", &complete_fops);
    if (result < 0)
        complete_major = result;
    return 0;
}

void __exit complete_cleanup()
{
    unregister_chrdev(complete_major, "complete");
}

module_init(complete_init);
module_exit(complete_cleanup);
