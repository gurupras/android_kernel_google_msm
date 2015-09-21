#ifndef _RATE_LIMITING_H
#define _RATE_LIMITING_H

#include <linux/types.h>

struct task_struct;

#ifdef CONFIG_RATE_LIMITING
/* We don't really use the following two variables.
 * They currently exist only to maintain a default setting
 */
extern int rate_limiting_base_energy;
extern int rate_limiting_prio_step;


extern int rate_limiting_refill_per_ms;

u64 rate_limiting_get_replenish_time(struct task_struct *task, s64 energy_diff);
u64 rate_limiting_get_replenish_energy(struct task_struct *task, u64 time);

#endif /* CONFIG_RATE_LIMITING */

#endif /* _RATE_LIMITING_H */
