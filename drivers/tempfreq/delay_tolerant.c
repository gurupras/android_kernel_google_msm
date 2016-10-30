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
#include <linux/smp.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/syscore_ops.h>
#include <linux/sched.h>

#include <linux/phonelab.h>
#include <linux/heap.h>

#include "tempfreq.h"
#include "netlink.h"

#include <../kernel/sched/sched.h>

extern int android_foreground_pid;

void move_to_cgroup_netlink_cmd(pid_t pid, char *cgroup_name, bool should_assign_cpuset)
{
	struct netlink_cmd cmd;
	memset(&cmd, 0, sizeof(struct netlink_cmd));
	strcpy(cmd.cmd, "move_to_cgroup");
	sprintf(cmd.args, "%d %s %s",
			pid,
			cgroup_name,
			should_assign_cpuset == 1 ? "true" : "false"
	);
	cmd.cmd_len = strlen(cmd.cmd);
	cmd.args_len = strlen(cmd.args);
	netlink_send(&cmd);
}

void make_delay_tolerant(struct task_struct *tsk)
{
	move_to_cgroup_netlink_cmd(tsk->pid, "delay_tolerant", true);

	// Attempt to make the task unschedulable
	__set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(MAX_SCHEDULE_TIMEOUT);

}


u64 time_since_epoch_ms(void) {
	struct timespec ts;
	u64 ret;
	getnstimeofday(&ts);
	ret = ((u64) (ts.tv_sec * MSEC_PER_SEC)) + div_u64(ts.tv_nsec, 1000000);
	return ret;
}

// TODO: Add logic to count down and move thread out of delay_tolerant cgroup
void countdown_delay_tolerant_timers(void)
{
	struct cgroup_iter it;
	struct task_struct *tsk;
	u64 now, diff;
	char cgrp_name[32];

	struct sched_param sp;
	sp.sched_priority = 0;


	now = time_since_epoch_ms();

	cgroup_iter_start(delay_tolerant, &it);
	while ((tsk = cgroup_iter_next(delay_tolerant, &it))) {
		diff = now - tsk->delay_tolerance_start_ms;
		printk(KERN_DEBUG "tempfreq: %s: %05d (%s) start_ms: %llu  now_ms: %llu  diff_ms: %llu\n",
				__func__,
				tsk->pid,
				tsk->comm,
				tsk->delay_tolerance_start_ms,
				now,
				diff);

		tsk->remaining_delay_ms = tsk->delay_tolerance_ms - diff;
		if(tsk->remaining_delay_ms < 0) {
			tsk->delay_tolerance_ms = 0;
			tsk->remaining_delay_ms = 0;
			tsk->delay_tolerance_start_ms = 0;
			(void) sp;
//			sched_setscheduler(tsk, SCHED_NORMAL, &sp);
			// TODO: Move this task out of delay_tolerant
			if(tsk->tgid == android_foreground_pid) {
				// Move to fg_bg cgroup
				sprintf(cgrp_name, "fg_bg");
			} else {
				// Move to bg_non_interactive cgroup
				sprintf(cgrp_name, "bg_non_interactive");
			}
			move_to_cgroup_netlink_cmd(tsk->pid, cgrp_name, 1);
			wake_up_process(tsk);
			printk(KERN_DEBUG "tempfreq: %s: Should have moved %05d (%s) out of delay_tolerant cgroup\n", __func__, tsk->pid, tsk->comm);
		}
	}
	cgroup_iter_end(delay_tolerant, &it);
}
