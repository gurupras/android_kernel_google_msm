#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/kernel_stat.h>
#include <asm/cputime.h>

static int power_agile_inefficiency_components_proc_show(struct seq_file *m, void *v)
{
	char buf[32];
	int ret;
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER_CPU
	ret = snprintf(buf, sizeof buf, "cpu %d", CPU_MAX_INEFFICIENCY);
	if(ret < 0)
		return -EINVAL;
#endif
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER_MEM
	ret = snprintf(buf, sizeof buf, "%s mem %d", buf, MEM_MAX_INEFFICIENCY);
	if(ret < 0)
		return -EINVAL;
#endif
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER_NET
	ret = snprintf(buf, sizeof buf, "%s net 1", buf);	//XXX: Hack to get network in
	if(ret < 0)
		return -EINVAL;
#endif
	ret = seq_printf(m, "%s\n", buf);
	if(ret)
		return -EINVAL;
	return 0;
}

static int power_agile_inefficiency_components_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, power_agile_inefficiency_components_proc_show, NULL);
}

static const struct file_operations power_agile_inefficiency_components_proc_fops = {
	.open		= power_agile_inefficiency_components_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_power_agile_inefficiency_components_init(void)
{
	if(!proc_create("power_agile_inefficiency_components", 0, NULL, &power_agile_inefficiency_components_proc_fops))
		printk(KERN_ERR "Failed to register proc interface\n");

	return 0;
}
module_init(proc_power_agile_inefficiency_components_init);
