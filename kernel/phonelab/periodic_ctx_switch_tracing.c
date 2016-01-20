#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/pid.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/spinlock_types.h>

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
DEFINE_PER_CPU(spinlock_t, ctx_switch_info_lock);
DEFINE_PER_CPU(int, test_field);
DEFINE_PER_CPU(struct periodic_work, periodic_ctx_switch_info_work);

int periodic_ctx_switch_info_ready;

static void clear_cpu_ctx_switch_info(int cpu);

void periodic_ctx_switch_info(struct work_struct *w) {

	int cpu, wcpu;
	int i, lim;
	struct task_struct *task;
	struct delayed_work *work, *dwork;
	struct periodic_work *pwork;
	spinlock_t *spinlock;
	unsigned long utime, stime, flags;

	cpu = smp_processor_id();

	dwork = container_of(w, struct delayed_work, work);
	pwork = container_of(dwork, struct periodic_work, dwork);
	wcpu = pwork->cpu;
	if(unlikely(cpu != wcpu)) {
		printk(KERN_DEBUG "periodic: wrong cpu (%d != %d)..restarting\n", cpu, wcpu);
		cpu = wcpu;
		goto out;
	}

	(void) utime;
	(void) stime;
	(void) spinlock;
	(void) flags;

	if(unlikely(!periodic_ctx_switch_info_ready))
		goto out;
	local_irq_save(flags);
		spinlock = &per_cpu(ctx_switch_info_lock, cpu);
		spin_lock(spinlock);

		per_cpu(test_field, cpu) = 1;
		lim = per_cpu(ctx_switch_info_idx, cpu);

//		printk(KERN_DEBUG "periodic: cpu=%d lim=%d\n", cpu, lim);
		for(i = 0; i < lim; i++) {
			task = per_cpu(ctx_switch_info[i], cpu);
			task_lock(task);
			if(!task->is_logged[cpu]) {
//				task_times(task, &utime, &stime);
//				printk(KERN_DEBUG "periodic: cpu=%d pid=%d tgid=%d comm=%s utime_t=%lu stime_t=%lu cutime=%lu cstime=%lu"
//					"cutime_t=%lu cstime_t=%lu cutime=%lu cstime=%lu",
//					cpu, task->pid, task->tgid, task->comm,
//					task->utime, task->stime,
//					cputime_to_clock_t(utime), cputime_to_clock_t(stime),
//					task->signal->cutime, task->signal->cstime,
//					cputime_to_clock_t(task->signal->cutime), cputime_to_clock_t(task->signal->cstime));
				trace_phonelab_periodic_ctx_switch_info(task, cpu);
				task->is_logged[cpu] = 1;
			}
			task_unlock(task);
		}
		clear_cpu_ctx_switch_info(cpu);
		per_cpu(test_field, cpu) = 0;

		spin_unlock(spinlock);
	local_irq_restore(flags);
out:
	work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
	schedule_delayed_work_on(cpu, work, msecs_to_jiffies(100));
}

static void
clear_cpu_ctx_switch_info(int cpu)
{
	int i;
	struct task_struct *task;
	for(i = 0; i < CTX_SWITCH_INFO_LIM; i++) {
		task = per_cpu(ctx_switch_info[i], cpu);
		if(task == NULL)
			continue;
		task_lock(task);
		task->is_logged[cpu] = 0;
		task_unlock(task);
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
		printk(KERN_DEBUG "periodic: scheduling work for CPU: %ld\n", cpu);
		clear_cpu_ctx_switch_info(cpu);
		work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
		schedule_delayed_work(work, msecs_to_jiffies(100));
		break;
#ifdef CONFIG_HOTPLUG_CPU
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		printk(KERN_DEBUG "periodic: cancelling work for CPU: %ld\n", cpu);
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
		spin_lock_init(&per_cpu(ctx_switch_info_lock, i));
	}
	periodic_ctx_switch_info_ready = 0;
	return 0;
}
early_initcall(init_per_cpu_data);

