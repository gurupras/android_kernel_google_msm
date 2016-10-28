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

#include "tempfreq.h"
#include "netlink.h"

extern int android_foreground_pid;

void make_delay_tolerant(pid_t pid, u64 duration_ms)
{
	struct netlink_cmd cmd;

	memset(&cmd, 0, sizeof(struct netlink_cmd));
	strcpy(cmd.cmd, "dt");
	sprintf(cmd.args, "%d %llu", pid, duration_ms);
	cmd.cmd_len = strlen(cmd.cmd);
	cmd.args_len = strlen(cmd.args);

	netlink_send(&cmd);
	set_current_state(TASK_UNINTERRUPTIBLE);
//	schedule();
}


u64 time_since_epoch_ns(void) {
	struct timespec ts;
	getnstimeofday(&ts);
	return (ts.tv_sec * NSEC_PER_SEC) + ts.tv_nsec;
}

// TODO: Add logic to count down and move thread out of delay_tolerant cgroup
void countdown_delay_tolerant_timers(void)
{
	struct cgroup_iter it;
	struct task_struct *tsk;
	u64 now;

	now = time_since_epoch_ns();

	cgroup_iter_start(delay_tolerant, &it);
	while ((tsk = cgroup_iter_next(delay_tolerant, &it))) {
		tsk->remaining_delay_ms -= div_u64((now - tsk->delay_tolerance_start_ns), 1000000);
		if(tsk->remaining_delay_ms < 0) {
			tsk->delay_tolerance_ms = 0;
			tsk->remaining_delay_ms = 0;
			tsk->delay_tolerance_start_ns = 0;
			set_task_state(tsk, TASK_INTERRUPTIBLE);
			// TODO: Move this task out of delay_tolerant
			if(tsk->tgid == android_foreground_pid) {
				if(attach_task_by_pid(fg_bg, tsk->pid, false) != 0) {
					printk(KERN_DEBUG "tempfreq: %s: Failed to attach task by pid to fg_bg\n", __func__);
				}
			} else {
				// Move to bg_non_interactive cgroup
				if(attach_task_by_pid(bg_non_interactive, tsk->pid, false) != 0) {
					printk(KERN_DEBUG "tempfreq: %s: Failed to attach task by pid to bg_non_interactive\n", __func__);
				}
			}
		}
	}
	cgroup_iter_end(delay_tolerant, &it);
}
