/*-----------------------------------------------------------------------------
 * Example for the kernel thread API
 *  - create a simple thread when the module is loaded
 *  - stop the thread when the modules is unloaded
 *
 * file: kthread0.c
 *
 * coperdli, Aug/2013
 *-----------------------------------------------------------------------------*/


#include <linux/module.h>	/* Specifically, a module */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/init.h>         /* modules_init/_exit macros */
#include <linux/kthread.h>	/* kthread_run*/
#include <linux/sched.h>	/* task_struct*/
#include <linux/delay.h>	/* mdelay()*/
/*
 * debug macro
 */
#define DEBUG

#ifdef DEBUG
#define DPRINT(fmt,args...) printk(KERN_INFO "%s,%i:" fmt "\n", \
                            __FUNCTION__, __LINE__,##args);
#else
#define DPRINT(fmt,args...)
#endif

MODULE_LICENSE("GPL v2");
/*
 * the kernel thread
 */
int kthread_fct(void *data)
{
	while(1) {
		DPRINT("kernel thread");
		//msleep(500);
		mdelay(500);
		msleep(1);
		if (kthread_should_stop())
			break;
	}
	return 0;
}

struct task_struct *ts;

/*
 * module init/exit functions
 */
int __init kthr_init(void)
{
	DPRINT("init_module() called\n");
	ts=kthread_run(kthread_fct,NULL,"kthread");
	return 0;
}

void __exit kthr_exit(void)
{
	DPRINT("exit_module() called\n");
	kthread_stop(ts);
}

module_init(kthr_init);
module_exit(kthr_exit);
