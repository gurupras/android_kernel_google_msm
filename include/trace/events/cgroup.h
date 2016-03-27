/*
 * Trace subsystem for cgroup events.
 *
 * Written for Phonelab
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cgroup

#if !defined(_TRACE_CGROUP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CGROUP_H

#include <linux/sched.h>
#include <linux/tracepoint.h>
#include <linux/binfmts.h>
#include <linux/cgroup.h>

TRACE_EVENT(cgroup_task_migrate,

	TP_PROTO(struct task_struct *task, struct cgroup *oldcgrp,
		struct cgroup *cgrp),

	TP_ARGS(task, oldcgrp, cgrp),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__array(char, comm, TASK_COMM_LEN)
		__field(int, is_bg)
		__field(int, was_bg)
	),

	TP_fast_assign(
		__entry->pid	= task->pid;
		__entry->tgid	= task->tgid;
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
		__entry->is_bg = cgroup_is_bg_task(cgrp); 
		__entry->was_bg = cgroup_is_bg_task(oldcgrp); 
	),

	TP_printk("pid=%d tgid=%d comm=%s is_bg=%d was_bg=%d",
		__entry->pid, __entry->tgid, __entry->comm,
		__entry->is_bg, __entry->was_bg)
);

#endif	/* _TRACE_CGROUP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
