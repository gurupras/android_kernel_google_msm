/*
 * power_agile.h
 *
 *  Created on: May 29, 2013
 *      Author: guru
 */

#ifndef _POWER_AGILE_H
#define _POWER_AGILE_H

#include <linux/types.h>
#include <linux/linkage.h>
#include <linux/pid.h>

#ifdef CONFIG_POWER_AGILE_TASK_STATS
#include <linux/../../lib/library/kernel/gem5_model.h>
#include <linux/power_agile_stats.h>
#endif

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
#include <linux/../../lib/library/kernel/state.h>
#include <linux/../../lib/library/kernel/algorithm.h>
#include <linux/power_agile_inefficiency.h>
#endif

#ifdef CONFIG_RATE_LIMITING
#include <linux/rate_limiting.h>
#endif

struct task_struct;
struct sock;

//Performance Registers
//FIXME: Remove stuff we don't need
enum power_agile_register {
	PID_REGISTER,
	CPU_BUSY_CYCLES,
	CPU_L1L2_STALL_CYCLES,
	CPU_DRAM_STALL_TIME_NS,
	CPU_QUIESCE_TIME_NS,
	CPU_USER_INSTRUCTIONS,
	CPU_KERNEL_INSTRUCTIONS,
	MEM_BUSY_TIME_NS,
	MEM_IDLE_TIME_NS,
	MEM_ACTIVATE_COUNT,
	MEM_PRECHARGE_COUNT,
	MEM_READ_COUNT,
	MEM_WRITE_COUNT,
	MEM_PRECHARGE_TIME_NS,
	MEM_ACTIVE_TIME_NS,
	MEM_REFRESH_COUNT,
	MEM_ACTIVE_IDLE_OVERLAP_TIME_NS,
	MEM_PRECHARGE_IDLE_OVERLAP_TIME_NS
};
extern const char *power_agile_register_names[];

extern int power_agile_new_counters;

extern int pa_default_cpu_freq, pa_default_mem_freq;
extern int is_pa_ctx_switch_freq;

#define RESOLVE(x) #x

void power_agile_read_reg(enum power_agile_register reg, u32 *ret_addr);
void power_agile_write_reg(enum power_agile_register reg, u32 value);


struct pa_tuning {
#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
	struct tuning_algorithm *algorithm;
	struct energy_container tuning_energy;
#endif
};

/*
 * For now, we'll store stats from a fixed number of interfaces.
 * In the future, we could use a hash table similar to how the
 * kernel stores devices.
 */
#define PA_NUM_NET_IFACE 5


struct power_agile {
	u32 cpu_freq;
	u32 mem_freq;
#ifdef CONFIG_POWER_AGILE_TASK_STATS
	struct pa_base_stats base_stats;

	struct statistics stats;
	struct statistics quantum_start_stats;
	u64 net_bytes;
	u64 break_after;
#endif
#ifdef CONFIG_POWER_AGILE_ENERGY
	u64 cum_energy;
	struct energy_container energy;
#endif

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
	struct pa_tuning tuning;
	struct statistics tuning_prev_stats;
	struct pa_inefficiency inefficiency;
	struct parameters current_params;
#endif

#ifdef CONFIG_POWER_AGILE_SIM
	/* Base stats when the simulation started */
	struct pa_base_stats base_sim_start;
#endif

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
	/* The current stats when the tuning started */
	struct pa_base_stats base_tune_start;

	/* Total tuning stats */
	struct pa_base_stats base_tune_stats;
#endif
#ifdef CONFIG_POWER_AGILE_SIM
	/* The stats when the simulation started */
	struct statistics sim_start;
#endif

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
	/* The current stats when the tuning started */
	struct statistics tune_start;

	/* Total tuning stats */
	struct statistics tune_stats;
#endif
#ifdef CONFIG_POWER_AGILE_PERIODIC_LOGGING
	int pa_print_log;
#endif
#ifdef CONFIG_RATE_LIMITING
	int rate_lim_enabled;
	s64 rate_lim_available_energy;
	s64 rate_lim_prev_cum_energy;
	s64 rate_lim_refill_rate;
	s64 rate_lim_default_energy;
	u64 rate_lim_prev_ctx_timestamp;
	u64 rate_lim_prev_sched_timestamp;
#endif
};

#ifdef CONFIG_CPU_FREQ
int pa_update_cpu_freq(int cpu, u32 freq);
#endif
#ifdef CONFIG_MEM_FREQ
int pa_update_mem_freq(u32 freq);
#endif

#ifdef CONFIG_POWER_AGILE_TASK_STATS
void store_power_agile_task_stats(struct task_struct *task);
void update_power_agile_net_stats(struct sock *sk, int ifindex, unsigned int bytes);

#ifdef CONFIG_POWER_AGILE_SIM
void print_simulation_stats(struct task_struct *task);
#endif

#ifdef CONFIG_POWER_AGILE_ENERGY
void power_agile_update_energy(struct task_struct *task);
void power_agile_get_current_parameters(struct task_struct *task, struct parameters *params);
#endif

void diff_base_stats(struct pa_base_stats *lhs, struct pa_base_stats *rhs, struct pa_base_stats *result);
void add_base_stats(struct pa_base_stats *lhs, struct pa_base_stats *rhs, struct pa_base_stats *result);

#endif

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
void power_agile_tune(struct task_struct *task);
#endif

#ifdef CONFIG_POWER_AGILE
struct net_device *dev_get_by_ip(unsigned int ip);
struct tcp_sock;
struct net_device *dev_by_tcp_sock(struct tcp_sock *tp);
#endif

#ifdef CONFIG_POWER_AGILE
unsigned int dev_get_e_min(void);
#endif


#define PA_LOG_TAG "PALOG:"
#define PA_LOG_EXEC 0
#define PA_LOG_EXIT 1
#define PA_LOG_CPU 2
#define PA_LOG_MEM 3
#define PA_LOG_SIM_STATS 4
#define PA_LOG_TUNE_STATS 5

#ifdef CONFIG_POWER_AGILE_PERIODIC_LOGGING
void power_agile_log_stats(struct task_struct *task);
#endif

#ifdef CONFIG_POWER_AGILE_TASK_STATS_EXIT
void printk_power_agile_task_stats(struct task_struct *task);
#endif

#ifdef CONFIG_QEMU
unsigned int cpufreq_get_unlocked(unsigned int cpu);
unsigned int memfreq_get(void);
#endif
#endif /* _POWER_AGILE_H */
