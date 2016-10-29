#ifndef __TEMPFREQ_DELAY_TOLERANT_H_
#define __TEMPFREQ_DELAY_TOLERANT_H_

struct task_struct;
void make_delay_tolerant(struct task_struct *tsk);
void countdown_delay_tolerant_timers(void);
u64 time_since_epoch_ms(void);

#endif	/* __TEMPFREQ_DELAY_TOLERANT_H_ */
