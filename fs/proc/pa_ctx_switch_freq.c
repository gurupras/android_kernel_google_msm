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
#include <linux/power_agile.h>
#include "internal.h"

int pa_default_cpu_freq, pa_default_mem_freq;

static ssize_t read_file(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int len;
	char buffer[PROC_NUMBUF];

	memset(buffer, 0, sizeof buffer);
	len = sprintf(buffer, "%d\n", is_pa_ctx_switch_freq);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t write_file(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int err = 0;
	char buffer[PROC_NUMBUF];
	int val;

	memset(buffer, 0, sizeof buffer);
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count)) {
		printk(KERN_ERR "pa_ctx_switch_freq: write failed\n");
		err = -EFAULT;
		goto out;
	}

	err = kstrtoint(strstrip(buffer), 0, &val);

	if(val == 0)
		is_pa_ctx_switch_freq = 0;
	if(val == 1)
		is_pa_ctx_switch_freq = 1;
	else
		err = -EINVAL;
out:
	return err < 0 ? err : count;
}

static const struct file_operations proc_pa_ctx_switch_freq_trigger_operations = {
	.read		= read_file,
	.write		= write_file,
};

static void pa_ctx_switch_freq_init_procfs(void)
{
	if (!proc_create("pa_ctx_switch_freq", S_IRUGO | S_IWUSR, NULL,
			 &proc_pa_ctx_switch_freq_trigger_operations))
		printk(KERN_ERR "Failed to register proc interface\n");
}

static int __init pa_ctx_switch_freq_init(void)
{
	pa_ctx_switch_freq_init_procfs();
	return 0;
}
module_init(pa_ctx_switch_freq_init);
