#ifndef __TEMPFREQ_HOTPLUG_H_
#define __TEMPFREQ_HOTPLUG_H_

#include <linux/kernel.h>
#include <linux/workqueue.h>

enum {
	PHONELAB_TEMPFREQ_NO_HOTPLUG_DRIVER,
	PHONELAB_TEMPFREQ_TASK_HOTPLUG_DRIVER,
	PHONELAB_TEMPFREQ_AUTOSMP_HOTPLUG_DRIVER,
	PHONELAB_TEMPFREQ_NR_HOTPLUG_DRIVERS
};

struct hotplug_driver {
	int id;
	char name[32];
	void (*hotplug_work_fn) (struct work_struct *);
};

struct hotplug_drivers_list {
	struct list_head list;
	struct hotplug_driver *driver;
};
extern struct hotplug_drivers_list hotplug_drivers_list;
#endif	/* __TEMPFREQ_HOTPLUG_H_ */
