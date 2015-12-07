
#undef TRACE_SYSTEM
#define TRACE_SYSTEM phonelab_syscall_tracing

// Name of _this_ file (.h appended automatically):
// Q -- should be undefined??
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE phonelab_syscall

#if !defined(_SYSCALL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _PHONELAB_SYSCALL_H_

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



// N.b., array entry references in struct definition (TP_STRUCT__entry) are of type [entry], not [entry*]

#define PLSC_OPEN_PATHMAX 256
TRACE_EVENT(plsc_open,
	TP_PROTO(long long unsigned time_gm, long long unsigned time_sc, char* tmp, int fd, struct kstat* stat_struct_ptr, int flags, umode_t mode, int error, int session),
	TP_ARGS(time_gm, time_sc, tmp, fd, stat_struct_ptr, flags, mode, error, session),
	TP_STRUCT__entry(
		__field(long long unsigned, time_gm)
		__field(long long unsigned, time_sc)
		__field(long, uid)
		__field(long, pid)
		__array(char, pathname, PLSC_OPEN_PATHMAX)
		__field(int, fd)
		__field(loff_t, size)
		__field(umode_t, type)
		__field(int, flags)
		__field(umode_t, mode)
		__field(int, error)
		__field(int, session)
		),
	TP_fast_assign(
		__entry->time_gm = time_gm;
		__entry->time_sc = time_sc;
		__entry->uid = current_uid();
		__entry->pid = task_tgid_vnr(current);
		memcpy(__entry->pathname, tmp, PLSC_OPEN_PATHMAX);
		__entry->fd = fd;
		__entry->size = stat_struct_ptr->size;
		__entry->type = stat_struct_ptr->mode & S_IFMT;
		__entry->flags = flags;
		__entry->mode = mode;
		__entry->error = error;
		__entry->session = session;
		),
	TP_printk("{\"time_gm\":%llu, \"time_sc\":%llu, \"uid\":%lu, \"pid\":%lu, \"path\":\"%s\", \"fd\":%i, \"size\":%lli, \"type\": %i, \"flags\":%i, \"mode\":%i, \"ERR\":%i, \"session\":%i}", __entry->time_gm, __entry->time_sc, __entry->uid, __entry->pid, __entry->pathname, __entry->fd, __entry->size, __entry->type, __entry->flags, __entry->mode, __entry->error, __entry->session)
);
// For TID:  "__field(long, pid)" -- "__entry->tid = task_pid_vnr(current);"
// For struct kstat:  loff_t size, umode_t mode



TRACE_EVENT(plsc_read,
	TP_PROTO(long long unsigned time_gm, long long unsigned time_sc, ssize_t ret, int session, unsigned int fd, size_t count, loff_t pos_old),
	TP_ARGS(time_gm, time_sc, ret, session, fd, count, pos_old),
	TP_STRUCT__entry(
		__field(long long unsigned, time_gm)
		__field(long long unsigned, time_sc)
		__field(ssize_t, error)
		__field(int, session)
		__field(unsigned int, fd)
		__field(size_t, bytes)
		__field(loff_t, offset)
		),
	TP_fast_assign(
		__entry->time_gm = time_gm;
		__entry->time_sc = time_sc;
		__entry->error = ret;
		__entry->session = session;
		__entry->fd = fd;
		__entry->bytes = count;
		__entry->offset = pos_old;
		),
	TP_printk("{\"time_gm\":%llu, \"time_sc\":%llu, \"error\":%i, \"session\":%i, \"fd\":%i, \"bytes\":%i, \"offset\":%llu}", __entry->time_gm, __entry->time_sc, __entry->error, __entry->session, __entry->fd, __entry->bytes, __entry->offset)
);



#endif /* _PHONELAB_SYSCALL_H  */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>


