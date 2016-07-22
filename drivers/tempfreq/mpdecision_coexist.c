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
int phonelab_tempfreq_mpdecision_coexist_cpu = 0;

void start_bg_core_control(void);
void stop_bg_core_control(void);

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
#define NETLINK_MPDECISION_COEXIST NETLINK_USERSOCK
static struct sock *netlink_sk = NULL;
static void netlink_send(char *msg);
static void netlink_recv(struct sk_buff *skb);
#endif

// Expects phone_state_lock to be held
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

	phonelab_tempfreq_mpdecision_blocked = 1;
	// Tasks may be pegged to any subset of cores.
	// Find this subset
	// FIXME: For now, we assume that it is only 1 CPU and its hard-coded to CPU 0
	if(!cpu_online(phonelab_tempfreq_mpdecision_coexist_cpu)) {
		cpu_up(phonelab_tempfreq_mpdecision_coexist_cpu);
		// Hotplug driver will not enable the flag when mpdecision is blocked
	} else {
		// We disable this cpu state so that binary mode will not change the frequency limits
		// and thereby give us complete control over this core
		update_phone_state(phonelab_tempfreq_mpdecision_coexist_cpu, 0);
	}

	// We set this core to a frequency that we know will lead to cooling
	// FIXME: Currently, this is hardcoded to 960000. We may need this to be adjustable
	// We know that this frequency will only work towards cooling the system
	policy = cpufreq_cpu_get(phonelab_tempfreq_mpdecision_coexist_cpu);
	if(policy == NULL) {
		// Try again after some time
		phonelab_tempfreq_mpdecision_blocked = 0;
		goto out;
	}
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

// Expects phone_state_lock to be held
inline void stop_bg_core_control(void)
{
#ifdef DEBUG
	u64 ns = sched_clock();
#endif
	// cpu_hotplug_driver is already locked
	if(!initialized || !phonelab_tempfreq_mpdecision_blocked) {
		goto out;
	}
	update_phone_state(phonelab_tempfreq_mpdecision_coexist_cpu, 1);
	phonelab_tempfreq_mpdecision_blocked = 0;
	// We don't need to set policy->max necessarily.
	// This will happen automatically once binary mode starts to run
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
	// We wait for userspace to inform us that it has set up everything else
	// Once that's done, the netlink recv() call will update state
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
	struct nlmsghdr *nlh = NULL;
	char *payload;
	int len;
	u32 real_len;
	void *data_ptr;
	char magic;

	if(skb == NULL) {
		return;
	}

	nlh = (struct nlmsghdr *) skb->data;
	data_ptr = NLMSG_DATA(nlh);

	magic = *(char *)data_ptr;
	if(magic != '@') {
		printk(KERN_ERR "tempfreq: %s: Magic was not '@' ('%c')\n", __func__, magic);
		return;
	}
	data_ptr++;

	len = nlh->nlmsg_len;

	real_len = *(u32 *) data_ptr;
	data_ptr += sizeof(u32);

	payload = kzalloc(real_len, GFP_KERNEL);
	strncpy(payload, data_ptr, real_len);

	printk(KERN_DEBUG "tempfreq: %s: seq=%d pid=%d len=%d real_len=%d payload=%s\n"
			, __func__,
			nlh->nlmsg_seq, nlh->nlmsg_pid, nlh->nlmsg_len,
			real_len, payload);
	// TODO: Handle the message from userspace
	if(strcmp(payload, "hello") == 0) {
		userspace_pid = nlh->nlmsg_pid;
		printk(KERN_DEBUG "tempfreq: %s: Updated pid to %d\n", __func__, userspace_pid);
	} else if(strcmp(payload, "0") == 0) {
		// Userspace finished handling stop_bg_core_control()
		printk(KERN_DEBUG "tempfreq: %s: Userspace finished handling 0\n", __func__);
		trace_tempfreq_mpdecision_blocked(0);
	} else if(strcmp(payload, "1") == 0) {
		// Userspace finished handling start_bg_core_control()
		printk(KERN_DEBUG "tempfreq: %s: Userspace finished handling 1\n", __func__);
#ifdef CONFIG_PHONELAB_TEMPFREQ_CGROUP_CPUSET_BIND
		//schedule_work(&bind_copy_work);
#endif
		trace_tempfreq_mpdecision_blocked(1);
	} else {
		printk(KERN_DEBUG "tempfreq: %s: Unknown payload: '%s'\n", __func__, payload);
	}
	kfree(payload);
}

static void netlink_send(char *msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int extra_hdr_len = sizeof(int);
	int real_msg_len = strlen(msg);
	int len = extra_hdr_len + NLMSG_SPACE(real_msg_len);
	int skblen = NLMSG_SPACE(len);
	int ret;

	if(netlink_sk == NULL) {
		printk(KERN_ERR "tempfreq: %s: Netlink socket not registered\n", __func__);
		return;
	}
	if(userspace_pid == -1) {
		printk(KERN_ERR "tempfreq: %s: No userspace program registered yet\n", __func__);
		return;
	}

	skb = alloc_skb(skblen, GFP_KERNEL);
	if(!skb) {
		printk(KERN_ERR "tempfreq: %s: Failed to allocate skb\n", __func__);
			return;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, len - sizeof(*nlh), 0);
	/*
	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_len = len;
	nlh->nlmsg_pid = userspace_pid;
	nlh->nlmsg_flags = 0;
	*/
	memset(NLMSG_DATA(nlh), 0, len);
	memcpy(NLMSG_DATA(nlh), &real_msg_len, extra_hdr_len);
	strncpy(NLMSG_DATA(nlh) + extra_hdr_len, msg, strlen(msg));
	NETLINK_CB(skb).pid = 0;
	NETLINK_CB(skb).dst_group = 0;

	ret = netlink_unicast(netlink_sk, skb, userspace_pid, 0);
	if(ret < 0) {
		printk(KERN_ERR "tempfreq: %s: Failed to broadcast message to userspace\n", __func__);
	} else {
		printk(KERN_DEBUG "tempfreq: %s: Successfully sent len=%d '%s'\n", __func__, len, msg);
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

static int phonelab_tempfreq_mpdecision_coexist_nl_send = -1;
static ssize_t store_mpdecision_coexist_nl_send(const char *_buf, size_t count)
{
	char buf[32];

	memset(buf, 0, sizeof buf);
	strncpy(buf, _buf, count);
	strcpy(buf, strstrip(buf));

	netlink_send(buf);
	return count;
}


static ssize_t store_mpdecision_bg_cpu(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;
	if(val < 0 || val > 3) {
		err = -EINVAL;
	}

	phone_state_lock();
	if(phonelab_tempfreq_mpdecision_blocked) {
		update_phone_state(phonelab_tempfreq_mpdecision_coexist_cpu, 1);
	}
	phonelab_tempfreq_mpdecision_coexist_cpu = val;
	if(phonelab_tempfreq_mpdecision_blocked) {
		update_phone_state(phonelab_tempfreq_mpdecision_coexist_cpu, 0);
	}
	phone_state_unlock();
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t __ref store_mpdecision_coexist_upcall(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;
	switch(val) {
	case 0:
		phone_state_lock();
		stop_bg_core_control();
		phone_state_unlock();
		break;
	case 1:
		phone_state_lock();
		start_bg_core_control();
		phone_state_unlock();
		break;
	default:
		err = -EINVAL;
	}
out:
	kfree(buf);
	return err != 0 ? err : count;
}

__show1(mpdecision_coexist_upcall, mpdecision_blocked);
__show1(mpdecision_bg_cpu, mpdecision_coexist_cpu);

//tempfreq_attr_rw(mpdecision_coexist_enable);
__show(mpdecision_coexist_enable);
struct tempfreq_attr mpdecision_coexist_enable =
__ATTR(mpdecision_coexist_enable, 0644, show_mpdecision_coexist_enable, store_mpdecision_coexist_enable);
struct tempfreq_attr mpdecision_coexist_upcall =
__ATTR(mpdecision_coexist_upcall, 0644, show_mpdecision_coexist_upcall, store_mpdecision_coexist_upcall);
struct tempfreq_attr mpdecision_bg_cpu =
__ATTR(mpdecision_bg_cpu, 0644, show_mpdecision_bg_cpu, store_mpdecision_bg_cpu);


export_tempfreq_attr_rw(mpdecision_coexist_nl_send);


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
