#ifndef __TEMPFREQ_H_
#define __TEMPFREQ_H_


#define MIN_TEMPERATURE		0
#define MAX_TEMPERATURE		100
#define TEMP_FREQUENCY_MS	250

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


#ifdef CONFIG_PHONELAB_CPUFREQ_GOVERNOR_FIX
ssize_t show_ondemand_ignore_bg(char *buf);
ssize_t set_ondemand_ignore_bg(const char *buf, size_t count);
#endif

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

#define tf_to_attr(a) container_of(a, struct tempfreq_attr, attr)

int tempfreq_show(struct kobject *kobj, struct attribute *attr, char *buf);
ssize_t tempfreq_store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count);

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
int __init init_mpdecision_coexist(void);
extern int phonelab_tempfreq_mpdecision_coexist_enable;
extern int phonelab_tempfreq_mpdecision_blocked;
void start_bg_core_control(void);
void stop_bg_core_control(void);
extern struct cgroup *fg_bg, *bg_non_interactive, *delay_tolerant;
extern struct tempfreq_attr mpdecision_coexist_enable;
extern struct tempfreq_attr mpdecision_coexist_upcall;
#endif

#endif	/* __TEMPFREQ_H_ */
