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
#include "netlink.h"

#ifdef CONFIG_PHONELAB_TEMPFREQ_NETLINK
struct sock *netlink_sk = NULL;
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_NETLINK
static int userspace_pid = -1;

void netlink_recv(struct sk_buff *skb)
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

	/*
	printk(KERN_DEBUG "tempfreq: %s: seq=%d pid=%d len=%d real_len=%d payload=%s\n"
			, __func__,
			nlh->nlmsg_seq, nlh->nlmsg_pid, nlh->nlmsg_len,
			real_len, payload);
	*/
	// TODO: Handle the message from userspace
	if(strcmp(payload, "hello") == 0) {
		userspace_pid = nlh->nlmsg_pid;
		printk(KERN_DEBUG "tempfreq: %s: Updated pid to %d\n", __func__, userspace_pid);
	} else if(strcmp(payload, "0") == 0) {
		// Userspace finished handling stop_bg_core_control()
		//printk(KERN_DEBUG "tempfreq: %s: Userspace finished handling 0\n", __func__);
	} else if(strcmp(payload, "1") == 0) {
		// Userspace finished handling start_bg_core_control()
		//printk(KERN_DEBUG "tempfreq: %s: Userspace finished handling 1\n", __func__);
#ifdef CONFIG_PHONELAB_TEMPFREQ_CGROUP_CPUSET_BIND
		//schedule_work(&bind_copy_work);
#endif
	} else {
		printk(KERN_DEBUG "tempfreq: %s: Unknown payload: '%s'\n", __func__, payload);
	}
	kfree(payload);
}

void netlink_send(struct netlink_cmd *cmd)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int extra_hdr_len = sizeof(int);
	int real_msg_len = sizeof(struct netlink_cmd);
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
	memcpy(NLMSG_DATA(nlh) + extra_hdr_len, cmd, sizeof(struct netlink_cmd));
	NETLINK_CB(skb).pid = 0;
	NETLINK_CB(skb).dst_group = 0;

	ret = netlink_unicast(netlink_sk, skb, userspace_pid, 0);
	if(ret < 0) {
		printk(KERN_ERR "tempfreq: %s: Failed to broadcast message to userspace\n", __func__);
	} else {
		//printk(KERN_DEBUG "tempfreq: %s: Successfully sent len=%d '%s'\n", __func__, len, msg);
	}
}
#endif
