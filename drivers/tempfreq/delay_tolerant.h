#ifndef __TEMPFREQ_DELAY_TOLERANT_H_
#define __TEMPFREQ_DELAY_TOLERANT_H_

void make_delay_tolerant(pid_t pid, u64 duration_ms);
void countdown_delay_tolerant_timers(void);
u64 time_since_epoch_ns(void);

#endif	/* __TEMPFREQ_DELAY_TOLERANT_H_ */
