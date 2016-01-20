#ifndef __PHONELAB__H_
#define __PHONELAB__H_

#include <linux/types.h>

#define PHONELAB_MAGIC "<PhoneLab>"

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING
#define CTX_SWITCH_INFO_LIM	4096
DECLARE_PER_CPU(struct task_struct *[CTX_SWITCH_INFO_LIM], ctx_switch_info);
DECLARE_PER_CPU(int, ctx_switch_info_idx);
DECLARE_PER_CPU(int, test_field);

extern int periodic_ctx_switch_info_ready;

void periodic_ctx_switch_info(struct work_struct *w);

#endif

/* This whole enum is borrowed from system/core/include/android/log.h */
typedef enum {
	ANDROID_LOG_UNKNOWN = 0,
	ANDROID_LOG_DEFAULT,    /* only for SetMinPriority() */
	ANDROID_LOG_VERBOSE,
	ANDROID_LOG_DEBUG,
	ANDROID_LOG_INFO,
	ANDROID_LOG_WARN,
	ANDROID_LOG_ERROR,
	ANDROID_LOG_FATAL,
	ANDROID_LOG_SILENT,     /* only for SetMinPriority(); must be last */
} LOGCAT_LEVEL;

void alog_v(char *tag, const char *fmt, ...);
void alog_d(char *tag, const char *fmt, ...);
void alog_i(char *tag, const char *fmt, ...);
void alog_w(char *tag, const char *fmt, ...);
void alog_e(char *tag, const char *fmt, ...);

#endif	/* __PHONELAB__H_ */
