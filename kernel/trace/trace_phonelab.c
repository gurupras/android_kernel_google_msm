#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/phonelab.h>
#include <linux/dcache.h>
#include <linux/uaccess.h>

#include "trace.h"

#define FREQ_BUFFER 32

static ssize_t
phonelab_freq_read(struct file *filp, char __user *ubuf, size_t cnt,
		   loff_t *ppos)
{
	int len;
	char buf[FREQ_BUFFER];
	memset(buf, 0, FREQ_BUFFER);

	len = sprintf(buf, "%u\n", periodic_ctx_switch_info_freq);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, len);
}

static ssize_t
phonelab_freq_write(struct file *filp, const char __user *ubuf, size_t cnt,
		   loff_t *ppos)
{
	int err = 0, temp = 0;
	char buf[FREQ_BUFFER];

	memset(buf, 0, FREQ_BUFFER);

	if (cnt > FREQ_BUFFER - 1)
		cnt = FREQ_BUFFER - 1;

	if (copy_from_user(buf, ubuf, cnt)) {
		printk(KERN_ERR "debugfs/periodic_ctx_switch_info_freq: write failed\n");
		err = -EFAULT;
		goto out;
	}

	err = kstrtoint(strstrip(buf), 0, &temp);
	if(err)
		goto out;

	periodic_ctx_switch_info_freq = (unsigned)temp;

out:
	return err < 0 ? err : cnt;
}

static const struct file_operations ftrace_phonelab_freq_fops = {
	.open =tracing_open_generic,
	.read = phonelab_freq_read,
	.write = phonelab_freq_write,
};

int create_phonelab_options(struct dentry *dentry)
{
	dentry = trace_create_file("frequency", 0644, dentry, NULL,
			  &ftrace_phonelab_freq_fops);
	return 0;
}
