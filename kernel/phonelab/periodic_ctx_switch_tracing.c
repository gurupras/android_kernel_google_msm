#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/pid.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/spinlock_types.h>

#include <linux/atomic.h>

#include <linux/list.h>
#include <linux/workqueue.h>

#include <linux/phonelab.h>

#include <trace/events/phonelab.h>

struct periodic_work {
	struct work_struct work;
	struct delayed_work dwork;
	int cpu;
};

static struct work_struct start_perf_work, stop_perf_work;

DEFINE_PER_CPU(struct task_struct *[CTX_SWITCH_INFO_LIM], ctx_switch_info);
DEFINE_PER_CPU(int, ctx_switch_info_idx);
DEFINE_PER_CPU(spinlock_t, ctx_switch_info_lock);
DEFINE_PER_CPU(atomic_t, test_field);
DEFINE_PER_CPU(struct periodic_work, periodic_ctx_switch_info_work);

int periodic_ctx_switch_info_ready;

static void clear_cpu_ctx_switch_info(int cpu);
static inline void setup_periodic_work(int cpu);
static inline void destroy_periodic_work(int cpu);

static void start_instruction_counter(struct work_struct *work);
static void stop_instruction_counter(struct work_struct *work);
static inline void select_counter(int idx);
static inline void enable_instruction_counter(void);
static inline void disable_instruction_counter(void);
static inline u32 read_instruction_counter(void);
static inline void write_instruction_counter(u32 val);


void periodic_ctx_switch_info(struct work_struct *w) {

	int cpu, wcpu;
	int i, lim;
	struct task_struct *task;
	struct delayed_work *work, *dwork;
	struct periodic_work *pwork;
	spinlock_t *spinlock;
	unsigned long utime, stime, flags;
	int DELAY=100;
	u32 val;
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

	(void) utime;
	(void) stime;
	(void) spinlock;
	(void) flags;

	if(unlikely(!periodic_ctx_switch_info_ready))
		goto out;
	if(unlikely(atomic_read(&per_cpu(test_field, cpu)) == 1)) {
		// We were rescheduled while another version of this function
		// was still running..Just reschedule in 10ms
		DELAY=10;
		goto out;
	}
//	local_irq_save(flags);
		atomic_set(&per_cpu(test_field, cpu), 1);
		trace_phonelab_periodic_ctx_switch_marker(cpu, 1);
		lim = per_cpu(ctx_switch_info_idx, cpu);

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
				trace_phonelab_periodic_ctx_switch_info(task, cpu);
				task->is_logged[cpu] = 1;
			}
			task_unlock(task);
		}
		clear_cpu_ctx_switch_info(cpu);
		atomic_set(&per_cpu(test_field, cpu), 0);
		trace_phonelab_periodic_ctx_switch_marker(cpu, 0);
//	local_irq_restore(flags);
	val = read_instruction_counter();
	trace_phonelab_instruction_count(cpu, val);
	disable_instruction_counter();
	write_instruction_counter(0);
	enable_instruction_counter();
out:
	work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
	schedule_delayed_work_on(cpu, work, msecs_to_jiffies(DELAY));
#ifdef TIMING
	trace_phonelab_timing(__func__, cpu, sched_clock_cpu(cpu) - ns);
#endif
	put_cpu();
}

static void
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

static int
hotplug_handler(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int cpu = (int)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
		setup_periodic_work(cpu);
		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		destroy_periodic_work(cpu);
		break;
	};
	return NOTIFY_OK;
}

static inline void setup_periodic_work(int cpu)
{
	struct delayed_work *work;
	schedule_work_on(cpu, &start_perf_work);
	clear_cpu_ctx_switch_info(cpu);
	work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
	schedule_delayed_work_on(cpu, work, msecs_to_jiffies(100));
}

static inline void destroy_periodic_work(int cpu)
{
	struct delayed_work *work;
	schedule_work_on(cpu, &stop_perf_work);
	clear_cpu_ctx_switch_info(cpu);
	work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
	cancel_delayed_work(work);
}
static struct notifier_block __cpuinitdata hotplug_notifier = {
	.notifier_call		= hotplug_handler,
};


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

static int __init init_periodic_ctx_switch_info(void) {

	int cpu;
	struct delayed_work *work;
	struct periodic_work *pwork;

	INIT_WORK(&start_perf_work, start_instruction_counter);
	INIT_WORK(&stop_perf_work, stop_instruction_counter);

	for_each_possible_cpu(cpu) {
		pwork = &per_cpu(periodic_ctx_switch_info_work, cpu);
		work = &pwork->dwork;
		INIT_DELAYED_WORK(work, periodic_ctx_switch_info);
		pwork->cpu = cpu;
	}
	for_each_online_cpu(cpu) {
		schedule_work_on(cpu, &start_perf_work);
		work = &per_cpu(periodic_ctx_switch_info_work.dwork, cpu);
		schedule_delayed_work_on(cpu, work, msecs_to_jiffies(100));
	}

	(void) hotplug_notifier;
	register_cpu_notifier(&hotplug_notifier);

	periodic_ctx_switch_info_ready = 1;

	num_events = armv7_read_num_pmnc_events();
	printk(KERN_DEBUG "periodic: num_events: %u\n", num_events);

	printk(KERN_DEBUG "periodic: init done\n");

	return 0;
}
module_init(init_periodic_ctx_switch_info);


static int __init init_per_cpu_data(void) {
	int i;
	for_each_possible_cpu(i) {
		per_cpu(ctx_switch_info_idx, i) = 0;
		spin_lock_init(&per_cpu(ctx_switch_info_lock, i));
		atomic_set(&per_cpu(test_field, i), 0);
	}
	periodic_ctx_switch_info_ready = 0;
	return 0;
}
early_initcall(init_per_cpu_data);

