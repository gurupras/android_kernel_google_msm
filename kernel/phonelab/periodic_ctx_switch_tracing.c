#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/pid.h>
#include <linux/types.h>

#include <linux/atomic.h>

#include <linux/list.h>
#include <linux/workqueue.h>

#include <linux/phonelab.h>

#include <trace/events/phonelab.h>


DEFINE_PER_CPU(struct task_struct *[CTX_SWITCH_INFO_LIM], ctx_switch_info);
DEFINE_PER_CPU(int, ctx_switch_info_idx);

static struct delayed_work periodic_ctx_switch_info_work;

static void periodic_ctx_switch_info(struct work_struct *work) {
	int cpu;
	int i, lim;
	struct task_struct *task;
	for_each_online_cpu(cpu) {
		lim = per_cpu(ctx_switch_info_idx, cpu);
	//	printk(KERN_DEBUG "periodic_ctx_switch: lim: %d\n", lim);
		for(i = 0; i < lim; i++) {
			task = per_cpu(ctx_switch_info[i], cpu);
			trace_phonelab_periodic_ctx_switch_info(task, cpu);
			per_cpu(ctx_switch_info[i], cpu) = NULL;
		}
		per_cpu(ctx_switch_info_idx, cpu) = 0;
	}
	schedule_delayed_work(&periodic_ctx_switch_info_work, 10);
}

static int __init init_periodic_ctx_switch_info(void) {
	INIT_DELAYED_WORK(&periodic_ctx_switch_info_work, periodic_ctx_switch_info);
	schedule_delayed_work(&periodic_ctx_switch_info_work, 1);
	return 0;
}
fs_initcall(init_periodic_ctx_switch_info);
