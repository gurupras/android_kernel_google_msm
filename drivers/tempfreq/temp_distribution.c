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

#include <linux/phonelab.h>
#include <linux/heap.h>

#define CREATE_TRACE_POINTS
#include <trace/events/tempd.h>

#include "tempfreq.h"


struct temp {
	int temp;
	struct list_head list;
};

struct temp_list *long_temp_list, *short_temp_list;

#define duration_to_elements(duration) (div_u64((duration + TEMP_FREQUENCY_MS), TEMP_FREQUENCY_MS))
#define elements_to_duration(elements) ((elements * TEMP_FREQUENCY_MS) - TEMP_FREQUENCY_MS)
/*
static inline u64 duration_to_elements(u64 duration)
{
	return div_u64(duration + TEMP_FREQUENCY_MS, TEMP_FREQUENCY_MS);
}

static inline u64 elements_to_duration(u64 elements)
{
	return ((elements * TEMP_FREQUENCY_MS) - TEMP_FREQUENCY_MS);
}
*/

static int init_temp_list(struct temp_list **tlist, u64 duration_ms)
{
	struct temp_list *tl;
	u64 max_elements = duration_to_elements(duration_ms);
	*tlist = kmalloc(sizeof(struct temp_list), GFP_KERNEL);
	tl = *tlist;
	if(tl == NULL) {
		return -ENOMEM;
	}
	tl->num_elements = 0;
	tl->max_elements = max_elements;
	memset(&tl->temperatures, 0, (MAX_TEMPERATURE - MIN_TEMPERATURE) * sizeof(int));
	INIT_LIST_HEAD(&tl->list);
#if 0
	// Now the heaps
	tl->min_heap = create_min_heap((max_elements * 3) / 4);
	tl->max_heap = create_max_heap((max_elements + 4) / 4);
#endif
	return 0;
}

static int temp_list_del_head(struct temp_list *tl)
{
	struct temp *entry, *tmp;
	int temp;
	list_for_each_entry_safe(entry, tmp, &tl->list, list) {
		temp = entry->temp;
		list_del(&entry->list);
		tl->num_elements--;
		tl->temperatures[temp]--;
		if(tl->temperatures[temp] < 0) {
			printk(KERN_ERR "temp_distribution: %s: Negative count for temperature: %d\n", __func__, temp);
		}
		kfree(entry);
		break;
	}
	return 0;
}

static int temp_list_add(struct temp_list *tl, int temp)
{
	struct temp *t;
	if(tl->num_elements == tl->max_elements) {
		//TODO: Remove first element
		temp_list_del_head(tl);
	}
	// Create struct temp from this temp
	t = kmalloc(sizeof(struct temp), GFP_KERNEL);
	if(t == NULL) {
		return -ENOMEM;
	}
	t->temp = temp;
	list_add_tail(&t->list, &tl->list);
	tl->num_elements++;
	tl->temperatures[temp]++;
	return 0;
}

static void build_heap_from_temp_list(struct temp_list *tl)
{
	struct temp *entry;
	int etemp;

	list_for_each_entry(entry, &tl->list, list) {
		etemp = entry->temp;
		if(tl->min_heap->count == -1 && tl->max_heap->count == -1) {
			heap_push(tl->max_heap, etemp);
		} else if(etemp <= heap_peek(tl->max_heap)) {
			if(tl->max_heap->count == tl->max_heap->size) {
				// It is full
				// We move the value to min heap and then push into max
				int tmp = heap_pop(tl->max_heap);
				heap_push(tl->min_heap, tmp);
			}
			heap_push(tl->max_heap, etemp);
		} else {
			// It needs to go to min heap
			if(tl->min_heap->count == tl->min_heap->size) {
				// Pop from min, push to max
				int tmp = heap_pop(tl->min_heap);
				heap_push(tl->max_heap, tmp);
			}
			heap_push(tl->min_heap, etemp);
		}
	}
}

int get_nth_percentile(struct temp_list *tl, int n)
{
#ifdef DEBUG
	u64 ns = sched_clock();
#endif

	int pn = -1;

	int x = 100 / n;
	// The window is not necessarily full
	// Find the number of elements to get to 25th percentile
	u64 npn = div_u64((tl->num_elements), x);
	int i;
	u64 j;

	i = 0;
	j = 0;
	while(i < 100 && j < npn) {
		j += tl->temperatures[i];
		if(j >= npn) {
			pn = i;
			break;
		}
		i++;
	}
#ifdef DEBUG
	trace_tempd_timing(__func__, sched_clock() - ns);
#endif
	return pn;
}

static int temp_distribution_thermal_callback(struct notifier_block *nfb,
					unsigned long action, void *temp_ptr)
{
	long *temp_ptr_long = (long *) temp_ptr;
	long temp = *temp_ptr_long;

	struct temp_list *stl, *ltl;
	int p25;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif

	stl = short_temp_list;
	ltl = long_temp_list;

	(void) stl;
	(void) ltl;
	(void) temp;
	(void) p25;
	(void) build_heap_from_temp_list;
	(void) temp_list_add;

	temp_list_add(stl, temp);
	temp_list_add(ltl, temp);

#if 0
	if(likely(stl->num_elements == stl->max_elements)) {
		p25 = get_nth_percentile(stl, 25);
		if(p25 != -1) {
			trace_tempd_pn("short", 25, p25);
		}
	}
	// TODO: If the current temperature is below p25, then unblock
	// We do this in tempfreq.c when checking each cgroup. So this remains commented
#endif
#ifdef DEBUG
	trace_tempd_timing(__func__, sched_clock() - ns);
#endif
	return 0;
}

/* sysfs hooks */


/* Thermal callback initcall */
extern struct srcu_notifier_head thermal_notifier_list;

static struct notifier_block __refdata temp_distribution_temp_notifier = {
    .notifier_call = temp_distribution_thermal_callback,
};


static int __init init_temp_distribution_callback(void)
{
	int ret = srcu_notifier_chain_register(
			&thermal_notifier_list, &temp_distribution_temp_notifier);
	if(ret) {
		printk(KERN_ERR "temp_distribution: Failed to register notifier callback: %d\n", ret);
	} else {
		printk(KERN_DEBUG "temp_distribution: Registered notifier: %d\n", ret);
	}
	return 0;
}

#define MSEC_PER_HOUR (60 * 60 * MSEC_PER_SEC)

static int __init init_temp_distribution(void)
{
	// Initialize state before callbacks
	//init_temp_list(&short_temp_list, 15 * MSEC_PER_SEC);
	init_temp_list(&short_temp_list, 2 * MSEC_PER_HOUR);
	init_temp_list(&long_temp_list, 24 * MSEC_PER_HOUR);
	// Callbacks
	init_temp_distribution_callback();
	return 0;
}
late_initcall(init_temp_distribution);


/* sysfs */
#define list_sysfs_hooks(name)									\
static ssize_t show_##name##_duration(char *buf)						\
{												\
	return sprintf(buf, "%llu", elements_to_duration(name->max_elements));			\
	return 0;	\
}												\
												\
static ssize_t store_##name##_duration(const char *_buf, size_t count)				\
{												\
	char *buf = kstrdup(_buf, GFP_KERNEL);							\
	struct temp *entry, *tmp;								\
	int err;										\
	s64 val;										\
	int temp;										\
	err = kstrtoull(strstrip(buf), 0, &val);						\
	if (err)										\
		goto out;									\
												\
	if(val <= 0) {										\
		err = -EINVAL;									\
		goto out;									\
	}											\
	name->max_elements = val;								\
	count = 0;										\
	list_for_each_entry_safe(entry, tmp, &(name->list), list) {				\
		count++;									\
		if(count > val) {								\
			temp = entry->temp;							\
			list_del(&entry->list);							\
			name->num_elements--;							\
			name->temperatures[temp]--;						\
			if(name->temperatures[temp] < 0) {					\
				printk(KERN_ERR "temp_distribution: %s: Negative count for temperature: %d\n", __func__, temp);	\
			}									\
			kfree(entry);								\
		}										\
	}											\
out:												\
	kfree(buf);										\
	return err != 0 ? err : count;								\
}												\
static struct tempfreq_attr name##_sysfs =							\
__ATTR(name##_duration, 0644, show_##name##_duration, store_##name##_duration)

list_sysfs_hooks(short_temp_list);
list_sysfs_hooks(long_temp_list);

static ssize_t show_short_term_list(char *buf)
{
	int offset = 0;
	struct temp *entry, *tmp;
	list_for_each_entry_safe(entry, tmp, &short_temp_list->list, list) {
		offset += sprintf(buf + offset, "%d ", entry->temp);
	}
	return offset;
}

static ssize_t show_long_term_list(char *buf)
{
	int offset = 0;
	struct temp *entry, *tmp;
	list_for_each_entry_safe(entry, tmp, &long_temp_list->list, list) {
		offset += sprintf(buf + offset, "%d ", entry->temp);
	}
	return offset;
}

tempfreq_attr_plain_ro(short_term_list);
tempfreq_attr_plain_ro(long_term_list);


static struct attribute *attrs[] = {
	&short_temp_list_sysfs.attr,
	&long_temp_list_sysfs.attr,
	&short_term_list.attr,
	&long_term_list.attr,
	NULL
};

static const struct sysfs_ops sysfs_ops = {
	.show	= tempfreq_show,
	.store	= tempfreq_store,
};

static struct kobj_type tempfreq_ktype = {
	.sysfs_ops = &sysfs_ops,
	.default_attrs = attrs,
};

static int __init init_tempd_sysfs(void)
{
	int ret = 0;
	struct kobject *kobj = &tempfreq_kobj;
	ret = kobject_init_and_add(kobj, &tempfreq_ktype,
				   &tempfreq_kobj, "tempd");

	return ret;
}
late_initcall(init_tempd_sysfs);


