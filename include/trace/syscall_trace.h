
#undef TRACE_SYSTEM
#define TRACE_SYSTEM syscall_tracing

// Name of _this_ file (.h appended automatically):
// Q -- should be undefined??
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE syscall_trace

#if !defined(_SYSCALL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _SYSCALL_TRACE_H_

#include <linux/tracepoint.h>



TRACE_EVENT(syscall_foobar,
            TP_PROTO(int x, int y, int z),
            TP_ARGS(x, y, z),
            TP_STRUCT__entry(
                             __field(int, val1)
                             __field(int, val2)
                             __field(int, val3)
                             ),

            TP_fast_assign(
                           __entry->val1 = x;
                           __entry->val2 = y;
                           __entry->val3 = z;
                           ),
            TP_printk("val1 = %i, val2 = %i, val3 = %i", __entry->val1, __entry->val2, __entry->val3)
);



#endif /* _SYSCALL_TRACE_H  */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>


