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

TRACE_EVENT(tempfreq_skin_temp,

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

TRACE_EVENT(tempfreq_thermal_cgroup_throttling,

	TP_PROTO(int temp, int idx, int state, int reason, int nth_percentile, u64 throttled_time),

	TP_ARGS(temp, idx, state, reason, nth_percentile, throttled_time),

	TP_STRUCT__entry(
		__field(	int,		temp		)
		__field(	int,		idx		)
		__field(	int,		state		)
		__field(	int,		reason		)
		__field(	int,		nth_percentile	)
		__field(	u64,		throttled_time	)
	),

	TP_fast_assign(
		__entry->temp		= temp;
		__entry->idx		= idx;
		__entry->state		= state;
		__entry->reason		= reason;
		__entry->nth_percentile	= nth_percentile;
		__entry->throttled_time	= throttled_time;
	),

	TP_printk("temp=%d idx=%d state=%s reason=%s nth_percentile=%d throttled_time=%llu",
			__entry->temp,
			__entry->idx,
			__entry->state == 0 ? "NORMAL" : "THROTTLED",
			__entry->reason == 0 ? "temp" : "timeout",
			__entry->nth_percentile,
			__entry->throttled_time
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


TRACE_EVENT(tempfreq_hotplug,

	TP_PROTO(int up_set, int down_set, int overall_up),

	TP_ARGS(up_set, down_set, overall_up),

	TP_STRUCT__entry(
		__array(	char,	up_buf,		10	)
		__array(	char,	down_buf,	10	)
		__array(	char,	overall_up_buf,	10	)
	),

	TP_fast_assign(
		__set_to_string(up_set, __entry->up_buf);
		__set_to_string(down_set, __entry->down_buf);
		__set_to_string(overall_up, __entry->overall_up_buf);
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

#ifdef CONFIG_PHONELAB_TEMPFREQ_HOTPLUG_DRIVER
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

TRACE_EVENT(tempfreq_hotplug_autosmp_rates,

	TP_PROTO(int max, int up, int down, int slow, int fast),

	TP_ARGS(max, up, down, slow, fast),

	TP_STRUCT__entry(
		__field(	int,		max	)
		__field(	int,		up	)
		__field(	int,		down	)
		__field(	int,		slow	)
		__field(	int,		fast	)
	),

	TP_fast_assign(
		__entry->max		= max;
		__entry->up		= up;
		__entry->down		= down;
		__entry->slow		= slow;
		__entry->fast		= fast;
	),

	TP_printk("max=%d up=%d down=%d slow=%d fast=%d",
		__entry->max,
		__entry->up,
		__entry->down,
		__entry->slow,
		__entry->fast
	)
);

TRACE_EVENT(tempfreq_mpdecision_blocked,

	TP_PROTO(int is_blocked),

	TP_ARGS(is_blocked),

	TP_STRUCT__entry(
		__field(	int,	is_blocked	)
	),

	TP_fast_assign(
		__entry->is_blocked = is_blocked
	),

	TP_printk("mpdecision_state=%s", __entry->is_blocked ? "blocked" : "unblocked")
);

TRACE_EVENT(tempfreq_cgroup_copy_tasks,

	TP_PROTO(int n, int failed),

	TP_ARGS(n, failed),

	TP_STRUCT__entry(
		__field(	int,	n	)
		__field(	int,	failed	)
	),

	TP_fast_assign(
		__entry->n = n;
		__entry->failed = failed;
	),

	TP_printk("n=%d failed=%d", __entry->n, __entry->failed)
);

TRACE_EVENT(tempfreq_log,

	TP_PROTO(char *message),

	TP_ARGS(message),

	TP_STRUCT__entry(
		__string(	message,	message	)
	),

	TP_fast_assign(
		__assign_str(message, message);
	),

	TP_printk("msg=%s", __get_str(message))
);

#endif
/* This part must be outside protection */
#include <trace/define_trace.h>
