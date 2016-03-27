#undef TRACE_SYSTEM
#define TRACE_SYSTEM ondemand

#if !defined(_TRACE_ONDEMAND_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ONDEMAND_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(ondemand_sysfs,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new),

	TP_STRUCT__entry(
		__field(	u32,		old		)
		__field(	u32,		new		)
	),

	TP_fast_assign(
		__entry->old = old;
		__entry->new = new;
	),

	TP_printk("old=%u new=%u", __entry->old, __entry->new)
);

DEFINE_EVENT(ondemand_sysfs, sampling_rate,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)

);

DEFINE_EVENT(ondemand_sysfs, sync_freq,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

DEFINE_EVENT(ondemand_sysfs, io_is_busy,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

DEFINE_EVENT(ondemand_sysfs, optimal_freq,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

DEFINE_EVENT(ondemand_sysfs, up_threshold,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

DEFINE_EVENT(ondemand_sysfs, up_threshold_multi_core,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

DEFINE_EVENT(ondemand_sysfs, up_threshold_any_cpu_load,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

DEFINE_EVENT(ondemand_sysfs, down_differential,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

DEFINE_EVENT(ondemand_sysfs, sampling_down_factor,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

DEFINE_EVENT(ondemand_sysfs, ignore_nice_load,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

DEFINE_EVENT(ondemand_sysfs, powersave_bias,

	TP_PROTO(unsigned int old, unsigned int new),

	TP_ARGS(old, new)
);

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
