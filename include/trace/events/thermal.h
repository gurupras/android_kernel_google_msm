#undef TRACE_SYSTEM
#define TRACE_SYSTEM thermal

#if !defined(_TRACE_THERMAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_THERMAL_H

#include <linux/tracepoint.h>

TRACE_EVENT(thermal_temp,

	TP_PROTO(unsigned int sensor_id, long temp),

	TP_ARGS(sensor_id, temp),

	TP_STRUCT__entry(
		__field(	unsigned int,	sensor_id	)
		__field(	long,		temp		)
	),

	TP_fast_assign(
		__entry->sensor_id	= sensor_id;
		__entry->temp		= temp;
	),

	TP_printk("sensor_id=%d temp=%ld", __entry->sensor_id, __entry->temp)
);

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
