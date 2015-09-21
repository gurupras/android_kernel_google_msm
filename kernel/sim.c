/*
 * sim.c
 *
 * System calls for storing/dumping various state at the
 * start and end of gem5 simulations
 */
#include <linux/preempt.h>
#include <linux/power_agile.h>
#include <linux/power_agile_inefficiency.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/sim.h>

#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
#endif
#ifdef CONFIG_MEM_FREQ
#include <linux/memfreq.h>
#endif

//extern void m5_exit(u64 ns_delay);
//extern void m5_dumpreset(u64 ns_delay, u64 ns_period);

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
static void set_task_budgets(struct task_struct *parent, int budget)
{
	struct task_struct *t;

	/*
	 * Set the budget for the current task and any child
	 * threads
	 */
	t = parent;
	do {
		t->pa.inefficiency.budget = budget;
		pr_debug("tid %d budget set to %d\n", t->pid, budget);
	} while_each_thread(parent, t);
}
#endif

#ifdef CONFIG_POWER_AGILE_TASK_STATS
static void store_stats(void)
{
	struct task_struct *idle;

	/* Handle all other threads */
	struct task_struct *g, *p;

	/* Handle the idle task */
	idle = idle_task(0);

#ifdef CONFIG_POWER_AGILE_SIM
	memcpy(&idle->pa.base_sim_start, &idle->pa.base_stats, sizeof(struct pa_base_stats));
	memcpy(&idle->pa.sim_start, &idle->pa.stats, sizeof(struct statistics));
#endif

	do_each_thread(g, p) {
#ifdef CONFIG_POWER_AGILE_SIM
		memcpy(&p->pa.base_sim_start, &p->pa.base_stats, sizeof(struct pa_base_stats));
		memcpy(&p->pa.sim_start, &p->pa.stats, sizeof(struct statistics));
#endif
    } while_each_thread(g, p);
}
#endif	/* CONFIG_POWER_AGILE_TASK_STATS */

#ifdef CONFIG_POWER_AGILE
SYSCALL_DEFINE1(sim_start, int, budget)
{
	/*
	 * Set everything up to track the simulation
	 */

	extern int pa_simulation_started;
	pr_debug("syscall: sim_start\n");
	//We don't want to be interrupted during this
	preempt_disable();

	//Update any unaccounted stats and store off starting values
#ifdef CONFIG_POWER_AGILE_TASK_STATS
	store_power_agile_task_stats(current);
	store_stats();
#endif
    //Set the budget for the current process and any threads
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
	set_task_budgets(current, budget);
#endif

	preempt_enable();

	pr_debug("Starting Power Agile Simulation\n");
	pa_simulation_started = 1;

	//dump and reset gem5 stats.  We probably only need to reset...
    asm volatile(
    	"ldr r0, =0x0\n\t"
    	"ldr r1, =0x0\n\t"
    	".long 0xEE000110 | (0x42 << 16)"
    	);
	//m5_dumpreset(0, 0);

	return 0;
}

SYSCALL_DEFINE0(sim_exit)
{
	/*
	 * 	Turn off interrupts
	 * 	For each process
	 *		print current stats
	 *  exit gem5
	 */
	struct task_struct *g, *p;

	pr_debug("syscall: sim_exit\n");
 	preempt_disable();

#ifdef CONFIG_POWER_AGILE_TASK_STATS
 	store_power_agile_task_stats(current);
#endif

 	/* print the idle task statistics */
#ifdef CONFIG_POWER_AGILE_SIM
 	print_simulation_stats(idle_task(0));
#endif

	do_each_thread(g, p) {
#ifdef CONFIG_POWER_AGILE_SIM
		print_simulation_stats(p);
#endif
    } while_each_thread(g, p);

	asm volatile(
    	"ldr r0, =0x0\n\t"
    	"ldr r1, =0x0\n\t"
    	".long 0xEE000110 | (0x21 << 16)\n\t"
        "mov pc,lr"
    	);
    //m5_exit(0);

	return 0;
}
#endif	/* CONFIG_POWER_AGILE */

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
SYSCALL_DEFINE0(tune_start)
{
	pr_debug("syscall: tune_start\n");
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
	current->pa.inefficiency.is_tuning = 1;
	store_power_agile_task_stats(current);

	memcpy(&current->pa.tune_start, &current->pa.stats, sizeof(struct statistics));
	memcpy(&current->pa.base_tune_start, &current->pa.base_stats, sizeof(struct pa_base_stats));
#endif
	return 0;
}

SYSCALL_DEFINE0(tune_stop)
{
	struct pa_base_stats base_diff;

	struct statistics diff;

	pr_debug("syscall: tune_stop\n");
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
	current->pa.inefficiency.is_tuning = 0;
	store_power_agile_task_stats(current);

	//Get diffs
	diff_statistics(&current->pa.stats, &current->pa.tune_start, &diff);

	//Increment tuning stats
	add_statistics(&current->pa.tune_stats, &diff, &current->pa.tune_stats);

	/*
	 * Reset tuning stats.
	 *
	 * Another option is to leave these alone and diff them at the end.
	 * If we do this, which means tuning includes tuning stats, we need
	 * to modify the tuning library saved start time accordingly.
	 */
	memcpy(&current->pa.stats, &current->pa.tune_start, sizeof(struct statistics));

	//Get diffs
	diff_base_stats(&current->pa.base_stats, &current->pa.base_tune_start, &base_diff);

	//Increment tuning stats
	add_base_stats(&current->pa.base_tune_stats, &base_diff, &current->pa.base_tune_stats);

	/* Reset tuning stats.  See above. */
	memcpy(&current->pa.base_stats, &current->pa.base_tune_start, sizeof(struct pa_base_stats));
#endif
	return 0;
}

inline int tune_getinterval(struct task_struct *task)
{
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
        return jiffies_to_msecs(task->pa.inefficiency.interval);
#else
        return 0;
#endif
}

SYSCALL_DEFINE0(tune_getinterval)
{
	return tune_getinterval(current);
}

inline int tune_setinterval(struct task_struct *task, unsigned int interval)
{
	pr_debug("tune_setinterval: PID: %05d Called with interval :%u\n", task->pid, interval);
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
        task->pa.inefficiency.interval = msecs_to_jiffies(interval);
	pr_debug("tune_setinterval: PID: %05d jiffies              :%lu\n", task->pid, task->pa.inefficiency.interval);
#endif
        return 0;
}

SYSCALL_DEFINE1(tune_setinterval, unsigned, interval)
{
	return tune_setinterval(current, interval);
}
#endif	/* CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER */
