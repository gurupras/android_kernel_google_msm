/*
 * power_agile.c
 *
 *  Created on: Jun 11, 2013
 *      Author: guru
 */

#include <linux/power_agile.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/netdevice.h>
#include <linux/pid.h>
#include <linux/types.h>
#include <linux/syscalls.h>

#include <linux/atomic.h>

#include <linux/list.h>
#include <linux/workqueue.h>

#include <net/sock.h>

#include <linux/inetdevice.h>
#include <linux/tcp.h>

#include <linux/cpufreq.h>

#ifdef CONFIG_RATE_LIMITING
#include <linux/rate_limiting.h>
#endif

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
#include <linux/sim.h>
#include <linux/../../lib/library/kernel/tune.h>
#include <linux/power_agile_inefficiency.h>
#endif

#ifdef CONFIG_POWER_AGILE_CPU_USE_NEW_COUNTERS
int power_agile_new_counters = 1;
#else
int power_agile_new_counters = 0;
#endif
int pa_default_budget = 0;
int pa_validation_enabled = 1;
int pa_simulation_started = 1;
// This flag is used to enable/disable frequency switching on ctx switch
int is_pa_ctx_switch_freq = 0;

const char *power_agile_register_names[] = {
	"PID_REGISTER",
	"CPU_BUSY_CYCLES",
	"CPU_L1L2_STALL_CYCLES",
	"CPU_DRAM_STALL_TIME_NS",
	"CPU_QUIESCE_TIME_NS",
	"CPU_BUSY_TIME_NS",
	"CPU_USER_INSTRUCTIONS",
	"CPU_KERNEL_INSTRUCTIONS",
	"MEM_BUSY_TIME_NS",
	"MEM_BUSY_TIME_NS",
	"MEM_IDLE_TIME_NS",
	"MEM_ACTIVATE_COUNT",
	"MEM_PRECHARGE_COUNT",
	"MEM_READ_COUNT",
	"MEM_WRITE_COUNT",
	"MEM_PRECHARGE_TIME_NS",
	"MEM_ACTIVE_TIME_NS",
	"MEM_REFRESH_COUNT",
	"MEM_ACTIVE_IDLE_OVERLAP_TIME_NS",
	"MEM_PRECHARGE_IDLE_OVERLAP_TIME_NS"
};

void power_agile_read_reg(enum power_agile_register reg, u32 *ret_addr) {
}
void power_agile_write_reg(enum power_agile_register reg, u32 value) {
}

SYSCALL_DEFINE0(dummy)
{
	return 0;
}

#ifdef CONFIG_POWER_AGILE_TASK_STATS
inline static void store_stats(struct task_struct *curr) {
#ifdef TIMING
	u64 ns = sched_clock();
#endif
	curr->pa.stats.cpu_busy_time_ns += curr->se.sum_exec_runtime - curr->prev_sum_exec_runtime;
	curr->prev_sum_exec_runtime = curr->se.sum_exec_runtime;

#ifdef TIMING
	printk(KERN_INFO "timing:%s: %lluns\n", __func__, sched_clock() - ns);
#endif
}

inline void store_power_agile_task_stats(struct task_struct *task) {
#ifdef CONFIG_RATE_LIMITING
	u64 energy;
#endif
#ifdef TIMING
	u64 ns = sched_clock();
#endif
	store_stats(task);

#ifdef CONFIG_POWER_AGILE_ENERGY
	power_agile_update_energy(task);
#endif

#ifdef CONFIG_RATE_LIMITING
	if(task->pa.rate_lim_enabled) {
		energy = task->pa.cum_energy - task->pa.rate_lim_prev_cum_energy;
		task->pa.rate_lim_available_energy -= energy;
	}
#endif

#ifdef TIMING
	ns = sched_clock() - ns;
	printk(KERN_INFO "timing:%s: %lluns\n", __func__, ns);
#endif
}

inline void update_power_agile_net_stats(struct sock *sk, int ifindex, unsigned int bytes)
{
	/*
	 * Keep in mind that this happens asynchronously for both rx and tx.
	 */
	struct net_device *dev;
	struct task_struct *task;
	unsigned int emin;

	u64 newEnergy = 0;

	if (!sk->sk_owner_task)
		return;

	task = sk->sk_owner_task;

	/* Energy*/
	dev = dev_get_by_index(sock_net(sk), ifindex);

	if (dev)
	{
		if(dev->netdev_ops->get_epb) {
			newEnergy = (bytes * dev->netdev_ops->get_epb(dev) * 8);
		}
		else {
			pr_debug("%s: Interface '%s' does not have get_epb()\n", __func__, dev->name);
		}
	}

	/* Per-interface stats */
	task->pa.stats.net_bytes[ifindex] += bytes;
#ifdef CONFIG_POWER_AGILE_ENERGY
	task->pa.energy.net_energy_nJ[ifindex] += newEnergy;
#endif
		//pr_debug("Updated per-interface network stats   Index: %d   Bytes: %llu  Total Energy: %llu\n",
			//	task->pa.net_stats[pos].ifindex, task->pa.net_stats[pos].total_bytes, task->pa.net_stats[pos].total_energy);

	/* Grand totals */

	/* E min */
	emin = dev_get_e_min();
	task->pa.stats.net_emin_energy += bytes * emin * 8;
	/* Current */
	task->pa.stats.net_energy += newEnergy;
	task->pa.stats.net_bytes_total += bytes;

	//pr_debug("Updated network stats   Total Bytes: %llu  Total Energy: %llu  E min: %llu\n",
		//	task->pa.net_bytes, task->pa.net_energy, task->pa.net_emin);
}
#endif

#ifdef CONFIG_POWER_AGILE_ENERGY
inline void power_agile_update_energy(struct task_struct *task)
{
	struct statistics diff;
	struct parameters params;
	struct energy_container container;
	u64 energy = 0;
	int cpu;
#ifdef CONFIG_QEMU
	u64 tmp;
#endif

#ifdef TIMING
	u64 ns = sched_clock();
#endif

	if(likely(task->pa.stats.cpu_busy_cycles)) {
		memset(&container, 0, sizeof(struct energy_container));

		cpu = task_thread_info(task)->cpu;

		power_agile_get_current_parameters(task, &params);
		if(params.cpu_frequency_MHZ == 0 || params.mem_frequency_MHZ == 0) {
			return;
		}

		diff_statistics(&task->pa.stats, &task->pa.quantum_start_stats, &diff);
		diff.pid = task->pid;

		//pr_debug("update_energy: begin\n");
		//print_stats(&diff);
		//pr_debug("update_energy: end\n");
		energy = model->estimate_energy(&diff, NULL, &params, &params, &container);
		task->pa.cum_energy += energy;
		add_energy_container(&task->pa.energy, &container, &task->pa.energy);

		memcpy(&task->pa.quantum_start_stats, &task->pa.stats, sizeof(struct statistics));
	}
#ifdef CONFIG_QEMU_ENERGY
	get_random_bytes(&tmp, sizeof(tmp));
	div64_u64_rem(tmp, 100000, &energy);	//Max per call is 100000
	task->pa.cum_energy += energy;
#endif

#ifdef TIMING
	ns = sched_clock() - ns;
	printk(KERN_INFO "timing:%s: %lluns\n", __func__, ns);
#endif
}

inline void power_agile_get_current_parameters(struct task_struct *task, struct parameters *params)
{
	int idx;
	struct net_device *dev;
	memset(params, 0, sizeof(struct parameters));
//	params->cpu_frequency_MHZ = (cpufreq_get_unlocked(cpu) + 500) / 1000;
//	params->mem_frequency_MHZ = (memfreq_get_unlocked() + 500) / 1000;
	params->cpu_frequency_MHZ = task->pa.cpu_freq / 1000;
	params->mem_frequency_MHZ = task->pa.mem_freq / 1000;
	idx = 0;
	for_each_netdev_rcu(&init_net, dev) {
		//printk(KERN_INFO "%s: %s\n", __func__, dev->name);
		if(dev->netdev_ops->get_epb) {
			params->net_epb[idx] = dev->netdev_ops->get_epb(dev);
		}
		if(dev->netdev_ops->get_bandwidth) {
			params->net_bandwidth[idx] = dev->netdev_ops->get_bandwidth(dev);
		}
		idx++;
	}
}
#endif

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
inline void power_agile_tune(struct task_struct *task)
{
	struct statistics *stats, *tune_stats;
	struct energy_container tune_energy;
	struct parameters params, result;
	int cpu;
	struct task_struct *tgid_task;
	struct tuning_algorithm *algorithm;
	u64 interval_ns, interval_ms;
#ifdef TIMING
	u64 ns = sched_clock();
#endif

	if(!library_initialized)
		return;
	/* We want tuning stats to be accounted separately
	 * for understanding overheads
	 */
	memcpy(&task->pa.tune_start, &task->pa.stats, sizeof(struct statistics));
	//TODO :Add network stuff here


	cpu = task_thread_info(task)->cpu;

	// Set up the state
	if(unlikely(task->pid != task->tgid)) {
		tgid_task = get_pid_task(task_tgid(task), PIDTYPE_PID);
		if(task->pa.inefficiency.budget != tgid_task->pa.inefficiency.budget)
			task->pa.inefficiency.budget = tgid_task->pa.inefficiency.budget;
	}
	power_agile_get_current_parameters(task, &params);

	printk(KERN_INFO "%s: PID:%05d  TGID:%05d  budget:%d\n", __func__, task->pid, task->tgid, task->pa.inefficiency.budget);
	//FIXME: This is a hack to stop tuning during atomic
	if(task->pa.stats.cpu_busy_time_ns != 0) {

		stats = kzalloc(sizeof(struct statistics), GFP_KERNEL);
		if(!stats)
			return;
		tune_stats = kzalloc(sizeof(struct statistics), GFP_KERNEL);
		if(!tune_stats)
			return;

		memset(&tune_energy, 0, sizeof(energy_container));
		memset(&result, 0, sizeof(struct parameters));

		// Update energy for task
		store_power_agile_task_stats(task);
		memcpy(stats, &task->pa.stats, sizeof(struct statistics));

		diff_statistics(stats, &task->pa.tuning_prev_stats, stats);
		algorithm = task->pa.tuning.algorithm;
		library_tune(stats, &params, task->pa.inefficiency.budget, algorithm, &result);
		memcpy(&task->pa.current_params, &result, sizeof(struct parameters));
		memcpy(&task->pa.tuning_prev_stats, stats, sizeof(struct statistics));

		printk(KERN_INFO "tuning: PID:%05d: TGID:%05d: CPU-%d: %d, %d\n",
				task->pid, task->tgid,
				cpu, result.cpu_frequency_MHZ, result.mem_frequency_MHZ);

		// Update the tuning interval
		// 100M cycles * 1e9 / HZ = ns
		interval_ns = div_u64(100 * 1000 * 1000 * 1000000000ULL,
				(result.cpu_frequency_MHZ * 1000 * 1000));
		interval_ms = div_u64(interval_ns, 1000000);
		tune_setinterval(task, interval_ms);

		// Update tune stats
		store_power_agile_task_stats(task);
		diff_statistics(&task->pa.stats, &task->pa.tune_start, tune_stats);
		add_statistics(&task->pa.tune_stats, tune_stats, &task->pa.tune_stats);
		model->estimate_energy(tune_stats, NULL, &params, &params, &tune_energy);
		add_energy_container(&task->pa.tuning.tuning_energy, &tune_energy, &task->pa.tuning.tuning_energy);

		kfree(stats);
		kfree(tune_stats);

#ifdef CONFIG_CPU_FREQ
		pa_update_cpu_freq(cpu, (result.cpu_frequency_MHZ * 1000));
		task->pa.cpu_freq = result.cpu_frequency_MHZ * 1000;
#endif
#ifdef CONFIG_MEM_FREQ
		pa_update_mem_freq(result.mem_frequency_MHZ * 1000);
		task->pa.mem_freq = result.mem_frequency_MHZ * 1000;
#endif
	}
	else
		pr_debug("tuning: PID:%05d: cpu_busy_cycles == 0, Not tuning! (Maybe in atomic CPU?)\n", task->pid);

#ifdef TIMING
	ns = sched_clock() - ns;
	printk(KERN_INFO "timing:%s: %lluns\n", __func__, ns);
#endif
}
#endif

#ifdef CONFIG_CPU_FREQ
extern int smp_cpufreq_callback(struct notifier_block *nb,
					unsigned long val, void *data);

static int pa_update_cpu_freq_cpufreq(int cpu, u32 freq) {
	return -EINVAL;
}

int pa_update_cpu_freq(int cpu, u32 freq) {
	return pa_update_cpu_freq_cpufreq(cpu, freq);
}
#endif	/* CONFIG_CPU_FREQ */

#ifdef CONFIG_MEM_FREQ
int pa_update_mem_freq(u32 freq) {
	printk(KERN_WARNING "Calling %s\n", __func__);
}
#endif	/* CONFIG_MEM_FREQ */

#ifdef CONFIG_POWER_AGILE
struct net_device *dev_get_by_ip(unsigned int ip)
{
	struct net_device *dev = NULL, *d = NULL;
	struct in_device *in_dev;
	struct in_ifaddr *ifaddr;
//	pr_debug("%s: ip:%pI4\n", __func__, &ip);
	rcu_read_lock();
	for_each_netdev_rcu(&init_net, d) {
		in_dev = rcu_dereference(d->ip_ptr);
		for(ifaddr = in_dev->ifa_list; ifaddr != NULL; ifaddr = ifaddr->ifa_next) {
			if(ifaddr->ifa_address == ip) {
				dev = d;
				break;
			}
		}
	}
	rcu_read_unlock();
	return dev;
}

struct net_device *dev_by_tcp_sock(struct tcp_sock *tp)
{
	__be32 addr;

	if(tp == NULL)
		return NULL;
	addr = ((struct inet_sock *)tp)->inet_saddr;
	return dev_get_by_ip(addr);
}
#endif

#ifdef CONFIG_POWER_AGILE
unsigned int dev_get_e_min(void)
{
	unsigned int e_min = ~0;
	unsigned int epb;
	struct net_device *dev;

	rcu_read_lock();

	dev = first_net_device(&init_net);
	while (dev) {
		if (dev->netdev_ops->get_epb) {
			epb = dev->netdev_ops->get_epb(dev);
			if (epb && epb < e_min)
			e_min = epb;
		}
		else {
			//pr_debug("%s: WARNING: %s->get_epb() is not registered!\n", __func__, dev->name);
		}
		dev = next_net_device(dev);
	}
	rcu_read_unlock();

	return e_min;
}
#endif





/* ------------- Logging stuff -------------- */
#ifdef CONFIG_POWER_AGILE_PERIODIC_LOGGING
void power_agile_log_stats(struct task_struct *task)
{
	//CPU
	printk(KERN_INFO "%s,%d,%d,%llu,%llu,%llu,%llu\n", PA_LOG_TAG, PA_LOG_CPU, task->pid,
		task->pa.stats.cpu_busy_cycles,
		task->pa.stats.cpu_l1l2_stall_cycles,
		task->pa.stats.cpu_dram_stall_time_ns,
		task->pa.stats.cpu_quiesce_time_ns
		);

	//Memory
	printk(KERN_INFO "%s,%d,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n", PA_LOG_TAG, PA_LOG_MEM, task->pid,
		task->pa.stats.mem_busy_time_ns,
		task->pa.stats.mem_idle_time_ns,
		task->pa.stats.mem_activate_count,
		task->pa.stats.mem_precharge_count,
		task->pa.stats.mem_read_count,
		task->pa.stats.mem_write_count,
		task->pa.stats.mem_precharge_time_ns,
		task->pa.stats.mem_active_time_ns,
		task->pa.stats.mem_refresh_count,
		task->pa.stats.mem_active_idle_overlap_time_ns,
		task->pa.stats.mem_precharge_idle_overlap_time_ns
		);
}

#endif

#ifdef CONFIG_POWER_AGILE_TASK_STATS
#define DIFF_STAT(which_stat) (result->which_stat = lhs->which_stat - rhs->which_stat)
#define ADD_STAT(which_stat) (result->which_stat = lhs->which_stat + rhs->which_stat)

void diff_base_stats(struct pa_base_stats *lhs, struct pa_base_stats *rhs, struct pa_base_stats *result)
{
	DIFF_STAT(user_instructions);
	DIFF_STAT(kernel_instructions);
	DIFF_STAT(user_cycles);
	DIFF_STAT(kernel_cycles);
	DIFF_STAT(instructions);
	DIFF_STAT(cycles);
}

void add_base_stats(struct pa_base_stats *lhs, struct pa_base_stats *rhs, struct pa_base_stats *result)
{
	ADD_STAT(user_instructions);
	ADD_STAT(kernel_instructions);
	ADD_STAT(user_cycles);
	ADD_STAT(kernel_cycles);
	ADD_STAT(instructions);
	ADD_STAT(cycles);
}
#endif

#ifdef CONFIG_POWER_AGILE_SIM

static inline void append_sim_heading(char *buffer, struct task_struct *task, int type)
{
	sprintf(buffer, "%s,%d,%d,%d,%s", PA_LOG_TAG, type, task->tgid, task->pid, task->comm);
}

static void append_sim_stats(char *buffer, struct statistics *stats)
{
	int i;
	int offset;

	offset = strlen(buffer);
	offset += sprintf(buffer + offset, ",%llu,%llu,%llu,%llu",
		stats->cpu_busy_cycles,
		stats->cpu_l1l2_stall_cycles,
		stats->cpu_dram_stall_time_ns,
		stats->cpu_quiesce_time_ns
	);
	offset += sprintf(buffer + offset, ",%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
		stats->mem_busy_time_ns,
		stats->mem_idle_time_ns,
		stats->mem_activate_count,
		stats->mem_precharge_count,
		stats->mem_read_count,
		stats->mem_write_count,
		stats->mem_precharge_time_ns,
		stats->mem_active_time_ns,
		stats->mem_refresh_count,
		stats->mem_active_idle_overlap_time_ns,
		stats->mem_precharge_idle_overlap_time_ns
	);

	for(i = 0; i < PA_NUM_NET_IFACE; i++) {
		offset += sprintf(buffer + offset, ",%llu,%llu,%llu",
			stats->net_bytes[i],
			stats->net_bytes_rx[i],
			stats->net_bytes_tx[i]
		);
	}
}

static void append_sim_base_stats(char *buffer, struct pa_base_stats *stats)
{
	int offset;

	offset = strlen(buffer);
	offset += sprintf(buffer + offset, ",%llu,%llu,%llu,%llu,%llu,%llu",
		stats->user_instructions,
		stats->kernel_instructions,
		stats->user_cycles,
		stats->kernel_cycles,
		stats->instructions,
		stats->cycles
	);
}

static void append_sim_energy_container(char *buffer, struct energy_container *container)
{
	int offset, i;
	u64 energy = 0;

	offset = strlen(buffer);

	energy += container->cpu_leakage_energy_nJ;
	energy += container->cpu_background_energy_nJ;
	energy += container->cpu_dynamic_energy_nJ;

	energy += container->mem_refresh_energy_nJ;
	energy += container->mem_activate_energy_nJ;
	energy += container->mem_precharge_energy_nJ;
	energy += container->mem_read_burst_energy_nJ;
	energy += container->mem_write_burst_energy_nJ;
	energy += container->mem_background_energy_nJ;
	for(i = 0; i < PA_NUM_NET_IFACE; i++)
		energy += container->net_energy_nJ[i];

	offset += sprintf(buffer + offset, ",%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
		energy,
		container->cpu_leakage_energy_nJ,
		container->cpu_background_energy_nJ,
		container->cpu_dynamic_energy_nJ,

		container->mem_refresh_energy_nJ,
		container->mem_activate_energy_nJ,
		container->mem_precharge_energy_nJ,
		container->mem_read_burst_energy_nJ,
		container->mem_write_burst_energy_nJ,
		container->mem_background_energy_nJ
	);

	offset += sprintf(buffer + offset, ",");
	for(i = 0; i < PA_NUM_NET_IFACE; i++) {
		offset += sprintf(buffer + offset, "%llu", container->net_energy_nJ[i]);
		if(i < PA_NUM_NET_IFACE - 1)
			offset += sprintf(buffer + offset, ",");
	}
}

void print_simulation_stats(struct task_struct *task)
{
	char buffer[512];
	struct pa_base_stats base_diff;
	struct statistics diff;

	if (pa_simulation_started == 0)
		return;

	/* Print main statistics */
	append_sim_heading(buffer, task, PA_LOG_SIM_STATS);

	diff_statistics(&task->pa.stats, &task->pa.sim_start, &diff);
	append_sim_stats(buffer, &diff);

	diff_base_stats(&task->pa.base_stats, &task->pa.base_sim_start, &base_diff);
	append_sim_base_stats(buffer, &base_diff);

	append_sim_energy_container(buffer, &task->pa.energy);

	printk(KERN_INFO "%s\n", buffer);

#ifdef CONFIG_POWER_AGILE_INEFFICIENCY_CONTROLLER
	/* Print tuning stats */
	memset(buffer, 0, sizeof buffer);
	append_sim_heading(buffer, task, PA_LOG_TUNE_STATS);

	append_sim_stats(buffer, &task->pa.tune_stats);
	append_sim_base_stats(buffer, &task->pa.base_tune_stats);
	append_sim_energy_container(buffer, &task->pa.tuning.tuning_energy);
	printk(KERN_INFO "%s\n", buffer);
#endif
}
#endif	/* CONFIG_POWER_AGILE_SIM */

#ifdef CONFIG_POWER_AGILE_TASK_STATS_EXIT
void printk_power_agile_task_stats(struct task_struct *task) {
	char buffer[512];
	int offset = 0;
	offset += sprintf(buffer + offset, "%llu %llu %llu %llu %llu %llu",
			task->pa.base_stats.instructions, task->pa.base_stats.cycles,
			task->pa.base_stats.user_instructions, task->pa.base_stats.kernel_instructions,
			task->pa.base_stats.user_cycles, task->pa.base_stats.kernel_cycles
			);
	offset += sprintf(buffer + offset, " %llu %llu %llu",
			task->pa.stats.cpu_busy_cycles,
			task->pa.stats.cpu_idle_time_ns,
			task->pa.stats.cpu_total_time_ns
		);
	offset += sprintf(buffer + offset, " %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			task->pa.stats.mem_busy_time_ns,
			task->pa.stats.mem_idle_time_ns,
			task->pa.stats.mem_activate_count,
			task->pa.stats.mem_precharge_count,
			task->pa.stats.mem_read_count,
			task->pa.stats.mem_write_count,
			task->pa.stats.mem_precharge_time_ns,
			task->pa.stats.mem_active_time_ns,
			task->pa.stats.mem_refresh_count,
			task->pa.stats.mem_active_idle_overlap_time_ns,
			task->pa.stats.mem_precharge_idle_overlap_time_ns
			);
	pr_debug("PID :%05d\n%s\n", task->pid, buffer);
}
#endif /* CONFIG_POWER_AGILE_TASK_STATS_EXIT */

#ifdef CONFIG_RATE_LIMITING
int rate_limiting_base_energy = 50000;
int rate_limiting_prio_step = 5000;
int rate_limiting_refill_per_ms = 100;

u64 rate_limiting_get_replenish_time(struct task_struct *task, s64 energy_diff) {
/*
	return (rate_limiting_base_energy + (abs(100 - task->prio) *
				rate_limiting_prio_step));
*/
	return div_u64(abs(energy_diff) + (task->pa.rate_lim_refill_rate - 1), task->pa.rate_lim_refill_rate);
}

u64 rate_limiting_get_replenish_energy(struct task_struct *task, u64 time) {
	u64 replenish_energy = 0;
	u64 remaining_energy = 0;

	// Compute how much to replenish based on time since last schedule
	replenish_energy = div_u64((time * task->pa.rate_lim_refill_rate), 1000000);

	// We only use at most 'default_energy'..to prevent hoarding
	replenish_energy = replenish_energy > task->pa.rate_lim_default_energy ?
		task->pa.rate_lim_default_energy : replenish_energy;

	return remaining_energy + replenish_energy;
}

#endif

#ifdef CONFIG_QEMU
unsigned int cpufreq_get_unlocked(unsigned int cpu) {
	return 0;
}

unsigned int memfreq_get() {
	return 0;
}

int gem5_energy_ctrl_get_performance(u32 clock_domain, u32 *freq) {
	*freq = 1000000;
	return 0;
}

int gem5_energy_ctrl_set_performance(u32 clock_domain, u32 freq) {
	return 0;
}

inline int freq_KHz_to_MHz(int freq) {
	return (freq + 500) / 1000;
}

unsigned int memfreq_get_unlocked() {
	return 0;
}

void cpufreq_set_rate(unsigned int freq, unsigned int cpu, int use_locks) {
}

void memfreq_set_rate(unsigned int freq, int use_locks) {
}

int pa_update_cpu_freq(int cpu, u32 freq) {
	return 0;
}

int pa_update_mem_freq(u32 freq) {
	return 0;
}

struct cpufreq_frequency_table *cpufreq_frequency_get_table(unsigned int cpu) {
	return NULL;
}

#endif

