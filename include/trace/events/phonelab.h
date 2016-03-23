#undef TRACE_SYSTEM
#define TRACE_SYSTEM phonelab

#if !defined(_TRACE_PHONELAB_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PHONELAB_H

#include <linux/sched.h>
#include <linux/tracepoint.h>
#include <linux/binfmts.h>
#include <linux/phonelab.h>

DECLARE_EVENT_CLASS(phonelab_foreground_switch,

	TP_PROTO(struct task_struct *prev, struct task_struct *next, u32 cpu),

	TP_ARGS(prev, next, cpu),

	TP_STRUCT__entry(
		__field( pid_t,	prev_pid			)
		__array( char,	prev_comm,	TASK_COMM_LEN	)
		__field( pid_t,	next_pid			)
		__array( char,	next_comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
		__entry->prev_pid	= prev->pid;
		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
		__entry->next_pid	= next->pid;
	),

	TP_printk("prev=%d prev_comm=%s next=%d next_comm=%s",
		__entry->prev_pid, __entry->prev_comm,
		__entry->next_pid, __entry->next_comm)
);

DEFINE_EVENT(phonelab_foreground_switch, phonelab_foreground_switch_in,

	TP_PROTO(struct task_struct *prev, struct task_struct *next, u32 cpu),

	TP_ARGS(prev, next, cpu)
);

DEFINE_EVENT(phonelab_foreground_switch, phonelab_foreground_switch_out,

	TP_PROTO(struct task_struct *prev, struct task_struct *next, u32 cpu),

	TP_ARGS(prev, next, cpu)
);

TRACE_EVENT(phonelab_periodic_ctx_switch_marker,

	TP_PROTO(int cpu, int begin),

	TP_ARGS(cpu, begin),

	TP_STRUCT__entry(
		__field( int,	cpu				)
		__field( int,	begin				)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->begin = begin;
	),

	TP_printk("===================== CPU-%d %s =====================",
		__entry->cpu,
		__entry->begin == 1 ?
			"BEGIN: PERIODIC_CTX_SWITCH_INFO" :
			"END: PERIODIC_CTX_SWITCH_INFO  ")
);

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
TRACE_EVENT(phonelab_periodic_ctx_switch_info,

	TP_PROTO(struct task_struct *task, u32 cpu),

	TP_ARGS(task, cpu),

	TP_STRUCT__entry(
		__field( int,	cpu				)
		__field( pid_t,	pid				)
		__field( pid_t,	tgid				)
		__array( char,	comm,	TASK_COMM_LEN		)
		__field( unsigned long,		utime_t		)
		__field( unsigned long,		stime_t		)
		__field( unsigned long,		utime		)
		__field( unsigned long,		stime		)
		__field( unsigned long,		cutime_t	)
		__field( unsigned long,		cstime_t	)
		__field( unsigned long,		cutime		)
		__field( unsigned long,		cstime		)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;

		__entry->pid	= task->pid;
		__entry->tgid	= task->tgid;
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);

		task_times(task, &__entry->utime_t, &__entry->stime_t);
		__entry->utime	= cputime_to_clock_t(task->utime);
		__entry->stime	= cputime_to_clock_t(task->stime);

		__entry->cutime_t	= task->signal->cutime;
		__entry->cstime_t	= task->signal->cstime;
		__entry->cutime	= cputime_to_clock_t(task->signal->cutime);
		__entry->cstime	= cputime_to_clock_t(task->signal->cstime);
	),

	TP_printk("cpu=%d pid=%d tgid=%d comm=%s utime_t=%lu stime_t=%lu utime=%lu stime=%lu "
		"cutime_t=%lu cstime_t=%lu cutime=%lu cstime=%lu",
		__entry->cpu,
		__entry->pid, __entry->tgid, __entry->comm,
		__entry->utime_t, __entry->stime_t,
		__entry->utime, __entry->stime,
		__entry->cutime_t, __entry->cstime_t,
		__entry->cutime, __entry->cstime)
);
#endif

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_HASH
TRACE_EVENT(phonelab_periodic_ctx_switch_info,

	TP_PROTO(struct periodic_task_stats *stats, u32 cpu),

	TP_ARGS(stats, cpu),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__array(char, comm, TASK_COMM_LEN)
		__field(int, nice)
		__field(unsigned long long, utime)
		__field(unsigned long long, stime)
		__field(unsigned long long, runtime)
		__field(unsigned long long, bg_utime)
		__field(unsigned long long, bg_stime)
		__field(unsigned long long, bg_runtime)
		__field(int, status_running)
		__field(int, status_interruptible)
		__field(int, status_uninterruptible)
		__field(int, status_other)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
		__entry->pid	= stats->pid;
		__entry->tgid	= stats->tgid;
		__entry->nice	= stats->nice;
		memcpy(__entry->comm, stats->comm, TASK_COMM_LEN);

		__entry->utime	= (unsigned long long)stats->agg_time.utime;
		__entry->stime	= (unsigned long long)(stats->agg_time.stime);
		__entry->runtime	= stats->agg_time.sum_exec_runtime;

		__entry->utime	= (unsigned long long)stats->agg_bg_time.utime;
		__entry->stime	= (unsigned long long)stats->agg_bg_time.stime;
		__entry->runtime	= stats->agg_bg_time.sum_exec_runtime;

		__entry->status_running = stats->dequeue_reasons[0];
		__entry->status_interruptible = stats->dequeue_reasons[1];
		__entry->status_uninterruptible = stats->dequeue_reasons[2];
		__entry->status_other = stats->dequeue_reasons[3];
	),

	TP_printk("cpu=%d pid=%d tgid=%d nice=%d comm=%s utime=%llu stime=%llu rtime=%llu bg_utime=%llu "
		"bg_stime=%llu bg_rtime=%llu running=%d interruptible=%d uninterruptible=%d other=%d",
		__entry->cpu,
		__entry->pid, __entry->tgid, __entry->nice, __entry->comm,
		__entry->utime, __entry->stime, __entry->runtime,
		__entry->bg_utime, __entry->bg_stime, __entry->bg_runtime,
		__entry->status_running, __entry->status_interruptible,
		__entry->status_uninterruptible, __entry->status_other)
);
#endif

TRACE_EVENT(phonelab_proc_foreground,

	TP_PROTO(struct task_struct *task),

	TP_ARGS(task),

	TP_STRUCT__entry(
		__field( struct task_struct *,	task	)
	),

	TP_fast_assign(
		__entry->task = task;
	),

	TP_printk("pid=%d tgid=%d comm=%s",
		__entry->task->pid, __entry->task->tgid,
		__entry->task->comm)
);

TRACE_EVENT(phonelab_periodic_lim_exceeded,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field( int,	cpu	)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("cpu=%d exceeded!", __entry->cpu)
);

TRACE_EVENT(phonelab_num_online_cpus,

	TP_PROTO(int ncpus),

	TP_ARGS(ncpus),

	TP_STRUCT__entry(
		__field(	int,	ncpus		)
	),

	TP_fast_assign(
		__entry->ncpus		= ncpus;
	),

	TP_printk("num_online_cpus=%d", __entry->ncpus)
);

TRACE_EVENT(phonelab_periodic_warning_cpu,

	TP_PROTO(char *message, int cpu),

	TP_ARGS(message, cpu),

	TP_STRUCT__entry(
		__string(	message,	message	)
		__field(	int,		cpu	)
	),

	TP_fast_assign(
		__assign_str(message, message);
		__entry->cpu		= cpu;
	),

	TP_printk("warning=%s cpu=%d", __get_str(message), __entry->cpu)
);

TRACE_EVENT(phonelab_timing,

	TP_PROTO(const char *func, int cpu, u64 time_ns),

	TP_ARGS(func, cpu, time_ns),

	TP_STRUCT__entry(
		__string(	func,	func	)
		__field(	int,		cpu	)
		__field(	u64,		time_ns	)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__entry->time_ns	= time_ns;
		__entry->cpu		= cpu;
	),

	TP_printk("func=%s time=%lluns cpu=%d", __get_str(func), __entry->time_ns, __entry->cpu)
);

TRACE_EVENT(phonelab_info,

	TP_PROTO(const char *func, int cpu, const char *msg),

	TP_ARGS(func, cpu, msg),

	TP_STRUCT__entry(
		__string(	func,	func	)
		__field(	int,		cpu	)
		__string(	msg,	msg	)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__entry->cpu		= cpu;
		__assign_str(msg, msg);
	),

	TP_printk("func=%s cpu=%d msg=%s", __get_str(func), __entry->cpu, __get_str(msg))
);

TRACE_EVENT(phonelab_instruction_count,

	TP_PROTO(int cpu, u32 count),

	TP_ARGS(cpu, count),

	TP_STRUCT__entry(
		__field(	int,		cpu	)
		__field(	u32,		count	)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->count		= count;
	),

	TP_printk("cpu=%d instructions=%u", __entry->cpu, __entry->count)
);

#endif	/* _TRACE_PHONELAB_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
