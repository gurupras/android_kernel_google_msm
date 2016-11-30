#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/syscore_ops.h>
#include <linux/cgroup.h>

#include <linux/netlink.h>
#include <net/sock.h>
#include <net/net_namespace.h>

#include <trace/events/tempfreq.h>

#include <linux/phonelab.h>

#include "tempfreq.h"
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
#include "netlink.h"
#endif

static void cpuset_netlink_send(char *cpuset, int pid)
{
	// We need to create a netlink_cmd and send it up
	struct netlink_cmd cmd;
	char *_cmd = "cpuset";
	char args[36];
	int len;

	memset(&cmd, 0, sizeof(struct netlink_cmd));

	cmd.cmd_len = strlen(_cmd);
	strncpy(cmd.cmd, _cmd, strlen(_cmd));

	len = sprintf(args, "%s %d", cpuset, pid);
	cmd.args_len = len;
	strncpy(cmd.args, args, len);
	printk(KERN_DEBUG "tempfreq: Sending cpuset command: PID: %05d\n", pid);
}

static void handle(char *cgroup, int pid)
{
	char cs_prefix[32];
	struct cgroup *cgrp = NULL;
	struct cgroup *cs = NULL;
	struct task_struct *tsk = NULL;

	sprintf(cs_prefix, "cs_%s", cgroup);

	if(strcmp(cgroup, "default") == 0) {
		cgrp = bg_non_interactive->top_cgroup;
		cs = cs_cgroup(&top_cpuset);
	} else if(strcmp(cgroup, "bg_non_interactive") == 0) {
		cgrp = bg_non_interactive;
		cs = cs_bg_non_interactive;
	} else if(strcmp(cgroup, "fg_bg") == 0) {
		cgrp = fg_bg;
		cs = cs_fg_bg;
	} else if(strcmp(cgroup, "delay_tolerant") == 0) {
		cgrp = delay_tolerant;
		cs = cs_delay_tolerant;
	}

//	tsk = pid_task(find_vpid(pid), PIDTYPE_PID);
//	if(tsk == NULL || cs == NULL) {
//		printk(KERN_DEBUG "tempfreq: Unable to handle '%d > %s'\n", pid, cgroup);
//		return;
//	}
//	cgroup_attach_task(cs, tsk);
	(void) tsk;
	(void) cgrp;
	(void) cpuset_netlink_send;
}

static ssize_t common_cpuset_handle(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;
	if(val < 0) {
		err = -EINVAL;
	}

	goto out;
out:
	kfree(buf);
	return err == 0 ? val : err;
}

#define DEFINE_CPUSET(name)						\
static ssize_t store_cs_##name(const char *_buf, size_t count)		\
{									\
	int pid = common_cpuset_handle(_buf, count);			\
	if(pid < 0) {							\
		return -EINVAL;						\
	}								\
	handle(#name, pid);						\
	return count;							\
}									\
struct tempfreq_attr cs_attr_##name =					\
__ATTR(cs_##name, 0222, NULL, store_cs_##name);

DEFINE_CPUSET(default);
DEFINE_CPUSET(bg_non_interactive);
DEFINE_CPUSET(fg_bg);
DEFINE_CPUSET(delay_tolerant);


