#undef TRACE_SYSTEM
#define TRACE_SYSTEM phonelab

#if !defined(_TRACE_PHONELAB_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PHONELAB_H

#include <linux/sched.h>
#include <linux/tracepoint.h>
#include <linux/binfmts.h>

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

	TP_printk("cpu=%d pid=%d tgid=%d comm=%s utime_t=%lu stime_t=%lu utime=%lu stime=%lu"
		"cutime_t=%lu cstime_t=%lu cutime=%lu cstime=%lu",
		__entry->cpu,
		__entry->pid, __entry->tgid, __entry->comm,
		__entry->utime_t, __entry->stime_t,
		__entry->utime, __entry->stime,
		__entry->cutime_t, __entry->cstime_t,
		__entry->cutime, __entry->cstime)
);

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

#endif	/* _TRACE_PHONELAB_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
