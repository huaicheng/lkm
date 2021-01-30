#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/cpumask.h>

#define sleep_millisecs (1000 * 60)

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Huaicheng Li <lhcwhu@gmail.com>");
MODULE_VERSION("v0.1");
MODULE_DESCRIPTION("kernel thread example");
MODULE_ALIAS("kthread");

int mythread(void *arg)
{
    //long ns1, ns2, delta;
    unsigned int cpu;
    //struct timespec ts1, ts2;

    cpu = *((unsigned int *)arg);
    printk(KERN_INFO "### [thread/%d] test start \n", cpu);
    while (!kthread_should_stop()) {
        schedule_timeout_interruptible(msecs_to_jiffies(1));
    }
    printk(KERN_INFO "### [thread/%d] test end \n", cpu);
    return 0;
}

static int __init kthread_init()
{
    int cpu;
    unsigned int cpu_count = num_online_cpus();
    unsigned int parameter[cpu_count];
    struct task_struct *t_thread[cpu_count];

    for_each_present_cpu(cpu) {
        parameter[cpu] = cpu;

        t_thread[cpu] = kthread_create(mythread, (void *)(parameter + cpu),
                                       "thread/%d", cpu);

        if (IS_ERR(t_thread[cpu])) {
            printk(KERN_ERR "[thread/%d]: creating kthread failed \n", cpu);
            goto out;
        }
        kthread_bind(t_thread[cpu], cpu);
        wake_up_process(t_thread[cpu]);
    }
    schedule_timeout_interruptible(msecs_to_jiffies(sleep_millisecs));

    for (cpu = 0; cpu < cpu_count; cpu++) {
        kthread_stop(t_thread[cpu]);
    }
out:
    return 0;
}

static void __exit kthread_exit()
{
    printk(KERN_INFO "kthread modules unloaded\n");
}

module_init(kthread_init);
module_exit(kthread_exit);
