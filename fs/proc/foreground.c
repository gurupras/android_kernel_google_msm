#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/reboot.h>
#include <linux/kbd_kern.h>
#include <linux/proc_fs.h>
#include <linux/nmi.h>
#include <linux/quotaops.h>
#include <linux/perf_event.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/writeback.h>
#include <linux/swap.h>
#include <linux/spinlock.h>
#include <linux/vt_kern.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/oom.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include "internal.h"

#include <trace/events/phonelab.h>
#ifdef CONFIG_PHONELAB_TEMPFREQ_SEPARATE_BG_THRESHOLDS
#include "../../drivers/tempfreq/tempfreq.h"
#endif

int android_foreground_pid;

static ssize_t read_file(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int len;
	char buffer[PROC_NUMBUF];

	memset(buffer, 0, sizeof buffer);
	len = sprintf(buffer, "%d\n", android_foreground_pid);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t write_file(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int err = 0;
	char buffer[PROC_NUMBUF];
	struct task_struct *task;

	memset(buffer, 0, sizeof buffer);
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count)) {
		printk(KERN_ERR "/proc/foreground: write failed\n");
		err = -EFAULT;
		goto out;
	}

	err = kstrtoint(strstrip(buffer), 0, &android_foreground_pid);
	if(err)
		goto out;

	if(android_foreground_pid == 0) {
		task = &init_task;
	}
	else {
		task = pid_task(find_vpid(android_foreground_pid), PIDTYPE_PID);
		if(!task) {
			printk(KERN_DEBUG "/proc/foreground: Could not find task struct for PID: %05d\n", android_foreground_pid);
			err = -EINVAL;
			goto out;
		}
	}
#ifdef CONFIG_PHONELAB_TEMPFREQ_SEPARATE_BG_THRESHOLDS
	if(android_foreground_pid == 0) {
		// Change the limits..
		cancel_delayed_work_sync(&threshold_change_fg_work);
		cancel_delayed_work_sync(&threshold_change_bg_work);
		schedule_delayed_work_on(0, &threshold_change_bg_work, msecs_to_jiffies(1000));
	} else {
		cancel_delayed_work_sync(&threshold_change_fg_work);
		cancel_delayed_work_sync(&threshold_change_bg_work);
		schedule_delayed_work_on(0, &threshold_change_fg_work, msecs_to_jiffies(1000));
	}
#endif
	trace_phonelab_proc_foreground(task);
out:
	return err < 0 ? err : count;
}

static const struct file_operations proc_foreground_trigger_operations = {
	.read           = read_file,
	.write          = write_file,
};

static void foreground_init_procfs(void)
{
	if (!proc_create("foreground", S_IRUGO | S_IWUGO, NULL,
			&proc_foreground_trigger_operations))
		printk(KERN_ERR "Failed to register proc interface\n");
}

static int __init foreground_init(void)
{
	foreground_init_procfs();
	return 0;
}
module_init(foreground_init);

