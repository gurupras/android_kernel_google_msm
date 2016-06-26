#undef TRACE_SYSTEM
#define TRACE_SYSTEM tempfreq

#if !defined(_TRACE_TEMPFREQ_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEMPFREQ_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

#include <linux/phonelab.h>

TRACE_EVENT(tempfreq_temp,

	TP_PROTO(long temp),

	TP_ARGS(temp),

	TP_STRUCT__entry(
		__field(	long,		temp		)
	),

	TP_fast_assign(
		__entry->temp		= temp;
	),

	TP_printk("temp=%ld", __entry->temp)
);

#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
TRACE_EVENT(tempfreq_binary_diff,

	TP_PROTO(long temp, long prev_temp, int culprit_cpu, int culprit_freq, int is_increase, int result_freq, const char *reason),

	TP_ARGS(temp, prev_temp, culprit_cpu, culprit_freq, is_increase, result_freq, reason),

	TP_STRUCT__entry(
		__field(	long,		temp		)
		__field(	long,		prev_temp	)
		__field(	int,		culprit_cpu	)
		__field(	int,		culprit_freq	)
		__field(	int,		is_increase	)
		__field(	int,		result_freq	)
		__string(	reason,		reason		)
	),

	TP_fast_assign(
		__entry->temp		= temp;
		__entry->prev_temp	= prev_temp;
		__entry->culprit_cpu	= culprit_cpu;
		__entry->culprit_freq	= culprit_freq;
		__entry->is_increase	= is_increase;
		__entry->result_freq	= result_freq;
		__assign_str(reason, reason);
	),

	TP_printk("temp=%ld prev_temp=%ld culprit_cpu=%d culprit_freq=%d going=%s result_freq=%d reason=%s",
			__entry->temp, __entry->prev_temp,
			__entry->culprit_cpu, __entry->culprit_freq,
			__entry->is_increase == 1 ? "UP" : "DOWN",
			__entry->result_freq, __get_str(reason))
);
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_BG_THROTTLING
TRACE_EVENT(tempfreq_thermal_bg_throttling,

	TP_PROTO(int temp, int idx, int state),

	TP_ARGS(temp, idx, state),

	TP_STRUCT__entry(
		__field(	int,		temp		)
		__field(	int,		idx		)
		__field(	int,		state		)
	),

	TP_fast_assign(
		__entry->temp		= temp;
		__entry->idx		= idx;
		__entry->state		= state;
	),

	TP_printk("temp=%d idx=%d state=%s",
			__entry->temp,
			__entry->idx,
			__entry->state == 0 ? "NORMAL" : "THROTTLED"
	)
);

TRACE_EVENT(tempfreq_thermal_bg_throttling_proc,

	TP_PROTO(struct task_struct *task, int state),

	TP_ARGS(task, state),

	TP_STRUCT__entry(
		__field(	int,		pid			)
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	int,		state			)
	),

	TP_fast_assign(
		__entry->pid		= task->pid;
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
		__entry->state		= state;
	),

	TP_printk("pid=%d comm=%s state=%s",
			__entry->pid,
			__entry->comm,
			__entry->state == 0 ? "NORMAL" : "THROTTLED"
	)
);

#endif

TRACE_EVENT(tempfreq_timing,

	TP_PROTO(const char *func, u64 time_ns),

	TP_ARGS(func, time_ns),

	TP_STRUCT__entry(
		__string(	func,	func	)
		__field(	u64,	time_ns	)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__entry->time_ns	= time_ns;
	),

	TP_printk("func=%s time=%lluns", __get_str(func), __entry->time_ns)
);


#ifdef CONFIG_PHONELAB_TEMPFREQ_HOTPLUG_DRIVER
#ifndef __TEMPFREQ_HOTPLUG_TRACE_HELPERS__
#define __TEMPFREQ_HOTPLUG_TRACE_HELPERS__
static void set_to_string(int set, char buf[10])
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
#endif
TRACE_EVENT(tempfreq_hotplug,

	TP_PROTO(int up_set, int down_set, int overall_up),

	TP_ARGS(up_set, down_set, overall_up),

	TP_STRUCT__entry(
		__array(	char,	up_buf,		10	)
		__array(	char,	down_buf,	10	)
		__array(	char,	overall_up_buf,	10	)
	),

	TP_fast_assign(
		set_to_string(up_set, __entry->up_buf);
		set_to_string(down_set, __entry->down_buf);
		set_to_string(overall_up, __entry->overall_up_buf);
	),

	TP_printk("up=%s down=%s overall_up=%s",
		__entry->up_buf,
		__entry->down_buf,
		__entry->overall_up_buf
	)
);

TRACE_EVENT(tempfreq_hotplug_target,

	TP_PROTO(int online, int target),

	TP_ARGS(online, target),

	TP_STRUCT__entry(
		__field(	int,	online	)
		__field(	int,	target	)
	),

	TP_fast_assign(
		__entry->online = online;
		__entry->target = target;
	),

	TP_printk("online=%d target=%d", __entry->online, __entry->target)
);

TRACE_EVENT(tempfreq_hotplug_nr_running,

	TP_PROTO(int nr_running),

	TP_ARGS(nr_running),

	TP_STRUCT__entry(
		__field(	int,	nr_running	)
	),

	TP_fast_assign(
		__entry->nr_running = nr_running;
	),

	TP_printk("nr_running=%d", __entry->nr_running)
);

#ifndef __TEMPFREQ_HOTPLUG_STATE__
#define __TEMPFREQ_HOTPLUG_STATE__
const char *HOTPLUG_STATE_STR[] = {
	"HOTPLUG_UNKNOWN_NEXT",
	"HOTPLUG_INCREASE_NEXT",
	"HOTPLUG_DECREASE_NEXT",
};
#endif
TRACE_EVENT(tempfreq_hotplug_state,

	TP_PROTO(int elapsed_epochs, int next_state, int expected_next_state),

	TP_ARGS(elapsed_epochs, next_state, expected_next_state),

	TP_STRUCT__entry(
		__field(	int,	elapsed_epochs		)
		__field(	int,	next_state		)
		__field(	int,	expected_next_state	)
	),

	TP_fast_assign(
		__entry->elapsed_epochs = elapsed_epochs;
		__entry->next_state = next_state;
		__entry->expected_next_state = expected_next_state;
	),

	TP_printk("elapsed_epochs=%d next_state=%s expected_next_state=%s",
		__entry->elapsed_epochs,
		HOTPLUG_STATE_STR[__entry->next_state],
		HOTPLUG_STATE_STR[__entry->expected_next_state]
	)
);

#endif

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
