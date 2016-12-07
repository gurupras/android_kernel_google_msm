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
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
#include "netlink.h"
#endif

static int initialized = 0;
int phonelab_tempfreq_mpdecision_coexist_enable = 1;
int phonelab_tempfreq_mpdecision_blocked = 0;
struct cpumask phonelab_tempfreq_mpdecision_coexist_cpu;
int phonelab_tempfreq_mpdecision_block_offline = 0;

static DEFINE_MUTEX(mpdecision_coexist_mutex);

static void mpdecision_netlink_send(char *val);

void start_bg_core_control(void);
void stop_bg_core_control(void);

static void mpdecision_coexist_lock(void)
{
	mutex_lock(&mpdecision_coexist_mutex);
}
static void mpdecision_coexist_unlock(void)
{
	mutex_unlock(&mpdecision_coexist_mutex);
}


inline __cpuinit void start_bg_core_control(void)
{
	struct cpufreq_policy *policy;
	int cpu;
	int ret;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif
	mutex_lock(&mpdecision_coexist_mutex);

	// cpu_hotplug_driver is already locked
	if(!initialized || phonelab_tempfreq_mpdecision_blocked) {
		goto out;
	}

	phonelab_tempfreq_mpdecision_blocked = 1;
	// Tasks may be pegged to any subset of cores.
	// Find this subset
	// FIXME: For now, we assume that it is only 1 CPU and its hard-coded to CPU 0
	for_each_cpu(cpu, &phonelab_tempfreq_mpdecision_coexist_cpu) {
		if(!cpu_online(cpu)) {
			printk(KERN_DEBUG "tempfreq: %s: Attempting to bring cpu-%d up\n", __func__, cpu);
			ret = cpu_up(cpu);
			if(ret) {
				printk(KERN_ERR "tempfreq: %s: Failed to bring cpu-%d online\n", __func__, cpu);
				phonelab_tempfreq_mpdecision_blocked = 0;
				goto out;
			}
			// Hotplug driver will not enable the flag when mpdecision is blocked
		} else {
			// We disable this cpu state so that binary mode will not change the frequency limits
			// and thereby give us complete control over this core
			update_phone_state(cpu, 0);
		}

		// We set this core to a frequency that we know will lead to cooling
		// FIXME: Currently, this is hardcoded to 960000. We may need this to be adjustable
		// We know that this frequency will only work towards cooling the system
		policy = cpufreq_cpu_get(cpu);
		if(policy == NULL) {
			// Try again after some time
			phonelab_tempfreq_mpdecision_blocked = 0;
			goto out;
		}
		lock_policy_rwsem_write(policy->cpu);
		policy->max = 960000;
		unlock_policy_rwsem_write(policy->cpu);
		cpufreq_cpu_put(policy);
	}
	trace_tempfreq_mpdecision_blocked(1);
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
	mpdecision_netlink_send("1");
#else
	sysfs_notify(&tempfreq_kobj, NULL, "mpdecision_coexist_upcall");
#endif
out:
	mpdecision_coexist_unlock();
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
}

// Expects phone state to be locked
static void update_bg_core_control(const struct cpumask *mask)
{
	int cpu;
	struct cpufreq_policy *policy;

	if(cpumask_equal(mask, &phonelab_tempfreq_mpdecision_coexist_cpu)) {
		return;
	}

	for_each_cpu(cpu, &phonelab_tempfreq_mpdecision_coexist_cpu) {
		policy = cpufreq_cpu_get(cpu);
		if(policy == NULL) {
			// Try again after some time
			continue;
		}
		lock_policy_rwsem_write(policy->cpu);
		policy->max = 2265600;
		unlock_policy_rwsem_write(policy->cpu);
		update_phone_state(cpu, 1);
		cpufreq_cpu_put(policy);
	}

	for_each_cpu(cpu, mask) {
		policy = cpufreq_cpu_get(cpu);
		if(policy == NULL) {
			// Try again after some time
			continue;
		}
		lock_policy_rwsem_write(policy->cpu);
		policy->max = 960000;
		unlock_policy_rwsem_write(policy->cpu);
		cpufreq_cpu_put(policy);
	}
}
// Expects phone_state_lock to be held
inline void stop_bg_core_control(void)
{
#ifdef DEBUG
	u64 ns = sched_clock();
#endif
	struct cpufreq_policy *policy;
	struct cftype cft;
	int cpu;

	mpdecision_coexist_lock();
	// cpu_hotplug_driver is already locked
	if(!initialized || !phonelab_tempfreq_mpdecision_blocked) {
		goto out;
	}

	phone_state_lock();
	for_each_cpu(cpu, &phonelab_tempfreq_mpdecision_coexist_cpu) {
		policy = cpufreq_cpu_get(cpu);
		if(policy == NULL) {
			// Try again after some time
			phonelab_tempfreq_mpdecision_blocked = 0;
			goto out;
		}
		lock_policy_rwsem_write(policy->cpu);
		policy->max = 2265600;
		unlock_policy_rwsem_write(policy->cpu);
		update_phone_state(cpu, 1);
		cpufreq_cpu_put(policy);
	}
	phone_state_unlock();

	cft.private = 1;
	cpuset_write_resmask(cs_fg_bg, &cft, "0-3");

	phonelab_tempfreq_mpdecision_blocked = 0;
	trace_tempfreq_mpdecision_blocked(0);
	// We don't need to set policy->max necessarily.
	// This will happen automatically once binary mode starts to run
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
	// We wait for userspace to inform us that it has set up everything else
	// Once that's done, the netlink recv() call will update state
	mpdecision_netlink_send("0");
#else
	sysfs_notify(&tempfreq_kobj, NULL, "mpdecision_coexist_upcall");
#endif
out:
	mpdecision_coexist_unlock();
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
}

void __cpuinit handle_bg_update(u64 last_bg_busy, u64 avg_fg_busy)
{
	struct cpufreq_policy *policy;
	int new_freq;
	int cpu;
	char reason[32];
	char buf[8];
	int offset = 0;
	struct cftype cft;
	struct cpumask mask;

	if(phonelab_tempfreq_mpdecision_coexist_enable) {
		printk(KERN_DEBUG "tempfreq: mpdecision - bg_busy: %llu  fg_busy: %llu\n", last_bg_busy, avg_fg_busy);

	       if(avg_fg_busy > 40) {
			// Split them up
			int _cpu;

			// Update bg cpu mask to be only core-0
			phone_state_lock();
			update_bg_core_control(cpumask_of(0));
			phone_state_unlock();
			// Update the cpuset to have only core 0
			memset(buf, 0, sizeof(buf));
			buf[0] = '0';
			cft.private = 1;
			cpuset_write_resmask(cs_bg_non_interactive, &cft, buf);
			sprintf(reason, "fg_busy(%llu) > 40", avg_fg_busy);
			trace_tempfreq_cpuset_change(0, buf, reason);



			if(!phonelab_tempfreq_mpdecision_blocked && is_netlink_ready()) {
				start_bg_core_control();
				// There is contention between foreground and background
				// Change cpuset.cpus affinity of cs_fg_bg to ignore bg core
				memset(buf, 0, 8);
				get_online_cpus();
				// Now update the mask
				// mask = available - 0
				memset(buf, 0, 8);
				get_online_cpus();
				for_each_online_cpu(_cpu) {
					if(_cpu == 0) {
						continue;
					}
					if(_cpu < 3) {
						offset += sprintf(buf + offset, "%d,", _cpu);
					} else {
						offset += sprintf(buf + offset, "%d", _cpu);
					}
				}
				cft.private = 1;
				//printk(KERN_DEBUG "tempfreq: %s: Updating cs_fg_bg cpuset mask to: %s\n", __func__, buf);
				cpuset_write_resmask(cs_fg_bg, &cft, buf);
				cpulist_parse(buf, &mask);
				cpumask_scnprintf(buf, 8, &mask);
				trace_tempfreq_cpuset_change(0, buf, reason);
				put_online_cpus();
			}
		}

		if(last_bg_busy > 30) {
			if(avg_fg_busy < 10) {
				int _cpu;

				memset(buf, 0, sizeof(buf));
				// We have high BG activity, but no FG activity..
				// Use more cores for BG
				phone_state_lock();
				update_bg_core_control(cpu_active_mask);
				for_each_online_cpu(_cpu) {
					if(_cpu < 3) {
						offset += sprintf(buf + offset, "%d,", _cpu);
					} else {
						offset += sprintf(buf + offset, "%d", _cpu);
					}
				}
				cft.private = 1;
				cpuset_write_resmask(cs_bg_non_interactive, &cft, buf);
				cpulist_parse(buf, &mask);
				cpumask_scnprintf(buf, 8, &mask);
				sprintf(reason, "bg_busy(%llu) > 50 && fg_busy(%llu) < 10", last_bg_busy, avg_fg_busy);
				trace_tempfreq_cpuset_change(1, buf, reason);
				phone_state_unlock();
			}
			new_freq = avg_fg_busy > 30 ? 960000 : 1574400;
			for_each_cpu(cpu, &phonelab_tempfreq_mpdecision_coexist_cpu) {
				policy = cpufreq_cpu_get(cpu);
				if(policy == NULL) {
					// Try again after some time
					return;
				}
				//printk(KERN_DEBUG "tempfreq: mpdecision - bg freq: %d (%llu%%)\n", new_freq, avg_fg_busy);
				if(policy->max != new_freq) {
					lock_policy_rwsem_write(policy->cpu);
					policy->max = new_freq;
					unlock_policy_rwsem_write(policy->cpu);
				}
				cpufreq_cpu_put(policy);
			}
		} else {
			// BG < 10
			// do nothing here..wait for temperature reading and then decide
		}
	}
}


static void mpdecision_netlink_send(char *val)
{
	// We need to create a netlink_cmd and send it up
	struct netlink_cmd cmd;
	char *_cmd = "mpdecision";

	cmd.cmd_len = strlen(_cmd);
	strncpy(cmd.cmd, "mpdecision", 16);

	cmd.args_len = strlen(val);
	strncpy(cmd.args, val, 24);
	netlink_send(&cmd);
}

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
		mpdecision_netlink_send("0");
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

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
static int phonelab_tempfreq_mpdecision_coexist_nl_send = -1;
static ssize_t store_mpdecision_coexist_nl_send(const char *_buf, size_t count)
{
	char buf[32];

	memset(buf, 0, sizeof buf);
	strncpy(buf, _buf, count);
	strcpy(buf, strstrip(buf));

	mpdecision_netlink_send(buf);
	return count;
}
#endif	/* CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK */


static ssize_t show_mpdecision_bg_cpu(char *buf)
{
	int cpu;
	int offset = 0;
	int idx = 0;
	memset(buf, 0, 16);

	phone_state_lock();
	for_each_cpu(cpu, &phonelab_tempfreq_mpdecision_coexist_cpu) {
		if(idx == 0) {
			offset += sprintf(buf + offset, "%d", cpu);
		} else {
			offset += sprintf(buf + offset, ",%d", cpu);
		}
		idx++;
	}
	phone_state_unlock();
	return strlen(buf);
}

static ssize_t store_mpdecision_bg_cpu(const char *_buf, size_t count)
{
	int err;
	int cpu;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	char cpumask_buf[8];
	struct cpumask trial_cpumask;
	err = cpulist_parse(_buf, &trial_cpumask);
	if(err < 0) {
		err = -EINVAL;
		goto out;
	}
	if (!cpumask_subset(&trial_cpumask, cpu_active_mask)) {
		err = -EINVAL;
		goto out;
	}

	phone_state_lock();
	if(phonelab_tempfreq_mpdecision_blocked) {
		for_each_cpu(cpu, &phonelab_tempfreq_mpdecision_coexist_cpu) {
			update_phone_state(cpu, 1);
		}
	}
	phonelab_tempfreq_mpdecision_coexist_cpu = trial_cpumask;
	cpumask_scnprintf(cpumask_buf, 8, &trial_cpumask);
	printk(KERN_DEBUG "tempfreq: Updated mpdecision CPU to: %s\n", cpumask_buf);
	if(phonelab_tempfreq_mpdecision_blocked) {
		for_each_cpu(cpu, &trial_cpumask) {
			update_phone_state(cpu, 0);
		}
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


static ssize_t store_mpdecision_block_offline(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val < 0) {
		err = -EINVAL;
		goto out;
	}

	phonelab_tempfreq_mpdecision_block_offline = val == 0 ? 0 : 1;
out:
	kfree(buf);
	return err != 0 ? err : count;
}

__show1(mpdecision_coexist_upcall, mpdecision_blocked);


//tempfreq_attr_rw(mpdecision_coexist_enable);
__show(mpdecision_coexist_enable);
__show(mpdecision_block_offline);

struct tempfreq_attr mpdecision_coexist_enable =
__ATTR(mpdecision_coexist_enable, 0644, show_mpdecision_coexist_enable, store_mpdecision_coexist_enable);
struct tempfreq_attr mpdecision_coexist_upcall =
__ATTR(mpdecision_coexist_upcall, 0644, show_mpdecision_coexist_upcall, store_mpdecision_coexist_upcall);
struct tempfreq_attr mpdecision_bg_cpu =
__ATTR(mpdecision_bg_cpu, 0644, show_mpdecision_bg_cpu, store_mpdecision_bg_cpu);
struct tempfreq_attr mpdecision_block_offline =
__ATTR(mpdecision_block_offline, 0644, show_mpdecision_block_offline, store_mpdecision_block_offline);



#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK
export_tempfreq_attr_rw(mpdecision_coexist_nl_send);
#endif	/* CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST_NETLINK */


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
