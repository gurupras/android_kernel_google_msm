#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/pid.h>
#include <linux/types.h>
#include <linux/cpu.h>

#include <linux/atomic.h>

#include <linux/list.h>
#include <linux/workqueue.h>

#include <linux/phonelab.h>

#include <trace/events/phonelab.h>

struct periodic_work {
	struct work_struct work;
	struct delayed_work dwork;
	int cpu;
};


DEFINE_PER_CPU(struct task_struct *[CTX_SWITCH_INFO_LIM], ctx_switch_info);
DEFINE_PER_CPU(int, ctx_switch_info_idx);

DEFINE_PER_CPU(struct periodic_work, periodic_ctx_switch_info_work);

int periodic_ctx_switch_info_ready;

void periodic_ctx_switch_info(struct work_struct *w) {

	int cpu, wcpu;
	int i, lim;
	struct task_struct *task;
	struct delayed_work *work, *dwork;
	struct periodic_work *pwork;

	cpu = smp_processor_id();

	dwork = container_of(w, struct delayed_work, work);
	pwork = container_of(dwork, struct periodic_work, dwork);
	wcpu = pwork->cpu;
	if(unlikely(cpu != wcpu)) {
		cpu = wcpu;
	}

	if(unlikely(!periodic_ctx_switch_info_ready))
		goto out;
	lim = per_cpu(ctx_switch_info_idx, cpu);

	for(i = 0; i < lim; i++) {
		task = per_cpu(ctx_switch_info[i], cpu);
		trace_phonelab_periodic_ctx_switch_info(task, cpu);
		per_cpu(ctx_switch_info[i], cpu) = NULL;
	}
	per_cpu(ctx_switch_info_idx, cpu) = 0;
out:
	work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
	schedule_delayed_work_on(cpu, work, msecs_to_jiffies(100));
}

static void
clear_cpu_ctx_switch_info(int cpu)
{
	int i;
	int lim = per_cpu(ctx_switch_info_idx, cpu);
	for(i = 0; i < lim; i++) {
		per_cpu(ctx_switch_info[i], cpu) = NULL;
	}
	per_cpu(ctx_switch_info_idx, cpu) = 0;
}

static int
hotplug_handler(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
	struct delayed_work *work;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		clear_cpu_ctx_switch_info(cpu);
		work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
		schedule_delayed_work(work, msecs_to_jiffies(100));
		break;
#ifdef CONFIG_HOTPLUG_CPU
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		clear_cpu_ctx_switch_info(cpu);
		work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
		cancel_delayed_work(work);
		break;
#endif
	};
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata hotplug_notifier = {
	.notifier_call		= hotplug_handler,
};

static int __init init_periodic_ctx_switch_info(void) {

	int cpu;
	struct delayed_work *work;
	struct periodic_work *pwork;
	for_each_possible_cpu(cpu) {
		pwork = &per_cpu(periodic_ctx_switch_info_work, cpu);
		work = &pwork->dwork;
		INIT_DELAYED_WORK(work, periodic_ctx_switch_info);
		pwork->cpu = cpu;
	}
	for_each_online_cpu(cpu) {
		work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
		schedule_delayed_work_on(cpu, work, msecs_to_jiffies(100));
	}

	(void) hotplug_notifier;
	register_cpu_notifier(&hotplug_notifier);

	periodic_ctx_switch_info_ready = 1;
	printk(KERN_DEBUG "periodic: init done\n");

	return 0;
}
module_init(init_periodic_ctx_switch_info);


static int __init init_per_cpu_data(void) {
	int i;
	for_each_possible_cpu(i) {
		per_cpu(ctx_switch_info_idx, i) = 0;
	}
	periodic_ctx_switch_info_ready = 0;
	return 0;
}
early_initcall(init_per_cpu_data);

