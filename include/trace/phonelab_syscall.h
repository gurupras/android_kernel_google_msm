
#undef TRACE_SYSTEM
#define TRACE_SYSTEM phonelab_syscall_tracing

// Name of _this_ file (.h appended automatically):
// Q -- should be undefined??
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE phonelab_syscall

#if !defined(_SYSCALL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _PHONELAB_SYSCALL_H_

#include <linux/tracepoint.h>

#define PHONELAB_LOG_SYSCALLS
#define PLSC_PATHMAX 256


// N.b., array entry references in struct definition (TP_STRUCT__entry) are of type [entry], not [entry*]
// For pid = task_tgid_vnr(current);
// For tid = task_pid_vnr(current);
// For struct kstat:  loff_t size, umode_t mode
// For task name:  strictly, should use get_task_comm() for atomic results; still safe w/o though -- buffer guaranteed to always have NULLTERM



TRACE_EVENT(plsc_ioprio,
	TP_PROTO(char* syscall, int which, int who, int ioprio),
	TP_ARGS(syscall, which, who, ioprio),
	TP_STRUCT__entry(
		__string(action, syscall)
		__field(int, which)
		__field(int, who)
		__field(int, ioprio)
		),
	TP_fast_assign(
		__assign_str(action, syscall);
		__entry->which = which;
		__entry->who = who;
		__entry->ioprio = ioprio;
		),
	TP_printk("{\"action\":\"%s\", \"which\":%i, \"who\":%i, \"ioprio\":%i}", __get_str(action), __entry->which, __entry->who, __entry->ioprio)
);



TRACE_EVENT(plsc_open,
	TP_PROTO(char* syscall, unsigned long long start, unsigned long long delta, char* tmp, int fd, int session, struct kstat* stat_struct_ptr, int flags, umode_t mode),
	TP_ARGS(syscall, start, delta, tmp, fd, session, stat_struct_ptr, flags, mode),
	TP_STRUCT__entry(
		__string(action, syscall)
		__field(unsigned long long, start)
		__field(unsigned long long, delta)
		__field(long, uid)
		__field(long, pid)
		__array(char, comm, TASK_COMM_LEN)
		__array(char, pathname, PLSC_PATHMAX)
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
		__entry->pid = current->tgid;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
		memcpy(__entry->pathname, tmp, PLSC_PATHMAX);
		__entry->retval = fd;
		__entry->session = session;
		__entry->size = stat_struct_ptr->size;
		__entry->type = stat_struct_ptr->mode & S_IFMT;
		__entry->flags = flags;
		__entry->mode = mode;
		),
	TP_printk("{\"action\":\"%s\", \"start\":%llu, \"delta\":%llu, \"uid\":%lu, \"pid\":%lu, \"task\":\"%s\", \"path\":\"%s\", \"retval\":%i, \"session\":%i, \"size\":%lli, \"type\": %i, \"flags\":%i, \"mode\":%i}", __get_str(action), __entry->start, __entry->delta, __entry->uid, __entry->pid, __entry->comm, __entry->pathname, __entry->retval, __entry->session, __entry->size, __entry->type, __entry->flags, __entry->mode)
);



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



TRACE_EVENT(plsc_mmap,
	TP_PROTO(char* syscall, unsigned long long start, unsigned long long delta, unsigned long retval, int session, unsigned int fd, unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags, unsigned long pgoff),
	TP_ARGS(syscall, start, delta, retval, session, fd, addr, len, prot, flags, pgoff),
	TP_STRUCT__entry(
		__string(action, syscall)
		__field(unsigned long long, start)
		__field(unsigned long long, delta)
		__field(unsigned long, retval)
		__field(int, session)
		__field(unsigned int, fd)
		__field(unsigned long, addr)
		__field(unsigned long, len)
		__field(unsigned long, prot)
		__field(unsigned long, flags)
		__field(unsigned long, pgoff)
		),
	TP_fast_assign(
		__assign_str(action, syscall);
		__entry->start = start;
		__entry->delta = delta;
		__entry->retval = retval;
		__entry->session = session;
		__entry->fd = fd;
		__entry->addr = addr;
		__entry->len = len;
		__entry->prot = prot;
		__entry->flags = flags;
		__entry->pgoff = pgoff;
		),
//	TP_printk("{\"action\":\"%s\", \"start\":%llu, \"delta\":%llu, \"retval\":%lu, \"session\":%i, \"fd\":%i, \"addr\":%lu, \"len\":%lu, \"prot\":%lu, \"flags\":%lu, \"pgoff\":%lu}", __get_str(action), __entry->start, __entry->delta, __entry->retval, __entry->session, __entry->fd, __entry->addr, __entry->len, __entry->prot, __entry->flags, __entry->pgoff)
	TP_printk("{\"action\":\"%s\", \"start\":%llu, \"delta\":%llu, \"retval\":%i, \"session\":%i, \"fd\":%i, \"addr\":%lu, \"len\":%lu, \"prot\":%lu, \"flags\":%lu, \"pgoff\":%lu}", __get_str(action), __entry->start, __entry->delta, __entry->retval, __entry->session, __entry->fd, __entry->addr, __entry->len, __entry->prot, __entry->flags, __entry->pgoff)
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



TRACE_EVENT(plsc_sync,
	TP_PROTO(char* syscall, unsigned long long start, unsigned long long delta, ssize_t retval, int session, unsigned int fd),
	TP_ARGS(syscall, start, delta, retval, session, fd),
	TP_STRUCT__entry(
		__string(action, syscall)
		__field(unsigned long long, start)
		__field(unsigned long long, delta)
		__field(ssize_t, retval)
		__field(int, session)
		__field(unsigned int, fd)
		),
	TP_fast_assign(
		__assign_str(action, syscall);
		__entry->start = start;
		__entry->delta = delta;
		__entry->retval = retval;
		__entry->session = session;
		__entry->fd = fd;
		),
	TP_printk("{\"action\":\"%s\", \"start\":%llu, \"delta\":%llu, \"retval\":%i, \"session\":%i, \"fd\":%i}", __get_str(action), __entry->start, __entry->delta, __entry->retval, __entry->session, __entry->fd)
);



TRACE_EVENT(plsc_dup,
	TP_PROTO(char* syscall, unsigned int oldfd, unsigned int newfd, int flags),
	TP_ARGS(syscall, oldfd, newfd, flags),
	TP_STRUCT__entry(
		__string(action, syscall)
		__field(unsigned int, oldfd)
		__field(unsigned int, newfd)
		__field(int, flags)
		),
	TP_fast_assign(
		__assign_str(action, syscall);
		__entry->oldfd = oldfd;
		__entry->newfd = newfd;
		__entry->flags = flags;
		),
	TP_printk("{\"action\":\"%s\", \"oldfd\":%i, \"newfd\":%i, \"flags\":%i}", __get_str(action), __entry->oldfd, __entry->newfd, __entry->flags)
);



TRACE_EVENT(plsc_delete,
	TP_PROTO(char* syscall, unsigned long long start, unsigned long long delta, char* name, int error, struct kstat* stat_struct_ptr),
	TP_ARGS(syscall, start, delta, name, error, stat_struct_ptr),
	TP_STRUCT__entry(
		__string(action, syscall)
		__field(unsigned long long, start)
		__field(unsigned long long, delta)
		__field(long, uid)
		__field(long, pid)
		__array(char, comm, TASK_COMM_LEN)
		__array(char, pathname, PLSC_PATHMAX)
		__field(int, retval)
		__field(loff_t, size)
		__field(umode_t, type)
		),
	TP_fast_assign(
		__assign_str(action, syscall);
		__entry->start = start;
		__entry->delta = delta;
		__entry->uid = current_uid();
		__entry->pid = task_tgid_vnr(current);
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
		memcpy(__entry->pathname, name, PLSC_PATHMAX);
		__entry->retval = error;
		__entry->size = stat_struct_ptr->size;
		__entry->type = stat_struct_ptr->mode & S_IFMT;
		),
	TP_printk("{\"action\":\"%s\", \"start\":%llu, \"delta\":%llu, \"uid\":%lu, \"pid\":%lu, \"task\":\"%s\", \"path\":\"%s\", \"retval\":%i, \"size\":%lli, \"type\": %i}", __get_str(action), __entry->start, __entry->delta, __entry->uid, __entry->pid, __entry->comm, __entry->pathname, __entry->retval, __entry->size, __entry->type)
);



#endif /* _PHONELAB_SYSCALL_H  */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>


