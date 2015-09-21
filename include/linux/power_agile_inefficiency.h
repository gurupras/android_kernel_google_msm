#ifndef _POWER_AGILE_INEFFICIENCY_H
#define _POWER_AGILE_INEFFICIENCY_H

#ifdef __KERNEL__
//These can be overwritten with a kernel cmd line arg
extern int pa_validation_enabled;
extern int pa_default_budget;
extern int pa_simulation_started;
#endif

#define POWER_AGILE_DEFAULT_INTERVAL 100

//handling inefficiencies in milli-ineffs to avoid floats
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER_CPU
#define CPU_MAX_INEFFICIENCY    4300
#endif

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER_MEM
#define MEM_MAX_INEFFICIENCY    3500
#endif

enum INEFFICIENCY_COMPONENTS {
	POWER_AGILE_CPU,
	POWER_AGILE_MEM,
	POWER_AGILE_NET,
	NR_INEFFICIENCY_COMPONENTS,
};

struct inefficiency_surface;

struct pa_inefficiency {
	u32 budget;
	bool library_started;
	u32 validation_ticks;
	unsigned long interval;
	unsigned long cur_jiffies;
	unsigned is_tuning;
	u64 energy_budget;
	u64 net_energy_budget;
	struct inefficiency_surface *surface;
};

void power_agile_tune(struct task_struct *task);
#endif /* _POWER_AGILE_INEFFICIENCY_H_ */
