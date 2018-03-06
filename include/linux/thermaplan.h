#ifndef __THERMAPLAN_H_
#define __THERMAPLAN_H_

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/async.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/module.h>

struct acpuclock_attr {
	struct attribute attr;
	ssize_t (*show)(char *);
	ssize_t (*store)(const char *, size_t count);
};
#define acpuclock_to_attr(a) container_of(a, struct acpuclock_attr, attr)

extern int thermaplan_sysfs_initialized;
#ifdef CONFIG_THERMAPLAN_BTM_USERSPACE_UNDERVOLT
struct vdd_data {
	int vdd_mem;
	int vdd_dig;
	int vdd_core;
	int ua_core;
};

extern struct drv_data *acpuclock_drv;
extern int acpuclock_ready;
extern int undervolt_mv;
extern struct acpu_level *undervolt_tbl;
extern struct acpuclock_attr should_undervolt_attr;
#ifdef CONFIG_THERMAPLAN_BTM_TRACK_UNDERVOLT_STATS
extern struct acpuclock_attr stats_attr;
#endif
#endif

/*
 * struct regulator
 *
 * One for each consumer device.
 */
struct regulator {
	struct device *dev;
	struct list_head list;
	int uA_load;
	int min_uV;
	int max_uV;
	int enabled;
	char *supply_name;
	struct device_attribute dev_attr;
	struct regulator_dev *rdev;
	struct dentry *debugfs;
};

void force_regulator_cpu(int cpu, struct acpu_level *tgt, struct vdd_data *vdd_data);
int calculate_vdd_mem(const struct acpu_level *tgt);
int calculate_vdd_dig(const struct acpu_level *tgt);
int calculate_vdd_core(const struct acpu_level *tgt);

int _regulator_set_voltage(struct regulator *regulator, int min_uV, int max_uV, bool should_lock);
int _regulator_set_optimum_mode(struct regulator *regulator, int uA_load, bool should_lock);

#ifdef CONFIG_THERMAPLAN_BTM_PER_PROCESS_VOLTAGE
extern void ctx_switch_undervolt(int);
#endif

#endif	/* __THERMAPLAN_H_ */

