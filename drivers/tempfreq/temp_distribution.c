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

static int init_temp_list(struct temp_list **tlist, int duration_ms)
{
	int max_elements = (duration_ms + TEMP_FREQUENCY_MS) / TEMP_FREQUENCY_MS;
	struct temp_list *tl;

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
	// The window is full
	// Find the number of elements to get to 25th percentile
	int npn = (tl->max_elements + x) / x;
	int i, j;

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

	if(likely(stl->num_elements == stl->max_elements)) {
		p25 = get_nth_percentile(stl, 25);
		if(p25 != -1) {
			trace_tempd_pn("short", 25, p25);
		}
	}
	// TODO: If the current temperature is below p25, then unblock
#ifdef DEBUG
	trace_tempd_timing(__func__, sched_clock() - ns);
#endif
	return 0;
}

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

static int __init init_temp_distribution(void)
{
	// Initialize state before callbacks
	//init_temp_list(&short_temp_list, 15 * MSEC_PER_SEC);
	init_temp_list(&short_temp_list, 15 * 60 * MSEC_PER_SEC);
	init_temp_list(&long_temp_list, 60 * 60 * MSEC_PER_SEC);
	// Callbacks
	init_temp_distribution_callback();
	return 0;
}
late_initcall(init_temp_distribution);

