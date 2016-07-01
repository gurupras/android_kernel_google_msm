#ifndef __PHONELAB__H_
#define __PHONELAB__H_

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/pid.h>

#define PHONELAB_MAGIC "<PhoneLab>"

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
#define CTX_SWITCH_INFO_LIM	4096
DECLARE_PER_CPU(struct task_struct *[CTX_SWITCH_INFO_LIM], ctx_switch_info);
DECLARE_PER_CPU(int, ctx_switch_info_idx);
#endif

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING
DECLARE_PER_CPU(atomic_t, test_field);
extern int periodic_ctx_switch_info_ready;
extern unsigned periodic_ctx_switch_info_freq;
#endif

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_HASH
// Per-cpu stats
struct periodic_task_stats {
	// From task_struct
	pid_t pid;
	pid_t tgid;
	int nice;
	char comm[TASK_COMM_LEN];

	// Hash list
	struct hlist_node hlist;

	// Previous values for caluclating aggregates
	struct task_cputime prev_time;

	// Should the current time count as BG or not?
	int count_as_bg;

	// Aggregate run times
	struct task_cputime agg_time;

	// Aggregate run times as bg task
	struct task_cputime agg_bg_time;

	// Why the process is being context switched out
	uint32_t dequeue_reasons[4];

	// Network
	u64 rx_bytes, tx_bytes;
};

void periodic_ctx_switch_info(struct work_struct *w);
void periodic_ctx_switch_update(struct task_struct *prev, struct task_struct *next);
#endif	/* CONFIG_PERIODIC_CTX_SWITCH_TRACING_HASH */

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

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING
void phonelab_update_task_net_stats(struct pid *pid, u32 rx, u32 tx);
#endif

// Enum for phonelab PM tracing
enum {
	PHONELAB_PM_SUSPEND_ENTRY,
	PHONELAB_PM_SUSPEND_EXIT,
};

#ifdef CONFIG_PHONELAB_TEMPFREQ_BINARY_MODE
extern int phonelab_tempfreq_binary_threshold_temp;
extern int phonelab_tempfreq_binary_critical;
extern int phonelab_tempfreq_binary_lower_threshold;
extern int phonelab_tempfreq_binary_short_epochs;
extern int phonelab_tempfreq_binary_short_diff_limit;
extern int phonelab_tempfreq_binary_long_epochs;
extern int phonelab_tempfreq_binary_long_diff_limit;
extern int phonelab_tempfreq_binary_jump_lower;
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_CGROUP_THROTTLING
#define CGROUP_MAP_MAX		10
struct cgroup_entry {
	int cur_idx;
	struct cgroup *cgroup;
	int throttling_temp;
	int unthrottling_temp;
	u64 cpu_shares;
	u64 throttle_time;
	int state;
};

struct cgroup_map {
	int cur_idx;
	struct cgroup_entry entries[CGROUP_MAP_MAX];
};
int tempfreq_update_cgroup_map(struct cgroup *cgrp, int throttling_temp, int unthrottling_temp);
#endif	/* CONFIG_PHONELAB_TEMPFREQ_THERMAL_CGROUP_THROTTLING */

#ifdef CONFIG_PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER
enum {
	HOTPLUG_UNKNOWN_NEXT = 0,
	HOTPLUG_INCREASE_NEXT,
	HOTPLUG_DECREASE_NEXT
};

struct hotplug_state {
	int elapsed_epochs;
	int next_state;
};

#endif	/* CONFIG_PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER */
#endif	/* __PHONELAB__H_ */
