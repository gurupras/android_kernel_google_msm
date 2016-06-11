#undef TRACE_SYSTEM
#define TRACE_SYSTEM tempfreq

#if !defined(_TRACE_TEMPFREQ_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEMPFREQ_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

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

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
