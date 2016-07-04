#undef TRACE_SYSTEM
#define TRACE_SYSTEM tempd

#if !defined(_TRACE_TEMPD_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEMPD_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(tempd_p25,

	TP_PROTO(const char *name, int p25),

	TP_ARGS(name, p25),

	TP_STRUCT__entry(
		__array(	char,	name,	8	)
		__field(	int,	p25		)
	),

	TP_fast_assign(
		memcpy(__entry->name, name, 8);
		__entry->p25 = p25;
	),

	TP_printk("list=%s p25=%d", __entry->name, __entry->p25)
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
