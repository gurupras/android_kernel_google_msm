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

#include <linux/netlink.h>
#include <net/sock.h>
#include <net/net_namespace.h>

#include <trace/events/tempfreq.h>

#include <linux/phonelab.h>

#include "tempfreq.h"

static int initialized = 0;
int phonelab_tempfreq_mpdecision_coexist_enable = 1;
int phonelab_tempfreq_mpdecision_blocked = 0;
void start_bg_core_control(void);
void stop_bg_core_control(void);
struct cgroup *fg_bg, *bg_non_interactive, *delay_tolerant;

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
#define NETLINK_MPDECISION_COEXIST 14
static struct sock *netlink_sk = NULL;
static void netlink_send(char *msg);
static void netlink_recv(struct sk_buff *skb);
#endif

int mpdecision_coexist_cpu = 0;
inline __cpuinit void start_bg_core_control(void)
{
	struct cpufreq_policy *policy;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif
	// cpu_hotplug_driver is already locked
	if(!initialized || phonelab_tempfreq_mpdecision_blocked) {
		goto out;
	}

	trace_tempfreq_mpdecision_blocked(1);

	phonelab_tempfreq_mpdecision_blocked = 1;
	// Tasks may be pegged to any subset of cores.
	// Find this subset
	// FIXME: For now, we assume that it is only 1 CPU and its hard-coded to CPU 0
	if(!cpu_online(mpdecision_coexist_cpu)) {
		cpu_up(mpdecision_coexist_cpu);
		// Hotplug driver will not enable the flag when mpdecision is blocked
	} else {
		// We disable this cpu state so that binary mode will not change the frequency limits
		// and thereby give us complete control over this core
		phone_state->cpu_states[mpdecision_coexist_cpu]->enabled = 0;
	}

	// We set this core to a frequency that we know will lead to cooling
	// FIXME: Currently, this is hardcoded to 960000. We may need this to be adjustable
	// We know that this frequency will only work towards cooling the system
	policy = cpufreq_cpu_get(mpdecision_coexist_cpu);
	policy->max = 960000;
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
	netlink_send("1");
#else
	sysfs_notify(&tempfreq_kobj, NULL, "mpdecision_coexist_upcall");
#endif
out:
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
}

inline void stop_bg_core_control(void)
{
	int cpu;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif
	// cpu_hotplug_driver is already locked
	if(!initialized || !phonelab_tempfreq_mpdecision_blocked) {
		goto out;
	}
	phonelab_tempfreq_mpdecision_blocked = 0;
	for_each_possible_cpu(cpu) {
		phone_state->cpu_states[cpu]->enabled = 1;
	}

	trace_tempfreq_mpdecision_blocked(0);
	phone_state->cpu_states[mpdecision_coexist_cpu]->enabled = 1;
	// We don't need to set policy->max necessarily.
	// This will happen automatically once binary mode starts to run
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
	netlink_send("0");
#else
	sysfs_notify(&tempfreq_kobj, NULL, "mpdecision_coexist_upcall");
#endif
out:
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
}



#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
static int userspace_pid = -1;
static void netlink_recv(struct sk_buff *skb)
{
	struct nlmsghdr *netlink_header = NULL;
	if(skb == NULL) {
		return;
	}

	netlink_header = (struct nlmsghdr *) skb->data;
	userspace_pid = netlink_header->nlmsg_pid;

	printk(KERN_DEBUG "tempfreq: %s: payload=%s\n", __func__, (char *)NLMSG_DATA(netlink_header));
	// TODO: Handle the message from userspace
}

static void netlink_send(char *msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *netlink_header;
	int len = strlen(msg);
	int ret;

	if(userspace_pid == -1) {
		printk(KERN_ERR "tempfreq: %s: No userspace program registered yet\n", __func__);
		return;
	}

	skb = nlmsg_new(len, 0);
	if(!skb) {
		printk(KERN_ERR "tempfreq: %s: Failed to allocate skb\n", __func__);
			return;
	}

	netlink_header = nlmsg_put(skb, 0, 0, NLMSG_DONE, len, 0);
	NETLINK_CB(skb).dst_group = 0;
	strncpy(nlmsg_data(netlink_header), msg, len);

	ret = nlmsg_unicast(netlink_sk, skb, userspace_pid);
	if(ret < 0) {
		printk(KERN_ERR "tempfreq: %s: Failed to send message to userspace\n", __func__);
	}
}
#endif















/* sysfs hooks */
static ssize_t store_mpdecision_coexist_enable(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;
	switch(val) {
	case 0:
		phonelab_tempfreq_mpdecision_coexist_enable = 0;
		stop_bg_core_control();
		break;
	case 1:
		phonelab_tempfreq_mpdecision_coexist_enable = 1;
		break;
	default:
		err = -EINVAL;
		break;
	}
out:
	kfree(buf);
	return err != 0 ? err : count;
}

__show1(mpdecision_coexist_upcall, mpdecision_blocked);

//tempfreq_attr_rw(mpdecision_coexist_enable);
__show(mpdecision_coexist_enable);
struct tempfreq_attr mpdecision_coexist_enable =
__ATTR(_name, 0644, show_mpdecision_coexist_enable, store_mpdecision_coexist_enable);
struct tempfreq_attr mpdecision_coexist_upcall =
__ATTR(mpdecision_coexist_upcall, 0444, show_mpdecision_coexist_upcall, NULL);


int __init init_mpdecision_coexist(void)
{
	int ret = 0;
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
	netlink_sk = netlink_kernel_create(&init_net, NETLINK_MPDECISION_COEXIST, 0, netlink_recv, NULL, THIS_MODULE);
	if(!netlink_sk) {
		printk(KERN_ERR "tempfreq: %s: Failed to create netlink socket\n", __func__);
		ret = -1;
	} else {
		printk(KERN_INFO "tempfreq: %s: Registered netlink socket\n", __func__);
	}
#endif
	initialized = 1;
	return ret;
}
