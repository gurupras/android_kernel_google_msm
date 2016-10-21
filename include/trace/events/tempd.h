#undef TRACE_SYSTEM
#define TRACE_SYSTEM tempd

#if !defined(_TRACE_TEMPD_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEMPD_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(tempd_pn,

	TP_PROTO(const char *name, int n, int pn),

	TP_ARGS(name, n, pn),

	TP_STRUCT__entry(
		__array(	char,	name,	8	)
		__field(	int,	n		)
		__field(	int,	pn		)
	),

	TP_fast_assign(
		memcpy(__entry->name, name, 8);
		__entry->n = n;
		__entry->pn = pn;
	),

	TP_printk("list=%s percentile_n=%d percentile=%d", __entry->name, __entry->n,  __entry->pn)
);

TRACE_EVENT(tempd_timing,

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


#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
