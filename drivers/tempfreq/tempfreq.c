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

#define CREATE_TRACE_POINTS
#include <trace/events/tempfreq.h>

#include <linux/phonelab.h>

#ifdef CONFIG_PHONELAB_TEMPFREQ_HOTPLUG_DRIVER
#include "hotplug.h"
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_CGROUP_THROTTLING
#include "tempfreq.h"
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER
#include <linux/rq_stats.h>
#endif

static int phonelab_tempfreq_enable = 1;

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
int phonelab_tempfreq_mpdecision_blocked = 0;
static void start_bg_core_control(void);
static void stop_bg_core_control(void);
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
int phonelab_tempfreq_binary_threshold_temp	= 70;
int phonelab_tempfreq_binary_critical		= 75;
int phonelab_tempfreq_binary_lower_threshold	= 65;
int phonelab_tempfreq_binary_jump_lower		= 2;

int phonelab_tempfreq_binary_short_epochs	= 2;
int phonelab_tempfreq_binary_short_diff_limit	= 3;

int phonelab_tempfreq_binary_long_epochs	= 5;
int phonelab_tempfreq_binary_long_diff_limit	= 2;
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_CGROUP_THROTTLING
//FIXME: This include is a hack
#include <linux/../../kernel/sched/sched.h>

enum {
	CGROUP_STATE_UNKNOWN = -1,
	CGROUP_STATE_NORMAL = 0,
	CGROUP_STATE_THROTTLED,
};
struct cgroup_map cgroup_map;

static void thermal_cgroup_throttling_update_cgroup_entry(struct cgroup_entry *entry, int temp);

// This function is implemented in kern/sched/core.c
// Borrowed since this would require header file change => long compilation
static inline struct task_group *cgroup_tg(struct cgroup *cgrp)
{
	return container_of(cgroup_subsys_state(cgrp, cpu_cgroup_subsys_id),
			    struct task_group, css);
}
#endif

DECLARE_PER_CPU(struct cpufreq_policy *, cpufreq_cpu_data);
DECLARE_PER_CPU(struct cpufreq_frequency_table *, cpufreq_show_table);


enum {
	TEMP_UNKNOWN
};

static int num_frequencies;
static int *FREQUENCIES;

struct phone_state *phone_state = NULL;

enum {
	HIGHEST,
	LOWEST,
	HIGHER,
	LOWER,
};

#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
enum {
	REASON_SHORT_EPOCH_DIFF = 0,
	REASON_LONG_EPOCH_DIFF,
	REASON_CRITICAL,
	REASON_COOL,
	NR_REASONS,
};
static const char *reasons[] = {
	"REASON_SHORT_EPOCH_DIFF",
	"REASON_LONG_EPOCH_DIFF",
	"REASON_CRITICAL",
	"REASON_COOL",
	NULL,
};
#endif

const char *HOTPLUG_STATE_STR[] = {
	"HOTPLUG_UNKNOWN_NEXT",
	"HOTPLUG_INCREASE_NEXT",
	"HOTPLUG_DECREASE_NEXT",
};

static inline int get_cpu_with(int relation);
static inline int get_frequency_index(int frequency);
static inline int get_new_frequency(int cpu, int relation);
static inline int get_next_frequency_index(int idx);

static void cpu_state_string(struct cpu_state *cs, char *str);


void __set_to_string(int set, char buf[10])
{
	int i;
	int offset = 0;
	char tmpbuf[10];
	offset += sprintf(tmpbuf + offset, "[");

	for_each_possible_cpu(i) {
		int is_online = set & (1 << i);
		if(is_online) {
			if(i != 0) {
				offset += sprintf(tmpbuf + offset, ",%d", i);
			} else {
				offset += sprintf(tmpbuf + offset, "%d", i);
			}
		}
	}
	offset += sprintf(tmpbuf + offset, "]");
	//printk(KERN_DEBUG "tempfreq: %s: %s\n", __func__, tmpbuf);
	sprintf(buf, "%s", tmpbuf);
}



static int tempfreq_thermal_callback(struct notifier_block *nfb,
					unsigned long action, void *temp_ptr)
{
	struct cpufreq_policy *policy;
	struct cpu_state *cs;
	long *temp_ptr_long = (long *) temp_ptr;
	long temp = *temp_ptr_long;
#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
	static long prev_short_epoch_temp = TEMP_UNKNOWN;
	static long prev_long_epoch_temp = TEMP_UNKNOWN;
	static int short_epochs_counted = 0, long_epochs_counted = 0;
	long prev_temp = TEMP_UNKNOWN;
	int cpu = -1;
	int freq;
	int binary_freq = 0;
	int is_increase = -1;
	int op;
	int reason_int = -1;
	const char *reason_str;
#endif
	int enabled = 0;
	int i;
	int ret = 0;

#ifdef DEBUG
	u64 ns = sched_clock();
#endif
	if(!phonelab_tempfreq_enable) {
		goto out;
	}

	for(i = 0; i < phone_state->ncpus; i++) {
		enabled |= phone_state->cpu_states[i]->enabled;
	}
	if(!enabled) {
		goto out;
	}

	// Handle
//	printk(KERN_DEBUG "tempfreq: Callback!\n");
	trace_tempfreq_temp(temp);
#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
	short_epochs_counted++;
	long_epochs_counted++;

	if(temp > phonelab_tempfreq_binary_threshold_temp) {
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
		// There is a case here where mpdecision has blocked a cpu just before we enter and block mpdecision
		// Think about whether this needs to be specially handled
		cpu_hotplug_driver_lock();
		phonelab_tempfreq_mpdecision_blocked = 1;
		cpu_hotplug_driver_unlock();
		start_bg_core_control();
#endif
		if(short_epochs_counted == phonelab_tempfreq_binary_short_epochs) {
			// We're above the phonelab_tempfreq_binary_critical and
			// if temperature increased by more than
			// phonelab_tempfreq_binary_diff_limit degrees in phonelab_tempfreq_binary_epochs
			// consecutive readings, we halve
			if(temp - prev_short_epoch_temp > phonelab_tempfreq_binary_short_diff_limit) {
				cpu = get_cpu_with(HIGHEST);
				op = LOWER;
				is_increase = 0;
				prev_temp = prev_short_epoch_temp;
				reason_int = REASON_SHORT_EPOCH_DIFF;
			}
			short_epochs_counted = 0;
			prev_short_epoch_temp = temp;
			goto done;
		}
	}

	// Now check for the UPPER and LOWER thresholds
	if(temp >= phonelab_tempfreq_binary_critical) {
		// This is the upper limit.
		// We need to move frequency down
		cpu = get_cpu_with(HIGHEST);
		op = LOWER;
		is_increase = 0;
		prev_temp = 0;
		reason_int = REASON_CRITICAL;
		goto done;
	}
	else if(temp <= phonelab_tempfreq_binary_lower_threshold) {
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
		cpu_hotplug_driver_lock();
		phonelab_tempfreq_mpdecision_blocked = 0;
		cpu_hotplug_driver_unlock();
#endif
		// We can now increase the limit on the lowest
		cpu = get_cpu_with(LOWEST);
		op = HIGHER;
		is_increase = 1;
		prev_temp = 0;
		reason_int = REASON_COOL;
		goto done;
	}

	// Now check for the stable long epoch
	if(long_epochs_counted >= phonelab_tempfreq_binary_long_epochs) {
		// If we've remained stable for a long period
		// Regardless of frequency
		if(temp - prev_long_epoch_temp < phonelab_tempfreq_binary_long_diff_limit) {
			if(temp < phonelab_tempfreq_binary_threshold_temp) {
				cpu = get_cpu_with(LOWEST);
				op = HIGHER;
				is_increase = 1;
				reason_int = REASON_LONG_EPOCH_DIFF;
				prev_temp = prev_long_epoch_temp;
			}
			else {
				if(temp < phonelab_tempfreq_binary_critical - 2) {	//FIXME: Currently '2' is hard-coded
					int next_idx;
					// This does smaller boosts hoping to squeeze out the best performance we can get
					cpu = get_cpu_with(LOWEST);
					if(cpu == -1) {
						printk(KERN_ERR "tempfreq: %s: Could not get CPU for linear increment\n", __func__);
						return -1;
					}
					op = HIGHER;
					is_increase = 1;
					reason_int = REASON_LONG_EPOCH_DIFF;
					prev_temp = prev_long_epoch_temp;
					next_idx = get_next_frequency_index(phone_state->cpu_states[cpu]->cur_idx);
					binary_freq = FREQUENCIES[next_idx];
				}
			}
		}
		long_epochs_counted = 0;
		prev_long_epoch_temp = temp;
		goto done;
	}

done:
	if(cpu == -1) {
		if(reason_int >= 0) {
			int tmp;
			reason_str = reasons[reason_int];
			printk(KERN_ERR "tempfreq: %s: Have reason but no CPU: cpu=%d op=%s reason=%s\n",
					__func__, cpu, op == HIGHER ? "HIGHER" : "LOWER", reason_str);
			for_each_possible_cpu(tmp) {
				char cs_str[128];
				cs = phone_state->cpu_states[tmp];
				cpu_state_string(cs, cs_str);
				printk(KERN_ERR "tempfreq: %s: %s\n", __func__, cs_str);
			}
		}
		// We have nothing more to do. Just return
//		printk(KERN_WARNING "tempfreq: CPU == -1\n");
		goto out;
	}

	reason_str = reasons[reason_int];
	cs = phone_state->cpu_states[cpu];
	if(!binary_freq) {
		binary_freq = get_new_frequency(cpu, op);
	}

	if(cs->cur_max_idx >= 0 && binary_freq == FREQUENCIES[cs->cur_max_idx]) {
		// We're not making any change
		char cs_str[128];
		cpu_state_string(cs, cs_str);
//		printk(KERN_DEBUG "tempfreq: %s: No change! temp=%ld prev_temp=%ld cpu=%d op=%s reason=%s {%s}\n",
//				__func__, temp, prev_temp, cpu, op == HIGHER ? "HIGHER" : "LOWER", reason_str, cs_str);
	}
	else {
		freq = cs->cur_freq;
		if(freq == FREQUENCIES[cs->cur_max_idx]) {
			// If frequency was currently at the max, then,
			// when we're changing limits, we need to reset the long and short
			short_epochs_counted = 0;
			prev_short_epoch_temp = temp;

			long_epochs_counted = 0;
			prev_long_epoch_temp = temp;
		}
		trace_tempfreq_binary_diff(temp, prev_temp, cpu, freq, is_increase, binary_freq, reason_str);
		// Write new values
		policy = cpufreq_cpu_get(cpu);
		policy->max = binary_freq;
		cpufreq_cpu_put(policy);
		cs->cur_max_idx = get_frequency_index(binary_freq);
	}
#else
	(void) cs;
	(void) policy;
	cpu_state_string(NULL, NULL);
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_CGROUP_THROTTLING
	for(i = 0; i < cgroup_map.cur_idx; i++) {
		struct cgroup_entry *entry = &cgroup_map.entries[i];
		thermal_cgroup_throttling_update_cgroup_entry(entry, temp);
	}
#endif
	(void) ret;
out:
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(tempfreq_thermal_callback);


#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
static void start_bg_core_control(void)
{
}

static void stop_bg_core_control(void)
{
}
#endif


#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_CGROUP_THROTTLING
static int compute_nth_percentile(int base_percentile, int elapsed, int max_timeout)
{
	return min(base_percentile + ((elapsed * 100) / max_timeout), 100);
}

static void thermal_cgroup_throttling_update_cgroup_entry(struct cgroup_entry *entry, int temp)
{
	struct cgroup *cgrp = entry->cgroup;
	u64 shares = entry->cpu_shares;
	int state = entry->state;
	struct task_group *tg;

	int elapsed_time;
	int nth_percentile;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif
	if(temp == 0) {
		// We just got an update from sysfs
		return;
	}

	tg = cgroup_tg(cgrp);
	elapsed_time = div_u64((sched_clock() - entry->throttle_time), 1000000);

	if(entry->state == CGROUP_STATE_THROTTLED) {
		// Check if timeout has exceeded. If it has, we just reset the cgroup
	       if(elapsed_time >= tg->tempfreq_thermal_cgroup_throttling_timeout) {
			shares = entry->cpu_shares;
			entry->cpu_shares = 0;
			entry->throttle_time = 0;
			state = CGROUP_STATE_NORMAL;
			sched_group_set_shares(tg, scale_load(shares));
			trace_tempfreq_thermal_cgroup_throttling(temp, entry->cur_idx, state, 1, 0, elapsed_time);
			entry->state = state;
		} else {
			// Cgroup is throttled but time hasn't elapsed
			// Relax threshold according to ratio of elapsed time to total timeout
			// FIXME: the timeout may be adjusted in between. Check to see if this is accounted for properly
			nth_percentile = compute_nth_percentile(25, elapsed_time, tg->tempfreq_thermal_cgroup_throttling_timeout);
			// Check if the current temperature is below 25th percentile of recently measured temperatures
			if(temp <= get_nth_percentile(short_temp_list, nth_percentile)) {
				shares = entry->cpu_shares;
				entry->cpu_shares = 0;
				entry->throttle_time = 0;
				state = CGROUP_STATE_NORMAL;
				sched_group_set_shares(tg, scale_load(shares));
				trace_tempfreq_thermal_cgroup_throttling(temp, entry->cur_idx, state, 0, nth_percentile, elapsed_time);
				entry->state = state;
			}
		}
	}
	// Check to see if we should throttle the cgroup
	else if(temp >= entry->throttling_temp && (entry->state == CGROUP_STATE_NORMAL || entry->state == CGROUP_STATE_UNKNOWN)) {
		// First store the current CPU shares
		entry->cpu_shares = scale_load_down(tg->shares);
		// Now set shares to 0
		shares = 0;
		state = CGROUP_STATE_THROTTLED;
		sched_group_set_shares(tg, scale_load(shares));
		trace_tempfreq_thermal_cgroup_throttling(temp, entry->cur_idx, state, 0, 0, 0);
		entry->state = state;
		entry->throttle_time = sched_clock();
	}
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
}
#endif


static int tempfreq_cpufreq_callback(struct notifier_block *nfb,
					unsigned long action, void *temp_ptr)
{
	struct cpufreq_freqs *freqs;
	int cpu;
	struct cpu_state *cs;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif

	freqs = (struct cpufreq_freqs *) temp_ptr;
	if(freqs == NULL) {
		printk(KERN_ERR "tempfreq: %s: freqs is NULL?\n", __func__);
	}

	cpu = freqs->cpu;
	cs = phone_state->cpu_states[cpu];

	if(action != CPUFREQ_POSTCHANGE)
		return 0;
	if(freqs->old != 0 && cs->cur_freq != freqs->old) {
		printk(KERN_WARNING "tempfreq: %s: cpu_state->cur_freq (%d) != freqs->old (%d)\n", __func__, cs->cur_freq, freqs->old);
	}
	cs->cur_freq = freqs->new;
	cs->cur_idx = get_frequency_index(cs->cur_freq);

#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(tempfreq_cpufreq_callback);


static int tempfreq_hotplug_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int cpu = (int)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
		//printk(KERN_DEBUG "tempfreq: %s: cpu_states[%d]->enabled = 1\n", __func__, cpu);
		phone_state->cpu_states[cpu]->enabled = 1;
		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		//printk(KERN_DEBUG "tempfreq: %s: cpu_states[%d]->enabled = 0\n", __func__, cpu);
		phone_state->cpu_states[cpu]->enabled = 0;
		break;
	};
	return NOTIFY_OK;
}



int tempfreq_update_cgroup_map(struct cgroup *cgrp, int throttling_temp, int unthrottling_temp)
{
	int i;
	for(i = 0; i < cgroup_map.cur_idx; i++) {
		if(cgroup_map.entries[i].cgroup == cgrp) {
			break;
		}
	}
	cgroup_map.entries[i].cgroup = cgrp;
	cgroup_map.entries[i].throttling_temp = throttling_temp;
	cgroup_map.entries[i].unthrottling_temp = unthrottling_temp;
	//FIXME: This could cause tasks to run when they're not supposed to
	thermal_cgroup_throttling_update_cgroup_entry(&cgroup_map.entries[i], 0);
	if(i == cgroup_map.cur_idx)
		cgroup_map.cur_idx++;
	return 0;
}


static int __get_frequency_index(int frequency, int start, int end);
static inline int get_frequency_index(int frequency)
{
	return __get_frequency_index(frequency, 0, num_frequencies);
}

static int __get_frequency_index(int frequency, int start, int end)
{
	int mid = (start + end) / 2;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif

	if(start  > end) {
		printk(KERN_ERR "tempfreq: %s: Could not find '%d' in FREQUENCIES\n", __func__, frequency);
		return -1;
	}

	if(frequency == FREQUENCIES[mid]) {
		return mid;
	}
	if(frequency < FREQUENCIES[mid]) {
		return __get_frequency_index(frequency, start, mid);
	}
	else {
		return __get_frequency_index(frequency, mid + 1, end);
	}
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
}

static inline int get_new_frequency(int cpu, int relation)
{
	struct cpufreq_policy *policy;
	struct cpu_state *cs;
	int cur_idx, result_idx;
	int tmp;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif

	cs = phone_state->cpu_states[cpu];
	policy = cpufreq_cpu_get(cpu);
	if(policy == NULL) {
		printk(KERN_ERR "tempfreq: %s: policy was NULL\n", __func__);
		return -1;
	}
	cur_idx = max((int) cs->cur_idx, (int) cs->cur_max_idx);
	cpufreq_cpu_put(policy);
	if(cur_idx == -1) {
		printk(KERN_ERR "tempfreq: %s: binary search returned -1 for frequency: %d\n", __func__, policy->cur);
		return -1;
	}

	switch(relation) {
		case HIGHER:
			result_idx = (cur_idx + num_frequencies) / 2;	// FIXME: Change this to find ceiling
			break;
		case LOWER:
			tmp = cur_idx - phonelab_tempfreq_binary_jump_lower;
			result_idx = tmp >= 0 ? tmp : 0;
			break;
	}
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
	return FREQUENCIES[result_idx];
}

static inline int get_cpu_with(int relation)
{
	int cpu;
	struct cpu_state *cs = NULL;

	int culprit_f_idx = relation == HIGHEST ? 0 : num_frequencies;
	int culprit_cpu = -1;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif

	// XXX: Check how all of this interacts with hotplug
	// Eventually, we will be the ones controlling hotplug, so
	// we should be able to maintain this state in the phone state
	for_each_online_cpu(cpu) {
		cs = phone_state->cpu_states[cpu];
		// Skip CPU if it is not enabled
		if(!cs->enabled)
			continue;
		if(relation == HIGHEST) {
			if(cs->cur_max_idx == num_frequencies - 1) {	// Max
				return cpu;
			}
			else if(cs->cur_max_idx > culprit_f_idx) {
				culprit_f_idx = cs->cur_max_idx;
				culprit_cpu = cpu;
			}
		}
		else if(relation == LOWEST) {
			if(cs->cur_max_idx == 0) {	// Min
				return cpu;
			}
			else if(cs->cur_max_idx < culprit_f_idx) {
				culprit_f_idx = cs->cur_max_idx;
				culprit_cpu = cpu;
			}
		}
		else {
			printk(KERN_ERR "tempfreq: %s: Neither HIGHEST/LOWEST: %d\n", __func__, relation);
		}
	}
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
	return culprit_cpu;
}


static inline int get_next_frequency_index(int idx)
{
	if(idx >= num_frequencies - 1) {
		return num_frequencies - 1;
	}
	else {
		return idx + 1;
	}
}


static void cpu_state_string(struct cpu_state *cs, char *str)
{
	if(cs == NULL || str == NULL)
		return;
	sprintf(str, "cpuid=%u cur_freq=%u cur_idx=%u cur_max_idx=%u governor=%s enabled=%u",
		cs->cpuid, cs->cur_freq, cs->cur_idx, cs->cur_max_idx, cs->governor, cs->enabled);
}




#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_CGROUP_THROTTLING
static int __init init_tempfreq_thermal_cgroup_throttling(void)
{
	int i;

	cgroup_map.cur_idx = 0;
	for(i = 0; i < CGROUP_MAP_MAX; i++) {
		cgroup_map.entries[i].cur_idx = i;
		cgroup_map.entries[i].cgroup = NULL;
		cgroup_map.entries[i].throttling_temp = 0;
		cgroup_map.entries[i].unthrottling_temp = 0;
		cgroup_map.entries[i].cpu_shares = 0;
		cgroup_map.entries[i].throttle_time = 0;
		cgroup_map.entries[i].state = CGROUP_STATE_UNKNOWN;
	}
	return 0;
}
#endif


#ifdef CONFIG_PHONELAB_TEMPFREQ_HOTPLUG_DRIVER
int phonelab_tempfreq_hotplug_epoch_ms = 100;

static DEFINE_MUTEX(hotplug_driver_mutex);
struct hotplug_driver *hotplug_driver = NULL;
struct delayed_work hotplug_work;
struct hotplug_drivers_list hotplug_drivers_list;

static struct hotplug_driver null_hotplug_driver = {
	.id = PHONELAB_TEMPFREQ_NO_HOTPLUG_DRIVER,
	.name = "null",
	.hotplug_work_fn = NULL
};

static void __cpuinit hotplug_driver_fn(struct work_struct *w)
{
	mutex_lock(&hotplug_driver_mutex);
	if(hotplug_driver->hotplug_work_fn)
		hotplug_driver->hotplug_work_fn(w);
	mutex_unlock(&hotplug_driver_mutex);
	schedule_delayed_work_on(0, &hotplug_work, msecs_to_jiffies(phonelab_tempfreq_hotplug_epoch_ms));
}

#endif


#ifdef CONFIG_PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER
int phonelab_tempfreq_hotplug_epochs_up = 2;
int phonelab_tempfreq_hotplug_epochs_down = 5;

static void __cpuinit task_hotplug_driver_fn(struct work_struct *w);


static struct hotplug_driver task_hotplug_driver = {
	.id = PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER,
	.name = "task_hotplug",
	.hotplug_work_fn = task_hotplug_driver_fn
};

static struct hotplug_state hotplug_state = {
	0, HOTPLUG_UNKNOWN_NEXT
};

// Borrowed from block/drbd/drbd_int.h
#define div_ceil(A, B) ((A)/(B) + ((A)%(B) ? 1 : 0))

/* The following functions are wrapper functions created so that
 * we can avoid code duplication inside task_hotplug_driver_fn
 */
static inline int is_cpu_online(unsigned int cpu) {
	return cpu_online(cpu);
}

static inline int is_cpu_offline(unsigned int cpu) {
	return !cpu_online(cpu);
}

static void __cpuinit task_hotplug_driver_fn(struct work_struct *w)
{
	int cpu;
	int ncpus = num_possible_cpus() - 1;
	int rq_len = nr_running();
	// We try and use the same logic as msm_rq_stats
	int target_ncpus = rq_len > num_possible_cpus() ? num_possible_cpus() : rq_len;

	int online_ncpus = num_online_cpus();
	int change = abs(target_ncpus - online_ncpus);

	int up_set = 0, down_set = 0, overall = 0;

	int (*cpu_fn) (unsigned int) = NULL;
	int *set_ptr = NULL;
	int (*check_fn) (unsigned int) = NULL;
	int expected_next_hotplug_state = HOTPLUG_UNKNOWN_NEXT;
	int required_num_elapsed_epochs = 0;

#ifdef DEBUG
	u64 ns = sched_clock();
#endif

	//printk(KERN_DEBUG "tempfreq: %s: Running\n", __func__);

	trace_tempfreq_hotplug_nr_running(rq_len);
	trace_tempfreq_hotplug_target(online_ncpus, target_ncpus);

	hotplug_state.elapsed_epochs++;

	if(online_ncpus == target_ncpus) {
		if(online_ncpus == 4) {
			hotplug_state.next_state = rq_len >= online_ncpus ? HOTPLUG_INCREASE_NEXT : HOTPLUG_DECREASE_NEXT;
		}
		goto out;
	} else if(online_ncpus < target_ncpus) {
		cpu_fn = cpu_up;
		set_ptr = &up_set;
		check_fn = is_cpu_offline;
		expected_next_hotplug_state = HOTPLUG_INCREASE_NEXT;
		required_num_elapsed_epochs = phonelab_tempfreq_hotplug_epochs_up;
	} else {
		cpu_fn = cpu_down;
		set_ptr = &down_set;
		check_fn = is_cpu_online;
		expected_next_hotplug_state = HOTPLUG_DECREASE_NEXT;
		required_num_elapsed_epochs = phonelab_tempfreq_hotplug_epochs_down;
	}
	trace_tempfreq_hotplug_state(hotplug_state.elapsed_epochs, hotplug_state.next_state, expected_next_hotplug_state);

	// Now check if everything matches and perform the operation
	if(hotplug_state.next_state != expected_next_hotplug_state) {
		hotplug_state.next_state = expected_next_hotplug_state;
		hotplug_state.elapsed_epochs = 0;
	}
	else {
		if(hotplug_state.elapsed_epochs >= required_num_elapsed_epochs) {
			// Alter number of cores
			for(cpu = ncpus; cpu > 0 && change > 0; cpu--) {
				if((*check_fn)(cpu)) {
					(*cpu_fn)(cpu);
					change--;
					*(set_ptr) = (*set_ptr) | (1 << cpu);
				}
			}
			hotplug_state.elapsed_epochs = 0;
		}
	}
out:
	for_each_possible_cpu(cpu) {
		if(cpu_online(cpu)) {
			overall |= (1 << cpu);
		}
	}
	trace_tempfreq_hotplug(up_set, down_set, overall);

#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
	//printk(KERN_DEBUG "tempfreq: %s: Rescheduling ...\n", __func__);
}

static int __init init_tempfreq_hotplug(void)
{
	struct hotplug_drivers_list *lst;
#ifdef CONFIG_PHONELAB_TEMPFREQ_AUTOSMP_HOTPLUG_DRIVER
	extern struct hotplug_driver autosmp_hotplug_driver;
#endif
	INIT_LIST_HEAD(&hotplug_drivers_list.list);

#ifdef CONFIG_PHONELAB_TEMPFREQ_HOTPLUG_DRIVER
	lst = kmalloc(sizeof(struct hotplug_drivers_list), GFP_KERNEL);
	lst->driver = &null_hotplug_driver;
	list_add(&lst->list, &hotplug_drivers_list.list);
#ifdef CONFIG_PHONELAB_TEMPFREQ_DEFAULT_NULL_HOTPLUG_DRIVER
	hotplug_driver = &null_hotplug_driver;
#endif
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER
	lst = kmalloc(sizeof(struct hotplug_drivers_list), GFP_KERNEL);
	lst->driver = &task_hotplug_driver;
	list_add(&lst->list, &hotplug_drivers_list.list);
#ifdef CONFIG_PHONELAB_TEMPFREQ_DEFAULT_TASK_HOTPLUG_DRIVER
	hotplug_driver = &task_hotplug_driver;
#endif
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_AUTOSMP_HOTPLUG_DRIVER
	lst = kmalloc(sizeof(struct hotplug_drivers_list), GFP_KERNEL);
	lst->driver = &autosmp_hotplug_driver;
	list_add(&lst->list, &hotplug_drivers_list.list);
#ifdef CONFIG_PHONELAB_TEMPFREQ_DEFAULT_AUTOSMP_HOTPLUG_DRIVER
	hotplug_driver = &autosmp_hotplug_driver;
#endif
#endif
	INIT_DELAYED_WORK(&hotplug_work, hotplug_driver_fn);
	schedule_delayed_work(&hotplug_work, 0);
	return 0;
}
late_initcall(init_tempfreq_hotplug);
#endif






/* sysfs hooks */
static ssize_t store_enable(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;
	switch(val) {
	case 0:
		phonelab_tempfreq_enable = 0;
		break;
	case 1:
		phonelab_tempfreq_enable = 1;
		break;
	default:
		err = -EINVAL;
		break;
	}
out:
	kfree(buf);
	return err != 0 ? err : count;
}

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
static ssize_t store_mpdecision_blocked(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;
	switch(val) {
	case 0:
		phonelab_tempfreq_mpdecision_blocked = 0;
		break;
	case 1:
		phonelab_tempfreq_mpdecision_blocked = 1;
		break;
	default:
		err = -EINVAL;
		break;
	}
out:
	kfree(buf);
	return err != 0 ? err : count;
}
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
static ssize_t store_binary_threshold_temp(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

#ifdef CONFIG_PHONELAB_TEMPFREQ_DISABLE_KERNEL_LIMITS
	if(val >= 90) {
		err = -EINVAL;
		goto out;
	}
#else
	if(val >= 80) {
		err = -EINVAL;
		goto out;
	}
#endif
	phonelab_tempfreq_binary_threshold_temp = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t store_binary_critical(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val <= phonelab_tempfreq_binary_threshold_temp) {
		err = -EINVAL;
		goto out;
	}

	phonelab_tempfreq_binary_critical = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t store_binary_lower_threshold(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val >= phonelab_tempfreq_binary_threshold_temp) {
		err = -EINVAL;
		goto out;
	}

	phonelab_tempfreq_binary_lower_threshold = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t store_binary_short_epochs(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val <= 0) {
		err = -EINVAL;
		goto out;
	}
	phonelab_tempfreq_binary_short_epochs = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t store_binary_short_diff_limit(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val <= 0) {
		err = -EINVAL;
		goto out;
	}
	phonelab_tempfreq_binary_short_diff_limit = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}


static ssize_t store_binary_long_epochs(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val <= 0) {
		err = -EINVAL;
		goto out;
	}

	if(val <= phonelab_tempfreq_binary_short_epochs) {
		err = -EINVAL;
		goto out;
	}

	phonelab_tempfreq_binary_long_epochs = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t store_binary_long_diff_limit(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val <= 0) {
		err = -EINVAL;
		goto out;
	}

	phonelab_tempfreq_binary_long_diff_limit = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t store_binary_jump_lower(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val <= 0) {
		err = -EINVAL;
		goto out;
	}

	phonelab_tempfreq_binary_jump_lower = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_HOTPLUG_DRIVER
static ssize_t store_tempfreq_hotplug_driver(const char *_buf, size_t count)
{
	int err = 0;
	char *buf = kstrdup(strstrip((char *)_buf), GFP_KERNEL);
	struct hotplug_driver *driver = NULL;
	struct hotplug_drivers_list *entry;
	list_for_each_entry(entry, &hotplug_drivers_list.list, list) {
		if(strcmp(entry->driver->name, buf) == 0) {
			driver = entry->driver;
			break;
		}
	}
	if(!driver) {
		printk(KERN_DEBUG "tempfreq: %s: Did not find driver for '%s'\n", __func__, buf);
		err = -EINVAL;
		goto out;
	}

	mutex_lock(&hotplug_driver_mutex);
	hotplug_driver = driver;
	mutex_unlock(&hotplug_driver_mutex);
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t show_tempfreq_hotplug_driver(char *buf)
{
	return sprintf(buf, "%s", hotplug_driver->name);
}

static ssize_t show_available_hotplug_drivers(char *buf)
{
	int i = 0;
	int offset = 0;
	struct hotplug_drivers_list *entry;
	list_for_each_entry(entry, &hotplug_drivers_list.list, list) {
		if(i) {
			offset += sprintf(buf + offset, " ");
		}
		offset += sprintf(buf + offset, "%s", entry->driver->name);
		i++;
	}
	return offset;
}
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER
static ssize_t store_hotplug_epoch_ms(const char *_buf, size_t count)
{
	int val, err = 0;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err) {
		goto out;
	}

	if(val <= 0) {
		err = -EINVAL;
		goto out;
	}

	phonelab_tempfreq_hotplug_epoch_ms = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t store_hotplug_epochs_up(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val <= 0) {
		err = -EINVAL;
		goto out;
	}

	phonelab_tempfreq_hotplug_epochs_up = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}

static ssize_t store_hotplug_epochs_down(const char *_buf, size_t count)
{
	int val, err;
	char *buf = kstrdup(_buf, GFP_KERNEL);
	err = kstrtoint(strstrip(buf), 0, &val);
	if (err)
		goto out;

	if(val <= 0) {
		err = -EINVAL;
		goto out;
	}

	phonelab_tempfreq_hotplug_epochs_down = val;
out:
	kfree(buf);
	return err != 0 ? err : count;
}
#endif

#ifdef CONFIG_PHONELAB_CPUFREQ_GOVERNOR_FIX
static ssize_t show_ignore_bg(char *buf)
{
	return show_ondemand_ignore_bg(buf);
}
static ssize_t store_ignore_bg(const char *_buf, size_t count)
{
	return set_ondemand_ignore_bg(_buf, count);
}
#endif


tempfreq_attr_rw(enable);

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
tempfreq_attr_rw(mpdecision_blocked);
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
tempfreq_attr_rw(binary_threshold_temp);
tempfreq_attr_rw(binary_critical);
tempfreq_attr_rw(binary_lower_threshold);
tempfreq_attr_rw(binary_short_epochs);
tempfreq_attr_rw(binary_short_diff_limit);
tempfreq_attr_rw(binary_long_epochs);
tempfreq_attr_rw(binary_long_diff_limit);
tempfreq_attr_rw(binary_jump_lower);
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_HOTPLUG_DRIVER
tempfreq_attr_plain_rw(tempfreq_hotplug_driver);
tempfreq_attr_plain_ro(available_hotplug_drivers);
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER
tempfreq_attr_rw(hotplug_epoch_ms);
tempfreq_attr_rw(hotplug_epochs_up);
tempfreq_attr_rw(hotplug_epochs_down);
#endif

#ifdef CONFIG_PHONELAB_CPUFREQ_GOVERNOR_FIX
tempfreq_attr_plain_rw(ignore_bg);
#endif

static struct attribute *attrs[] = {
#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
	&enable.attr,
	&binary_threshold_temp.attr,
	&binary_critical.attr,
	&binary_lower_threshold.attr,
	&binary_short_epochs.attr,
	&binary_short_diff_limit.attr,
	&binary_long_epochs.attr,
	&binary_long_diff_limit.attr,
	&binary_jump_lower.attr,
#endif
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
	&mpdecision_blocked.attr,
#endif
#ifdef CONFIG_PHONELAB_TEMPFREQ_HOTPLUG_DRIVER
	&tempfreq_hotplug_driver.attr,
	&available_hotplug_drivers.attr,
	&hotplug_epoch_ms.attr,
#endif
#ifdef CONFIG_PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER
	&hotplug_epochs_up.attr,
	&hotplug_epochs_down.attr,
#endif
#ifdef CONFIG_PHONELAB_CPUFREQ_GOVERNOR_FIX
	&ignore_bg.attr,
#endif
	NULL
};

int tempfreq_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	ssize_t ret = -EINVAL;
	struct tempfreq_attr *fattr = tf_to_attr(attr);
	if (fattr->show)
		ret = fattr->show(buf);
	else
		ret = -EIO;
	return ret;
}

ssize_t tempfreq_store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct tempfreq_attr *fattr = tf_to_attr(attr);
	ssize_t ret = -EINVAL;
	//printk(KERN_DEBUG "tempfreq: %s: %s\n", __func__, buf);
	if (fattr->store) {
		ret = fattr->store(buf, count);
	}
	else
		ret = -EIO;
	return ret;
}

static const struct sysfs_ops sysfs_ops = {
	.show	= tempfreq_show,
	.store	= tempfreq_store,
};


static struct kobj_type tempfreq_ktype = {
	.sysfs_ops = &sysfs_ops,
	.default_attrs = attrs,
};

struct kobject tempfreq_kobj;

static int __init init_tempfreq_sysfs(void)
{
	int ret = 0;
	struct kobject *kobj = &tempfreq_kobj;
	ret = kobject_init_and_add(kobj, &tempfreq_ktype,
				   NULL, "tempfreq");
//	ret = sysfs_create_group(tempfreq_kobj, &tempfreq_attr_group);
//	if (ret)
//		kobject_put(tempfreq_kobj);
//	else
//		kobject_uevent(tempfreq_kobj, KOBJ_ADD);


	return ret;
}
late_initcall(init_tempfreq_sysfs);




/* Initcall stuff */

/* Phone state initcall */
static int __init init_phone_state(void)
{
	int cpu;
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *table;
	int i;

	phone_state = kmalloc(sizeof(struct phone_state), GFP_KERNEL);
	if(phone_state == NULL) {
		return ENOMEM;
	}
	phone_state->ncpus = 4;	//FIXME: Use nr_cpu_ids or some equivalent variable
	phone_state->cpu_states = kmalloc(sizeof(struct cpu_state *) * phone_state->ncpus, GFP_KERNEL);
	if(phone_state->cpu_states == NULL) {
		kfree(phone_state);
		return ENOMEM;
	}
	for_each_possible_cpu(cpu) {
		struct cpu_state *cs = kmalloc(sizeof(struct cpu_state), GFP_KERNEL);
		if(cs == NULL) {
			// FIXME: There is a leak here..we just return ENOMEM..
			// We should be releasing previously allocated memory.
			return ENOMEM;
		}
		cs->cpuid = cpu;
		(void) policy;
#if 0
		policy = cpufreq_cpu_get(cpu);
		if(policy != NULL) {
			if(policy->governor != NULL) {
//				snprintf(cs->governor, CPUFREQ_NAME_LEN, "%s", policy->governor->name);
//				if(strncmp(policy->governor->name, "ondemand", CPUFREQ_NAME_LEN) == 0) {
					cs->enabled = 1;
//				}
			}
			cs->cur_freq = policy->cur;
			cs->cur_idx = get_frequency_index(cs->cur_freq);
			cs->cur_max_idx = get_frequency_index(policy->max);
		}
		cpufreq_cpu_put(policy);
#endif
		cs->enabled = 1;
		phone_state->cpu_states[cpu] = cs;
	}

	// Now initialize FREQUENCIES
	// XXX: Assumes all CPUs share the same table
	table = per_cpu(cpufreq_show_table, 0);

        for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
                  if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
                          continue;
	}

	// XXX: FREQUENCIES expects to be in sorted order..
	// We do not explicitly sort.
	num_frequencies = i;
	FREQUENCIES = kmalloc(sizeof(int) * (num_frequencies + 1), GFP_KERNEL);
        for (i = 0; i < num_frequencies; i++) {
		FREQUENCIES[i] = table[i].frequency;
	}
	FREQUENCIES[num_frequencies] = CPUFREQ_TABLE_END;

	printk(KERN_DEBUG "tempfreq: Initialized phone state\n");
	return 0;
}


/* Thermal callback initcall */
extern struct srcu_notifier_head thermal_notifier_list;

static struct notifier_block __refdata tempfreq_temp_notifier = {
    .notifier_call = tempfreq_thermal_callback,
};

static struct notifier_block __refdata tempfreq_cpufreq_notifier = {
    .notifier_call = tempfreq_cpufreq_callback,
};

static struct notifier_block __cpuinitdata tempfreq_hotplug_notifier = {
    .notifier_call = tempfreq_hotplug_callback,
};

static int __init init_tempfreq_callbacks(void)
{
	int ret = srcu_notifier_chain_register(
			&thermal_notifier_list, &tempfreq_temp_notifier);
	if(ret) {
		printk(KERN_ERR "tempfreq: Failed to register notifier callback: %d\n", ret);
//		panic("tempfreq: Failed to register notifier callback\n");
	} else {
		printk(KERN_DEBUG "tempfreq: Registered notifier: %d\n", ret);
	}

	ret = cpufreq_register_notifier(&tempfreq_cpufreq_notifier, CPUFREQ_TRANSITION_NOTIFIER);
	if(ret) {
		printk(KERN_ERR "tempfreq: Failed to register notifier callback: %d\n", ret);
//		panic("tempfreq: Failed to register notifier callback\n");
	} else {
		printk(KERN_DEBUG "tempfreq: Registered notifier: %d\n", ret);
	}

	register_cpu_notifier(&tempfreq_hotplug_notifier);
	return 0;
}

static int __init init_tempfreq(void)
{
	// Initialize state before callbacks
#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_CGROUP_THROTTLING
	init_tempfreq_thermal_cgroup_throttling();
#endif
	init_phone_state();
	init_tempfreq_callbacks();
	return 0;
}
late_initcall(init_tempfreq);

