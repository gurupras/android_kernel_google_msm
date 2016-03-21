#ifndef __PHONELAB__H_
#define __PHONELAB__H_

#include <linux/types.h>

#define PHONELAB_MAGIC "<PhoneLab>"

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING
#define CTX_SWITCH_INFO_LIM	4096
DECLARE_PER_CPU(struct task_struct *[CTX_SWITCH_INFO_LIM], ctx_switch_info);
DECLARE_PER_CPU(int, ctx_switch_info_idx);
DECLARE_PER_CPU(atomic_t, test_field);

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

int armv7_pmnc_select_counter(int idx);
u32 armv7pmu_read_counter(int idx);
void armv7pmu_write_counter(int idx, u32 value);
void armv7_pmnc_write_evtsel(int idx, u32 val);
int armv7_pmnc_enable_counter(int idx);
int armv7_pmnc_disable_counter(int idx);
u32 __init armv7_read_num_pmnc_events(void);
void armv7_pmnc_write(u32 val);
u32 armv7_pmnc_read(void);

#endif	/* __PHONELAB__H_ */
