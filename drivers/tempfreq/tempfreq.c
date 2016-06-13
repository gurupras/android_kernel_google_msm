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

#define TEMPFREQ_BINARY_THRESHOLD_TEMP		70
#define TEMPFREQ_BINARY_UPPER_THRESHOLD		75
#define TEMPFREQ_BINARY_LOWER_THRESHOLD		60
#define TEMPFREQ_BINARY_SHORT_EPOCHS		2
#define TEMPFREQ_BINARY_SHORT_DIFF_LIMIT	3

#define TEMPFREQ_BINARY_LONG_EPOCHS		5
#define TEMPFREQ_BINARY_LONG_DIFF_LIMIT		2

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

static inline int get_cpu_with(int relation);
static inline int get_frequency_index(int frequency);
static inline int get_new_frequency(int cpu, int relation);
static inline int get_next_frequency_index(int idx);

static void cpu_state_string(struct cpu_state *cs, char *str);


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

	for(i = 0; i < phone_state->ncpus; i++) {
		enabled |= phone_state->cpu_states[i]->enabled;
	}
	if(!enabled) {
		return 0;
	}

	// Handle
//	printk(KERN_DEBUG "tempfreq: Callback!\n");
	trace_tempfreq_temp(temp);
#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
	short_epochs_counted++;
	long_epochs_counted++;

	if(temp > TEMPFREQ_BINARY_THRESHOLD_TEMP) {
		if(short_epochs_counted == TEMPFREQ_BINARY_SHORT_EPOCHS) {
			// We're above the TEMPFREQ_BINARY_UPPER_THRESHOLD and
			// if temperature increased by more than
			// TEMPFREQ_BINARY_DIFF_LIMIT degrees in TEMPFREQ_BINARY_EPOCHS
			// consecutive readings, we halve
			if(temp - prev_short_epoch_temp > TEMPFREQ_BINARY_SHORT_DIFF_LIMIT) {
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
	if(temp >= TEMPFREQ_BINARY_UPPER_THRESHOLD) {
		// This is the upper limit.
		// We need to move frequency down
		cpu = get_cpu_with(HIGHEST);
		op = LOWER;
		is_increase = 0;
		prev_temp = 0;
		reason_int = REASON_CRITICAL;
		goto done;
	}
	else if(temp <= TEMPFREQ_BINARY_LOWER_THRESHOLD) {
		// We can now increase the limit on the lowest
		cpu = get_cpu_with(LOWEST);
		op = HIGHER;
		is_increase = 1;
		prev_temp = 0;
		reason_int = REASON_COOL;
		goto done;
	}

	// Now check for the stable long epoch
	if(long_epochs_counted >= TEMPFREQ_BINARY_LONG_EPOCHS) {
		// If we've remained stable for a long period
		// Regardless of frequency
		if(temp - prev_long_epoch_temp < TEMPFREQ_BINARY_LONG_DIFF_LIMIT) {
			if(temp < TEMPFREQ_BINARY_THRESHOLD_TEMP) {
				cpu = get_cpu_with(LOWEST);
				op = HIGHER;
				is_increase = 1;
				reason_int = REASON_LONG_EPOCH_DIFF;
				prev_temp = prev_long_epoch_temp;
			}
			else {
				if(temp < TEMPFREQ_BINARY_UPPER_THRESHOLD - 2) {	//FIXME: Currently '2' is hard-coded
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
		return 0;
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
	return 0;
}
EXPORT_SYMBOL_GPL(tempfreq_thermal_callback);

static int tempfreq_cpufreq_callback(struct notifier_block *nfb,
					unsigned long action, void *temp_ptr)
{
	struct cpufreq_freqs *freqs;
	int cpu;
	struct cpu_state *cs;

	freqs = (struct cpufreq_freqs *) temp_ptr;
	if(freqs == NULL) {
		printk(KERN_ERR "tempfreq: %s: freqs is NULL?\n", __func__);
	}

	cpu = freqs->cpu;
	cs = phone_state->cpu_states[cpu];

	if(action != CPUFREQ_POSTCHANGE)
		return 0;
	if(cs->cur_freq != freqs->old) {
		printk(KERN_WARNING "tempfreq: %s: cpu_state->cur_freq (%d) != freqs->old (%d)\n", __func__, cs->cur_freq, freqs->old);
	}
	cs->cur_freq = freqs->new;
	cs->cur_idx = get_frequency_index(cs->cur_freq);

	return 0;
}
EXPORT_SYMBOL_GPL(tempfreq_cpufreq_callback);






static int __get_frequency_index(int frequency, int start, int end);
static inline int get_frequency_index(int frequency)
{
	return __get_frequency_index(frequency, 0, num_frequencies);
}

static int __get_frequency_index(int frequency, int start, int end)
{
	int mid = (start + end) / 2;

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
}

static inline int get_new_frequency(int cpu, int relation)
{
	struct cpufreq_policy *policy;
	struct cpu_state *cs;
	int cur_idx, result_idx;

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
			result_idx = (cur_idx - 2) >= 0 ? cur_idx - 2 : 0;
			break;
	}
	return FREQUENCIES[result_idx];
}

static inline int get_cpu_with(int relation)
{
	int cpu;
	struct cpu_state *cs = NULL;

	int culprit_f_idx = relation == HIGHEST ? 0 : num_frequencies;
	int culprit_cpu = -1;
	// XXX: Check how all of this interacts with hotplug
	// Eventually, we will be the ones controlling hotplug, so
	// we should be able to maintain this state in the phone state
	for_each_online_cpu(cpu) {
		cs = phone_state->cpu_states[cpu];
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

	return 0;
}

static int __init init_tempfreq(void)
{
	init_tempfreq_callbacks();
	init_phone_state();
	return 0;
}
late_initcall(init_tempfreq);

