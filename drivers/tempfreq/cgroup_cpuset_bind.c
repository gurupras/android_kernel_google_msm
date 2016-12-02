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

#include <linux/cgroup.h>
#include "tempfreq.h"

#include <trace/events/tempfreq.h>

struct cgroup *fg_bg, *bg_non_interactive, *delay_tolerant;
struct cgroup *cs_fg_bg, *cs_bg_non_interactive, *cs_delay_tolerant;

// Expects cgroup_mutex to be acquired
int copy_tasks_cgroup_to_cgroup(struct cgroup *from, struct cgroup *to)
{
	struct cgroup_iter it;
	struct task_struct *tsk;
	int pid;
	int ret = 0;
	int n = 0;
	int failed = 0;
#ifdef DEBUG
	u64 ns = sched_clock();
#endif

	cgroup_iter_start(from, &it);
	while ((tsk = cgroup_iter_next(from, &it))) {
		n++;
		pid = tsk->pid;
		cgroup_attach_task(to, tsk);
		if(ret) {
			failed++;
		}
	}
	cgroup_iter_end(from, &it);
	//printk(KERN_DEBUG "tempfreq: %s: n=%d failed=%d ret=%d\n", __func__, n, failed, ret);
	trace_tempfreq_cgroup_copy_tasks(n, failed);
	return ret;
#ifdef DEBUG
	trace_tempfreq_timing(__func__, sched_clock() - ns);
#endif
}

static void copy_fn(struct work_struct *work)
{
	copy_tasks_cgroup_to_cgroup(bg_non_interactive, cs_bg_non_interactive);
}

DECLARE_WORK(bind_copy_work, copy_fn);


// delay tolerant cpuset gets no CPUs
static int __init init_make_delay_tolerant_dead(void)
{
	return 0;
}
late_initcall(init_make_delay_tolerant_dead);


