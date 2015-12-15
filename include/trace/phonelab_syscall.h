
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

#define PHONELAB_LOG_SYSCALLS

#define PLSC_OPEN_PATHMAX 256
TRACE_EVENT(plsc_open,
	TP_PROTO(char* syscall, unsigned long long start, unsigned long long delta, char* tmp, int fd, int session, struct kstat* stat_struct_ptr, int flags, umode_t mode),
	TP_ARGS(syscall, start, delta, tmp, fd, session, stat_struct_ptr, flags, mode),
	TP_STRUCT__entry(
		__string(action, syscall)
		__field(unsigned long long, start)
		__field(unsigned long long, delta)
		__field(long, uid)
		__field(long, pid)
		__array(char, pathname, PLSC_OPEN_PATHMAX)
		__field(int, retval)
		__field(int, session)
		__field(loff_t, size)
		__field(umode_t, type)
		__field(int, flags)
		__field(umode_t, mode)
		),
	TP_fast_assign(
		__assign_str(action, syscall);
		__entry->start = start;
		__entry->delta = delta;
		__entry->uid = current_uid();
		__entry->pid = task_tgid_vnr(current);
		memcpy(__entry->pathname, tmp, PLSC_OPEN_PATHMAX);
		__entry->retval = fd;
		__entry->session = session;
		__entry->size = stat_struct_ptr->size;
		__entry->type = stat_struct_ptr->mode & S_IFMT;
		__entry->flags = flags;
		__entry->mode = mode;
		),
	TP_printk("{\"action\":\"%s\", \"start\":%llu, \"delta\":%llu, \"uid\":%lu, \"pid\":%lu, \"path\":\"%s\", \"retval\":%i, \"session\":%i, \"size\":%lli, \"type\": %i, \"flags\":%i, \"mode\":%i}", __get_str(action), __entry->start, __entry->delta, __entry->uid, __entry->pid, __entry->pathname, __entry->retval, __entry->session, __entry->size, __entry->type, __entry->flags, __entry->mode)
);
// For TID:  "__field(long, pid)" -- "__entry->tid = task_pid_vnr(current);"
// For struct kstat:  loff_t size, umode_t mode



TRACE_EVENT(plsc_rw,
	TP_PROTO(char* syscall, unsigned long long start, unsigned long long delta, ssize_t retval, int session, unsigned int fd, size_t count, loff_t pos_old),
	TP_ARGS(syscall, start, delta, retval, session, fd, count, pos_old),
	TP_STRUCT__entry(
		__string(action, syscall)
		__field(unsigned long long, start)
		__field(unsigned long long, delta)
		__field(ssize_t, retval)
		__field(int, session)
		__field(unsigned int, fd)
		__field(size_t, bytes)
		__field(loff_t, offset)
		),
	TP_fast_assign(
		__assign_str(action, syscall);
		__entry->start = start;
		__entry->delta = delta;
		__entry->retval = retval;
		__entry->session = session;
		__entry->fd = fd;
		__entry->bytes = count;
		__entry->offset = pos_old;
		),
	TP_printk("{\"action\":\"%s\", \"start\":%llu, \"delta\":%llu, \"retval\":%i, \"session\":%i, \"fd\":%i, \"bytes\":%i, \"offset\":%llu}", __get_str(action), __entry->start, __entry->delta, __entry->retval, __entry->session, __entry->fd, __entry->bytes, __entry->offset)
);



TRACE_EVENT(plsc_close,
	TP_PROTO(unsigned long long start, unsigned long long delta, ssize_t retval, int session, unsigned int fd, struct kstat* stat_struct_ptr),
	TP_ARGS(start, delta, retval, session, fd, stat_struct_ptr),
	TP_STRUCT__entry(
		__field(unsigned long long, start)
		__field(unsigned long long, delta)
		__field(ssize_t, retval)
		__field(int, session)
		__field(unsigned int, fd)
		__field(loff_t, size)
		),
	TP_fast_assign(
		__entry->start = start;
		__entry->delta = delta;
		__entry->retval = retval;
		__entry->session = session;
		__entry->fd = fd;
		__entry->size = stat_struct_ptr->size;
		),
	TP_printk("{\"action\":\"close\", \"start\":%llu, \"delta\":%llu, \"retval\":%i, \"session\":%i, \"fd\":%i, \"size\":%llu}", __entry->start, __entry->delta, __entry->retval, __entry->session, __entry->fd, __entry->size)
);



TRACE_EVENT(plsc_lseek,
	TP_PROTO(char* syscall, unsigned long long start, unsigned long long delta, ssize_t retval, int session, unsigned int fd, loff_t offset, unsigned int origin),
	TP_ARGS(syscall, start, delta, retval, session, fd, offset, origin),
	TP_STRUCT__entry(
		__string(action, syscall)
		__field(unsigned long long, start)
		__field(unsigned long long, delta)
		__field(ssize_t, retval)
		__field(int, session)
		__field(unsigned int, fd)
		__field(loff_t, offset)
		__field(unsigned int, origin)
		),
	TP_fast_assign(
		__assign_str(action, syscall)
		__entry->start = start;
		__entry->delta = delta;
		__entry->retval = retval;
		__entry->session = session;
		__entry->fd = fd;
		__entry->offset = offset;
		__entry->origin = origin;
		),
	TP_printk("{\"action\":\"%s\", \"start\":%llu, \"delta\":%llu, \"retval\":%i, \"session\":%i, \"fd\":%i, \"offset\":%llu, \"origin\":%u}", __get_str(action), __entry->start, __entry->delta, __entry->retval, __entry->session, __entry->fd, __entry->offset, __entry->origin)
);



#endif /* _PHONELAB_SYSCALL_H  */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>


