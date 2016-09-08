#ifndef __TEMPFREQ_H_
#define __TEMPFREQ_H_

#include <linux/cgroup.h>

#define MIN_TEMPERATURE		0
#define MAX_TEMPERATURE		100
#define TEMP_FREQUENCY_MS	250

void phone_state_lock(void);
void phone_state_unlock(void);
void update_phone_state(int cpu, int enabled);

extern struct kobject tempfreq_kobj;

struct temp_list {
	u64 num_elements;
	u64 max_elements;
	int temperatures[MAX_TEMPERATURE - MIN_TEMPERATURE];
	struct heap *min_heap;
	struct heap *max_heap;
	struct list_head list;
};

extern struct temp_list *long_temp_list, *short_temp_list;

int get_nth_percentile(struct temp_list *tl, int n);

/* sysfs helpers */
struct tempfreq_attr {
	struct attribute attr;
	ssize_t (*show)(char *);
	ssize_t (*store)(const char *, size_t count);
};



#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_BG_THROTTLING
/* which pidlist file are we talking about? */
enum cgroup_filetype {
	CGROUP_FILE_PROCS,
	CGROUP_FILE_TASKS,
};

/*
 * A pidlist is a list of pids that virtually represents the contents of one
 * of the cgroup files ("procs" or "tasks"). We keep a list of such pidlists,
 * a pair (one each for procs, tasks) for each pid namespace that's relevant
 * to the cgroup.
 */
struct cgroup_pidlist {
	/*
	 * used to find which pidlist is wanted. doesn't change as long as
	 * this particular list stays in the list.
	*/
	struct { enum cgroup_filetype type; struct pid_namespace *ns; } key;
	/* array of xids */
	pid_t *list;
	/* how many elements the above list has */
	int length;
	/* how many files are using the current array */
	int use_count;
	/* each of these stored in a list by its cgroup */
	struct list_head links;
	/* pointer to the cgroup we belong to, for list removal purposes */
	struct cgroup *owner;
	/* protects the other fields */
	struct rw_semaphore mutex;
};
struct file;
int pidlist_array_load(struct cgroup *cgrp, enum cgroup_filetype type,
			      struct cgroup_pidlist **lp);
void cgroup_release_pid_array(struct cgroup_pidlist *l);
#endif	/* CONFIG_PHONELAB_TEMPFREQ_THERMAL_BG_THROTTLING */

#ifdef CONFIG_PHONELAB_TEMPFREQ_THERMAL_CGROUP_THROTTLING
int tempfreq_update_cgroup_map(struct cgroup *cgrp, int throttling_temp, int unthrottling_temp);
#endif

#ifdef CONFIG_PHONELAB_CPUFREQ_GOVERNOR_FIX
ssize_t show_ondemand_ignore_bg(char *buf);
ssize_t set_ondemand_ignore_bg(const char *buf, size_t count);
#endif	/* CONFIG_PHONELAB_CPUFREQ_GOVERNOR_FIX */

#define __show(name) __show1(name, name);

#define __show1(fname, vname)					\
static ssize_t show_##fname(char *buf)				\
{								\
	/* printk(KERN_DEBUG "tempfreq: %s: show() -> %d\n", __func__, phonelab_tempfreq_##vname);*/	\
	return sprintf(buf, "%d", phonelab_tempfreq_##vname);	\
}


#define tempfreq_attr_ro(_name)				\
__show(_name);						\
static struct tempfreq_attr _name =			\
__ATTR(_name, 0444, show_##_name, NULL)

#define tempfreq_attr_rw(_name)				\
__show(_name);						\
static struct tempfreq_attr _name =			\
__ATTR(_name, 0644, show_##_name, store_##_name)

#define tempfreq_attr_plain_rw(_name)			\
static struct tempfreq_attr _name =			\
__ATTR(_name, 0644, show_##_name, store_##_name)

#define tempfreq_attr_plain_ro(_name)			\
static struct tempfreq_attr _name =			\
__ATTR(_name, 0444, show_##_name, NULL)

#define export_tempfreq_attr_rw(_name)			\
__show(_name);						\
struct tempfreq_attr _name =				\
__ATTR(_name, 0644, show_##_name, store_##_name)

#define tf_to_attr(a) container_of(a, struct tempfreq_attr, attr)

int tempfreq_show(struct kobject *kobj, struct attribute *attr, char *buf);
ssize_t tempfreq_store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count);

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
int __init init_mpdecision_coexist(void);
extern int phonelab_tempfreq_mpdecision_coexist_enable;
extern int phonelab_tempfreq_mpdecision_blocked;
extern int phonelab_tempfreq_mpdecision_coexist_cpu;
void start_bg_core_control(void);
void stop_bg_core_control(void);
extern struct tempfreq_attr mpdecision_coexist_enable;
extern struct tempfreq_attr mpdecision_coexist_upcall;
extern struct tempfreq_attr mpdecision_coexist_nl_send;
extern struct tempfreq_attr mpdecision_bg_cpu;
#endif	/* CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST */

#ifdef CONFIG_PHONELAB_TEMPFREQ_CGROUP_CPUSET_BIND
extern struct mutex cgroup_mutex, cgroup_root_mutex;
int __attach_task_by_pid(struct cgroup *cgrp, u64 pid, bool threadgroup, bool check_cred);
int attach_task_by_pid(struct cgroup *cgrp, u64 pid, bool threadgroup);
int cgroup_tasks_write(struct cgroup *cgrp, struct cftype *cft, u64 pid);

/* cpuset hacks */
struct fmeter {
	int cnt;		/* unprocessed events count */
	int val;		/* most recent output value */
	time_t time;		/* clock (secs) when val computed */
	spinlock_t lock;	/* guards read or write of above */
};

struct cpuset {
	char name[32];
	struct cgroup_subsys_state css;

	unsigned long flags;		/* "unsigned long" so bitops work */
	cpumask_var_t cpus_allowed;	/* CPUs allowed to tasks in cpuset */
	nodemask_t mems_allowed;	/* Memory Nodes allowed to tasks */

	struct cpuset *parent;		/* my parent */

	struct fmeter fmeter;		/* memory_pressure filter */

	/* partition number for rebuild_sched_domains() */
	int pn;

	/* for custom sched domain */
	int relax_domain_level;

	/* used for walking a cpuset hierarchy */
	struct list_head stack_list;
};
struct cpuset *cgroup_cs(struct cgroup *cont);
struct cgroup *cs_cgroup(struct cpuset *cs);
extern struct cpuset top_cpuset;


extern struct cgroup *fg_bg, *bg_non_interactive, *delay_tolerant;
extern struct cgroup *cs_fg_bg, *cs_bg_non_interactive, *cs_delay_tolerant;
//int copy_tasks_cgroup_to_cgroup(struct cgroup *from, struct cgroup *to);
extern struct work_struct bind_copy_work;
#endif	/* CONFIG_PHONELAB_TEMPFREQ_CGROUP_CPUSET_BIND */

#ifdef CONFIG_PHONELAB_TEMPFREQ_SEPARATE_BG_THRESHOLDS
extern struct delayed_work threshold_change_bg_work, threshold_change_fg_work;
#endif

#endif	/* __TEMPFREQ_H_ */
