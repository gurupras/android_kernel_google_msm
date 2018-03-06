#undef TRACE_SYSTEM
#define TRACE_SYSTEM thermaplan

#if !defined(_TRACE_THERMAPLAN_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_THERMAPLAN_H

#include <linux/sched.h>
#include <linux/tracepoint.h>
#include <linux/binfmts.h>

TRACE_EVENT(thermaplan_info,

	TP_PROTO(const char *func, int cpu, const char *msg),

	TP_ARGS(func, cpu, msg),

	TP_STRUCT__entry(
		__string(       func,	func    )
		__field(        int,	cpu     )
		__string(       msg,	msg     )
	),

	TP_fast_assign(
		__assign_str(func, func);
		__entry->cpu = cpu;
		__assign_str(msg, msg);
	),

	TP_printk("func=%s cpu=%d msg=%s", __get_str(func), __entry->cpu, __get_str(msg))
);

#endif /* _TRACE_THERMAPLAN_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
