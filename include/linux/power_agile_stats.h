#ifndef _POWER_AGILE_STATS_H
#define _POWER_AGILE_STATS_H

struct pa_base_stats {
	u64 user_instructions;
	u64 kernel_instructions;
	u64 user_cycles;
	u64 kernel_cycles;
	u64 instructions;
	u64 cycles;
};
/*
 * Per-interface statistics.  These aren't necessary
 * but are nice to have.
 */
struct pa_net_stats {
	int ifindex;
	u64 total_bytes;
	u64 total_energy;
};

struct pa_cpu_stats {
	u64 busy_cycles;
	u64 l1l2_stall_cycles;
	u64 quiesce_time_ns;
	u64 dram_stall_time_ns;
	u32 freq;
};

struct pa_mem_stats {
	u64 busy_time_ns;
	u64 idle_time_ns;
	u64 activate_count;
	u64 precharge_count;
	u64 read_count;
	u64 write_count;
	u64 precharge_time_ns;
	u64 active_time_ns;
	u64 refresh_count;
	u64 active_idle_overlap_time_ns;
	u64 precharge_idle_overlap_time_ns;
	u32 freq;
};

#endif /* _POWER_AGILE_STATS_H */
