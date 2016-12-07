#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/hash.h>
#include <linux/pid.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/spinlock_types.h>
#include <linux/slab.h>
#include <linux/cgroup.h>

#include <linux/atomic.h>

#include <linux/list.h>
#include <linux/workqueue.h>

#include <linux/phonelab.h>

#include <trace/events/phonelab.h>

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
#include <../drivers/tempfreq/tempfreq.h>
#include <trace/events/tempfreq.h>
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
static u64 rtime[4], bg_rtime[4];
u64 last_bg_busy;

static u64 avg_fg_busy(void) {
	int i;
	u64 tot_rtime = 0, tot_fg_rtime = 0;

	for(i = 0; i < 4; i++) {
		if(i == phonelab_tempfreq_mpdecision_coexist_cpu) {
			continue;
		}
		tot_rtime += rtime[i];
		tot_fg_rtime = rtime[i] - bg_rtime[i];
	}
	return div_u64(tot_fg_rtime * 100, tot_rtime);
}

static u64 bg_percent(void) {
	int cpu = phonelab_tempfreq_mpdecision_coexist_cpu;
	if(rtime[cpu] == 0) {
		return 0;
	}
	return div_u64(bg_rtime[cpu] * 100, rtime[cpu]);
}
#endif


unsigned int periodic_ctx_switch_info_freq = 1000;

struct periodic_work {
	struct work_struct work;
	struct delayed_work dwork;
	int cpu;
};

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
static struct work_struct start_perf_work, stop_perf_work;

DEFINE_PER_CPU(struct task_struct *[CTX_SWITCH_INFO_LIM], ctx_switch_info);
DEFINE_PER_CPU(int, ctx_switch_info_idx);
#endif

DEFINE_PER_CPU(spinlock_t, ctx_switch_info_lock);
DEFINE_PER_CPU(atomic64_t, periodic_log_idx);
DEFINE_PER_CPU(atomic_t, test_field);
DEFINE_PER_CPU(struct periodic_work, periodic_ctx_switch_info_work);
static struct work_struct setup_periodic_work_cpu;

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_HASH
#define HASH_BITS 8
#define HT_SIZE 1 << HASH_BITS
DEFINE_PER_CPU(struct hlist_head[HT_SIZE], ctx_switch_ht);
#endif

int periodic_ctx_switch_info_ready;

static void clear_cpu_ctx_switch_info(int cpu);
static inline void setup_periodic_work(int cpu);
static inline void destroy_periodic_work(int cpu);

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
static void start_instruction_counter(struct work_struct *work);
static void stop_instruction_counter(struct work_struct *work);
static inline void select_counter(int idx);
static inline void enable_instruction_counter(void);
static inline void disable_instruction_counter(void);
static inline u32 read_instruction_counter(void);
static inline void write_instruction_counter(u32 val);
#endif

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_HASH

//struct hlist_head ctx_switch_ht[NR_CPUS][HT_SIZE];

// Get the hash bucket from the per-cpu hash table.
// We hash the task pointer, but could also try the pid.
static inline
struct hlist_head *
get_hash_bucket(int cpu, struct task_struct *task)
{
	int index = (int)hash_ptr(task, HASH_BITS);
	return &per_cpu(ctx_switch_ht[index], cpu);
}

// Find the stats struct for task in the hash bucket, or NULL if it's not there.
static inline
struct periodic_task_stats *
find_task_stats(struct task_struct *task, struct hlist_head *bucket)
{
	struct periodic_task_stats *stats;
	struct hlist_node *node;

	hlist_for_each_entry(stats, node, bucket, hlist) {
		if (stats->pid == task->pid) {
			return stats;
		}
	}
	return NULL;
}

// Create a periodic_task_stats object from a task
static
struct periodic_task_stats *
stats_from_task(struct task_struct *task)
{
	struct periodic_task_stats *stats;
	stats = kzalloc(sizeof(*stats), GFP_KERNEL);

	stats->pid = task->pid;
	stats->tgid = task->tgid;
	stats->nice = task_nice(task);
	memcpy(stats->comm, task->comm, TASK_COMM_LEN);
	INIT_HLIST_NODE(&stats->hlist);

	return stats;
}

static inline
void
diff_task_cputime(struct task_cputime *old, struct task_cputime *new, struct task_cputime *res)
{
	res->utime = new->utime - old->utime;
	res->stime = new->stime - old->stime;
	res->sum_exec_runtime = new->sum_exec_runtime - old->sum_exec_runtime;
}

static inline
void
add_task_cputime(struct task_cputime *old, struct task_cputime *delta, struct task_cputime *res)
{
	res->utime = old->utime + delta->utime;
	res->stime = old->stime + delta->stime;
	res->sum_exec_runtime = old->sum_exec_runtime + delta->sum_exec_runtime;
}

// Called on context_switch to update statistics for the task that just ran,
// and initialize state for the task that's about to run.
void periodic_ctx_switch_update(struct task_struct *prev, struct task_struct *next)
{
	int cpu;
	struct hlist_head *bucket;
	struct periodic_task_stats *stats;
	struct task_cputime time_diff, cur_time;
	char buf[128];

	if(unlikely(!periodic_ctx_switch_info_ready))
		return;

	cpu = get_cpu();

	// Account for the task that just finished
	stats = find_task_stats(prev, get_hash_bucket(cpu, prev));

	if (likely(stats)) {
		// Update time stats
		task_times(prev, &cur_time.utime, &cur_time.stime);
		cur_time.sum_exec_runtime = prev->se.sum_exec_runtime;
		diff_task_cputime(&stats->prev_time, &cur_time, &time_diff);
		add_task_cputime(&stats->agg_time, &time_diff, &stats->agg_time);
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
			rtime[cpu] += time_diff.sum_exec_runtime;
#endif
		if (stats->count_as_bg) {
			add_task_cputime(&stats->agg_bg_time, &time_diff, &stats->agg_bg_time);
#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
			bg_rtime[cpu] += time_diff.sum_exec_runtime;
#endif
		}

		// Update reason for being taken off the CPU
		if (prev->state == TASK_RUNNING || prev->state == TASK_WAKING) {
			stats->dequeue_reasons[0]++;
		} else if (prev->state == TASK_INTERRUPTIBLE) {
			stats->dequeue_reasons[1]++;
		} else if (prev->state == TASK_UNINTERRUPTIBLE) {
			stats->dequeue_reasons[2]++;
		} else {
			stats->dequeue_reasons[3]++;
		}
	} else {
		snprintf(buf, 128, "periodic_stats: could not find pid %d (%s) cpu: %d\n", prev->pid, prev->comm, cpu);
		trace_phonelab_periodic_warning_cpu(buf, cpu);
	}

	// Account for the new task
	bucket = get_hash_bucket(cpu, next);
	stats = find_task_stats(next, bucket);
	if (stats == NULL) {
		stats = stats_from_task(next);
		hlist_add_head(&stats->hlist, bucket);
	}
	// Set prev times for diffs when is context switched out
	task_times(next, &stats->prev_time.utime, &stats->prev_time.stime);
	stats->prev_time.sum_exec_runtime = next->se.sum_exec_runtime;
	stats->count_as_bg = is_background_task(next);

	preempt_enable_no_resched();
}

static
unsigned
do_trace_periodic_ctx_switch(int cpu, u64 periodic_log_idx)
{
	int i;
	unsigned count;
	struct periodic_task_stats *stats;
	struct hlist_node *node;
	struct hlist_head *bucket;

	periodic_ctx_switch_update(current, current);
	count = 0;
	for (i = 0; i < HT_SIZE; i++) {
		bucket = &per_cpu(ctx_switch_ht[i], cpu);
		hlist_for_each_entry(stats, node, bucket, hlist) {
			trace_phonelab_periodic_ctx_switch_info(stats, cpu, periodic_log_idx);
			count++;
		}
	}

	// Clear the hashtable
	clear_cpu_ctx_switch_info(cpu);
	return count;
}

static inline
void
clear_cpu_ctx_switch_info(int cpu)
{
	int i;
	struct periodic_task_stats *stats;
	struct hlist_node *node, *other;
	struct hlist_head *bucket;

	for (i = 0; i < HT_SIZE; i++) {
		bucket = &per_cpu(ctx_switch_ht[i], cpu);
		hlist_for_each_entry_safe(stats, node, other, bucket, hlist) {
			if (stats->pid != current->pid) {
				hlist_del(&stats->hlist);
				kfree(stats);
			} else {
				stats->count_as_bg = 0;
				memset(&stats->agg_time, 0, sizeof(struct task_cputime));
				memset(&stats->agg_bg_time, 0, sizeof(struct task_cputime));
				stats->dequeue_reasons[0] = 0;
				stats->dequeue_reasons[1] = 0;
				stats->dequeue_reasons[2] = 0;
				stats->dequeue_reasons[3] = 0;
			}
		}
	}
}
#endif	/* CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG */

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
static
unsigned
do_trace_periodic_ctx_switch(int cpu, u64 periodic_log_idx)
{
	int i, lim;
	struct task_struct *task;
	unsigned count;

	atomic_set(&per_cpu(test_field, cpu), 1);
	lim = per_cpu(ctx_switch_info_idx, cpu);
	count = 0;

//		printk(KERN_DEBUG "periodic: cpu=%d lim=%d\n", cpu, lim);
	for(i = 0; i < lim; i++) {
		task = per_cpu(ctx_switch_info[i], cpu);
		if(task == NULL)
			continue;
		task_lock(task);
		if(!task->is_logged[cpu]) {
//				task_times(task, &utime, &stime);
//				printk(KERN_DEBUG "periodic: cpu=%d pid=%d tgid=%d comm=%s utime_t=%lu stime_t=%lu cutime=%lu cstime=%lu"
//					"cutime_t=%lu cstime_t=%lu cutime=%lu cstime=%lu",
//					cpu, task->pid, task->tgid, task->comm,
//					task->utime, task->stime,
//					cputime_to_clock_t(utime), cputime_to_clock_t(stime),
//					task->signal->cutime, task->signal->cstime,
//					cputime_to_clock_t(task->signal->cutime), cputime_to_clock_t(task->signal->cstime));
			trace_phonelab_periodic_ctx_switch_info(task, cpu, periodic_log_idx);
			task->is_logged[cpu] = 1;
			count++;
		}
		task_unlock(task);
	}
	clear_cpu_ctx_switch_info(cpu);
	atomic_set(&per_cpu(test_field, cpu), 0);
	return count;
}

static inline
void
clear_cpu_ctx_switch_info(int cpu)
{
	int i;
	struct task_struct *task;
	for(i = 0; i < CTX_SWITCH_INFO_LIM; i++) {
		task = per_cpu(ctx_switch_info[i], cpu);
		if(task == NULL)
			continue;
		task_lock(task);
		task->is_logged[cpu] = 0;
		task_unlock(task);
		per_cpu(ctx_switch_info[i], cpu) = NULL;
	}
	per_cpu(ctx_switch_info_idx, cpu) = 0;
}
#endif

void __cpuinit periodic_ctx_switch_info(struct work_struct *w) {

	int cpu, wcpu;
	struct delayed_work *work, *dwork;
	struct periodic_work *pwork;
	unsigned DELAY=periodic_ctx_switch_info_freq;
	u64 log_idx;
	unsigned count;
#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
	u32 val;
#endif

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
	int i;
	u64 fg_busy;
#endif

#ifdef TIMING
	u64 ns;
#endif
	cpu = get_cpu();
#ifdef TIMING
	ns = sched_clock_cpu(cpu);
#endif

	dwork = container_of(w, struct delayed_work, work);
	pwork = container_of(dwork, struct periodic_work, dwork);
	wcpu = pwork->cpu;
	if(unlikely(cpu != wcpu)) {
		char buf[64];
		snprintf(buf, 64, "wrong cpu (%d != %d)..restarting", cpu, wcpu);
		trace_phonelab_periodic_warning_cpu(buf, cpu);
		cpu = wcpu;
		goto out;
	}

	if(unlikely(!periodic_ctx_switch_info_ready))
		goto out;
	if(unlikely(atomic_read(&per_cpu(test_field, cpu)) == 1)) {
		// We were rescheduled while another version of this function
		// was still running..Just reschedule in 10ms
		DELAY=10;
		goto out;
	}

	// Get the periodic log index
	log_idx = atomic64_inc_return(&per_cpu(periodic_log_idx, cpu));
	trace_phonelab_periodic_ctx_switch_marker(cpu, 1, 0, log_idx);
	count = do_trace_periodic_ctx_switch(cpu, log_idx);
	trace_phonelab_periodic_ctx_switch_marker(cpu, 0, count, log_idx);

#ifdef CONFIG_PHONELAB_TEMPFREQ_MPDECISION_COEXIST
	if(cpu == phonelab_tempfreq_mpdecision_coexist_cpu) {
		last_bg_busy = bg_percent();
		fg_busy = avg_fg_busy();
		handle_bg_update(last_bg_busy, fg_busy);
		for(i = 0; i < 4; i++) {
			rtime[i] = 0;
			bg_rtime[i] = 0;
		}
	}
#endif

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
	val = read_instruction_counter();
	trace_phonelab_instruction_count(cpu, val);
	disable_instruction_counter();
	write_instruction_counter(0);
	enable_instruction_counter();
#endif
out:
	work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
	schedule_delayed_work_on(cpu, work, msecs_to_jiffies(DELAY));
#ifdef TIMING
	trace_phonelab_timing(__func__, cpu, sched_clock_cpu(cpu) - ns);
#endif
	preempt_enable_no_resched();
}

static int
hotplug_handler(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int cpu = (int)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
		trace_phonelab_periodic_warning_cpu("notify_up", smp_processor_id());
		//printk(KERN_DEBUG "periodic: notify_up: cpu=%d smp_cpu=%d\n", cpu, smp_processor_id());
		setup_periodic_work(cpu);
		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		trace_phonelab_periodic_warning_cpu("notify_down", smp_processor_id());
		//printk(KERN_DEBUG "periodic: notify_down: cpu=%d smp_cpu=%d\n", cpu, smp_processor_id());
		destroy_periodic_work(cpu);
		break;
	};
	return NOTIFY_OK;
}

void init_periodic_work_on_cpu(struct work_struct *w)
{
	struct delayed_work *work;
	int cpu = smp_processor_id();
	clear_cpu_ctx_switch_info(cpu);
	work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
	schedule_delayed_work(work, msecs_to_jiffies(periodic_ctx_switch_info_freq));
}

static inline void setup_periodic_work(int cpu)
{
	trace_phonelab_periodic_warning_cpu("setup_periodic_work", cpu);
#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
	schedule_work_on(cpu, &start_perf_work);
#endif
	schedule_work_on(cpu, &setup_periodic_work_cpu);
}

static inline void destroy_periodic_work(int cpu)
{
	struct delayed_work *work;
	trace_phonelab_periodic_warning_cpu("destroy_periodic_work", cpu);
#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
	schedule_work_on(cpu, &stop_perf_work);
#endif
	work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
	cancel_delayed_work(work);
}
static struct notifier_block __cpuinitdata hotplug_notifier = {
	.notifier_call		= hotplug_handler,
};

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
static u32 num_events;
#define	ARMV7_IDX_CYCLE_COUNTER	0
#define	ARMV7_IDX_COUNTER0	1
#define	ARMV7_IDX_COUNTER_LAST	(ARMV7_IDX_CYCLE_COUNTER + num_events - 1)

#define	ARMV7_MAX_COUNTERS	32
#define	ARMV7_COUNTER_MASK	(ARMV7_MAX_COUNTERS - 1)

/*
 * ARMv7 low level PMNC access
 */

/*
 * Perf Event to low level counters mapping
 */
#define	ARMV7_IDX_TO_COUNTER(x)	\
	(((x) - ARMV7_IDX_COUNTER0) & ARMV7_COUNTER_MASK)
/*
 * Per-CPU PMNC: config reg
 */
#define ARMV7_PMNC_E		(1 << 0) /* Enable all counters */
#define ARMV7_PMNC_P		(1 << 1) /* Reset all counters */
#define ARMV7_PMNC_C		(1 << 2) /* Cycle counter reset */
#define ARMV7_PMNC_D		(1 << 3) /* CCNT counts every 64th cpu cycle */
#define ARMV7_PMNC_X		(1 << 4) /* Export to ETM */
#define ARMV7_PMNC_DP		(1 << 5) /* Disable CCNT if non-invasive debug*/
#define	ARMV7_PMNC_N_SHIFT	11	 /* Number of counters supported */
#define	ARMV7_PMNC_N_MASK	0x1f
#define	ARMV7_PMNC_MASK		0x3f	 /* Mask for writable bits */

/*
 * FLAG: counters overflow flag status reg
 */
#define	ARMV7_FLAG_MASK		0xffffffff	/* Mask for writable bits */
#define	ARMV7_OVERFLOWED_MASK	ARMV7_FLAG_MASK

/*
 * PMXEVTYPER: Event selection reg
 */
#define	ARMV7_EVTYPE_MASK	0xc00000ff	/* Mask for writable bits */
#define	ARMV7_EVTYPE_EVENT	0xff		/* Mask for EVENT bits */

/*
 * Event filters for PMUv2
 */
#define	ARMV7_EXCLUDE_PL1	(1 << 31)
#define	ARMV7_EXCLUDE_USER	(1 << 30)
#define	ARMV7_INCLUDE_HYP	(1 << 27)


static void start_instruction_counter(struct work_struct *work)
{
	int cpu = get_cpu();
	int event = 8;	// Instruction counter
	(void) work;
	armv7_pmnc_write(armv7_pmnc_read() | ARMV7_PMNC_E);
	// Disable
	select_counter(1);
	disable_instruction_counter();
	select_counter(1);
	// evtsel
	asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r" (event));
	// Reset counter
	write_instruction_counter(0);
	trace_phonelab_info(__func__, cpu, "Enabled instruction counter");
	put_cpu();
}

static void stop_instruction_counter(struct work_struct *work)
{
	int cpu = get_cpu();
	(void) work;
	disable_instruction_counter();
	armv7_pmnc_write(armv7_pmnc_read() & ~ARMV7_PMNC_E);
	trace_phonelab_info(__func__, cpu, "Stopped instruction counter");
	put_cpu();
}

static inline void enable_instruction_counter(void)
{
	int counter = ARMV7_IDX_TO_COUNTER(1);
	// Enable counter
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (BIT(counter)));
}
static inline void disable_instruction_counter(void)
{
	int counter = ARMV7_IDX_TO_COUNTER(1);
	// Disable
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r" (BIT(counter)));
}
static inline void select_counter(int idx)
{
	u32 counter = ARMV7_IDX_TO_COUNTER(idx);
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r" (counter));
	isb();
}
static inline u32 read_instruction_counter(void)
{
	u32 value = 0;
	// Select
	select_counter(1);
	// Read
	asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r" (value));
	return value;
}

static inline void write_instruction_counter(u32 val)
{
	select_counter(1);
	// Write
	asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r" (val));
}
#endif	/* CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG */

static int __init init_periodic_ctx_switch_info(void) {

	int cpu;
	struct delayed_work *work;
	struct periodic_work *pwork;

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
	INIT_WORK(&start_perf_work, start_instruction_counter);
	INIT_WORK(&stop_perf_work, stop_instruction_counter);
#endif
	for_each_possible_cpu(cpu) {
		pwork = &per_cpu(periodic_ctx_switch_info_work, cpu);
		work = &pwork->dwork;
		INIT_DELAYED_WORK(work, periodic_ctx_switch_info);
		pwork->cpu = cpu;
	}
	for_each_online_cpu(cpu) {
#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
		schedule_work_on(cpu, &start_perf_work);
#endif
		work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
		schedule_delayed_work_on(cpu, work, msecs_to_jiffies(periodic_ctx_switch_info_freq));
	}

	INIT_WORK(&setup_periodic_work_cpu, init_periodic_work_on_cpu);

	(void) hotplug_notifier;
	register_cpu_notifier(&hotplug_notifier);

	periodic_ctx_switch_info_ready = 1;

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
	num_events = armv7_read_num_pmnc_events();
	printk(KERN_DEBUG "periodic: num_events: %u\n", num_events);
#endif

	printk(KERN_DEBUG "periodic: init done\n");

	return 0;
}
module_init(init_periodic_ctx_switch_info);


static int __init init_per_cpu_data(void) {
	int i;
#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_HASH
	int j;
#endif

	for_each_possible_cpu(i) {
#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_ORIG
		per_cpu(ctx_switch_info_idx, i) = 0;
		spin_lock_init(&per_cpu(ctx_switch_info_lock, i));
		atomic_set(&per_cpu(test_field, i), 0);
		atomic64_set(&per_cpu(periodic_log_idx, i), 0);
#endif

#ifdef CONFIG_PERIODIC_CTX_SWITCH_TRACING_HASH
		// Init per-cpu hash table
		for (j = 0; j < HT_SIZE; j++)
			INIT_HLIST_HEAD(&per_cpu(ctx_switch_ht[j], i));
#endif
	}
	periodic_ctx_switch_info_ready = 0;
	return 0;
}
early_initcall(init_per_cpu_data);
