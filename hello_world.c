#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/percpu.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("coperd <lhcwhu@gmail.com>");
MODULE_VERSION("v0.1");
MODULE_DESCRIPTION("hell wolrd kernel module");
MODULE_ALIAS("a simple example");

static int __init hello_init(void)
{
    int cpu_id;
    unsigned int *p = 0;

    printk(KERN_ALERT "Hello, kernel world!\n");
    cpu_id = get_cpu();
    printk(KERN_ALERT "Now I'm in the cpu of %d\n", cpu_id);

    *p = 1;

    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_ALERT "Goodbye, kernel world!\n");
}

module_init(hello_init);
module_exit(hello_exit);

