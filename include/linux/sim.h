/*
 * sim.h
 *
 */

void m5_exit(u64 ns_delay);
void m5_dumpreset_stats(u64 ns_delay, u64 ns_period);

#ifdef CONFIG_POWER_AGILE
extern asmlinkage long sys_sim_exit(void);
int tune_getinterval(struct task_struct *task);
int tune_setinterval(struct task_struct *task, unsigned int interval);
#endif
