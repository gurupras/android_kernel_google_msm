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
		__field( struct task_struct *,	task	)
		__field( int,	cpu			)
	),

	TP_fast_assign(
		__entry->task = task;
		__entry->cpu  = cpu;
	),

	TP_printk("cpu=%d pid=%d tgid=%d cutime=%lu cstime=%lu",
		__entry->cpu,
		__entry->task->pid, __entry->task->tgid,
		__entry->task->utime, __entry->task->stime)
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

#endif	/* _TRACE_PHONELAB_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
