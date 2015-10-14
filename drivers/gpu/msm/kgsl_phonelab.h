#ifndef __KGSL_PHONELAB_H
#define __KGSL_PHONELAB_H

#include <linux/phonelab.h>

// PhoneLab tags
#define PHONELAB_TAG_GPU "GPU-Utilization-QoE"

// KGSL States
#define NUM_STATES 9
static char *state_desc[NUM_STATES] = {"None", "Init", "Active", "Nap", "Sleep", "Slumber", "Suspend", "Hung", "Slumber"};

// Logging functions

/*
 * Phonelab logging function for logging state changes
 */
static inline void phonelab_log_state_change(unsigned int state)
{
    /**
    * PhoneLab
    *
    * {
    * "Category": "GPU",
    * "SubCategory": "Utilization",
    * "Tag": "GPU-Utilization-QoE",
    * "Action": "StateChange",
    * "Description": "GPU State changes.  E.g. nap, slumber, active."
    * }
    */
    
    int count, cur;
    cur = state;
    count = 0;
    
    while (cur)
    {
        cur = cur >> 1;
        count++;
    }

    if (count < NUM_STATES)
        alog_i(PHONELAB_TAG_GPU, "{\"Action\":\"StateChange\",\"NewState\":\"%s\"}", state_desc[count]);
}

/*
 * Phonelab logging function for busy time
 */
static inline void phonelab_log_busy_time(unsigned on_time, unsigned elapsed)
{
    /**
    * PhoneLab
    *
    * {
    * "Category": "GPU",
    * "SubCategory": "Utilization",
    * "Tag": "GPU-Utilization-QoE",
    * "Action": "BusyTime",
    * "Description": "Busy time and elapased time since the last time the statistics were updated."
    * }
    */
    alog_i(PHONELAB_TAG_GPU, "{\"Action\":\"BusyTime\",\"OnTime\":%u,\"Elapsed\":%u}", on_time, elapsed);
}

/*
 * Phonelab logging function for logging gpu commands
 */
static inline void phonelab_log_commands(unsigned count)
{
    /**
    * PhoneLab
    *
    * {
    * "Category": "GPU",
    * "SubCategory": "Utilization",
    * "Tag": "GPU-Utilization-QoE",
    * "Action": "Commands",
    * "Description": "Busy time and elapased time since the last time the statistics were updated."
    * }
    */
    alog_i(PHONELAB_TAG_GPU, "{\"Action\":\"Commands\",\"Count\":%u}", count);
}

#endif /* __KGSL_PHONELAB_H */

