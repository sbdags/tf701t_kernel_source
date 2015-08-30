/*
 * arch/arm/mach-tegra/cpu-tegra.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Based on arch/arm/plat-omap/cpu-omap.c, (C) 2005 Nokia Corporation
 *
 * Copyright (C) 2010-2013 NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/cpu.h>
#include <asm/mach-types.h>
#include <mach/clk.h>
#include <mach/edp.h>

#include "clock.h"
#include "cpu-tegra.h"
#include "dvfs.h"
#include "pm.h"

#define SYSTEM_NORMAL_MODE	(0)
#define SYSTEM_BALANCE_MODE	(1)
#define SYSTEM_PWRSAVE_MODE	(2)
#define SYSTEM_VIDEO_MODE	(3)
#define SYSTEM_BROWSER_MODE	(4)
#define SYSTEM_MODE_END 		(SYSTEM_BROWSER_MODE + 1)

static  int system_mode = 0;
unsigned int power_mode_table[SYSTEM_MODE_END] = {1912500, 1708500, 1224000, 1224000, 1224000};
static unsigned int pwr_cap_limits[4] = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
static unsigned int pwr_save = 0;
static unsigned int pwr_save_freq = 1224000;

/* tegra throttling and edp governors require frequencies in the table
   to be in ascending order */
static struct cpufreq_frequency_table *freq_table;
static unsigned int freq_table_size=0;
static struct clk *cpu_clk;
static struct clk *emc_clk;

static unsigned long policy_max_speed[CONFIG_NR_CPUS];
static unsigned long target_cpu_speed[CONFIG_NR_CPUS];
static DEFINE_MUTEX(tegra_cpu_lock);
static bool is_suspended;
static int suspend_index;
static unsigned int volt_capped_speed;
static bool force_policy_max;

extern bool low_battery_flag;
bool display_on_flag = 0;

static int force_policy_max_set(const char *arg, const struct kernel_param *kp)
{
	int ret;
	bool old_policy = force_policy_max;

	mutex_lock(&tegra_cpu_lock);

	ret = param_set_bool(arg, kp);
	if ((ret == 0) && (old_policy != force_policy_max))
		tegra_cpu_set_speed_cap_locked(NULL);

	mutex_unlock(&tegra_cpu_lock);
	return ret;
}

static int force_policy_max_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops policy_ops = {
	.set = force_policy_max_set,
	.get = force_policy_max_get,
};
module_param_cb(force_policy_max, &policy_ops, &force_policy_max, 0644);

static int system_mode_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(arg, kp);
	if (ret == 0)
	{
		printk("system_mode_set system_mode=%u\n",system_mode);

		if((system_mode < SYSTEM_NORMAL_MODE) || (system_mode > SYSTEM_BROWSER_MODE))
		{
			system_mode = SYSTEM_NORMAL_MODE;
		}

		tegra_cpu_set_speed_cap(NULL);
	}
	return ret;
}

static int system_mode_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_int(buffer, kp);
}

static struct kernel_param_ops system_mode_ops = {
	.set = system_mode_set,
	.get = system_mode_get,
};
module_param_cb(system_mode, &system_mode_ops, &system_mode, 0644);

static int pwr_save_freq_set(const char *arg, const struct kernel_param *kp)
{
	int ret = 0;

	ret = param_set_int(arg, kp);
	if (ret == 0)
	{
		printk("pwr_save_freq_set pwr_save=%u\n", power_mode_table[SYSTEM_PWRSAVE_MODE]);
	}
	return ret;
}

static int pwr_save_freq_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_int(buffer, kp);
}

static struct kernel_param_ops tegra_pwr_save_freq_ops = {
	.set = pwr_save_freq_set,
	.get = pwr_save_freq_get,
};
module_param_cb( pwr_save_freq,&tegra_pwr_save_freq_ops, &pwr_save_freq, 0644);

static int pwr_save_state_set(const char *arg, const struct kernel_param *kp)
{
	int ret = 0;

	ret = param_set_int(arg, kp);

	if (ret == 0)
	{
		printk("pwr_save_state_set pwr_save=%u\n",pwr_save);
	}
	return ret;
}

static int pwr_save_state_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_int(buffer, kp);
}

static struct kernel_param_ops tegra_pwr_save_ops = {
	.set = pwr_save_state_set,
	.get = pwr_save_state_get,
};
module_param_cb(enable_pwr_save, &tegra_pwr_save_ops, &pwr_save, 0644);

 unsigned int ASUS_governor_speed(unsigned int requested_speed)
{
	unsigned int new_speed=requested_speed;

	if(machine_is_mozart())
	{
		if(low_battery_flag && (requested_speed > power_mode_table[SYSTEM_PWRSAVE_MODE]))
		{
			new_speed = power_mode_table[SYSTEM_PWRSAVE_MODE];
		}
		else
		{
			if((system_mode == SYSTEM_BALANCE_MODE) && (requested_speed > power_mode_table[SYSTEM_BALANCE_MODE]))
				new_speed = power_mode_table[SYSTEM_BALANCE_MODE];
			else  if((system_mode == SYSTEM_PWRSAVE_MODE) && (requested_speed > power_mode_table[SYSTEM_PWRSAVE_MODE]))
				new_speed = power_mode_table[SYSTEM_PWRSAVE_MODE];
			else  if((system_mode == SYSTEM_NORMAL_MODE) && (requested_speed > power_mode_table[SYSTEM_NORMAL_MODE]))
				new_speed = power_mode_table[SYSTEM_NORMAL_MODE];
			else  if((system_mode == SYSTEM_VIDEO_MODE) && (requested_speed > power_mode_table[SYSTEM_VIDEO_MODE]))
				new_speed = power_mode_table[SYSTEM_VIDEO_MODE];
			else  if((system_mode == SYSTEM_BROWSER_MODE) && (requested_speed > power_mode_table[SYSTEM_BROWSER_MODE]))
				new_speed = power_mode_table[SYSTEM_BROWSER_MODE];
		}
	}
	else
	{

		if((system_mode == SYSTEM_BALANCE_MODE) && (requested_speed > power_mode_table[SYSTEM_BALANCE_MODE]))
			new_speed = power_mode_table[SYSTEM_BALANCE_MODE];
		else  if((system_mode == SYSTEM_PWRSAVE_MODE) && (requested_speed > power_mode_table[SYSTEM_PWRSAVE_MODE]))
			new_speed = power_mode_table[SYSTEM_PWRSAVE_MODE];
		else  if((system_mode == SYSTEM_NORMAL_MODE) && (requested_speed > power_mode_table[SYSTEM_NORMAL_MODE]))
			new_speed = power_mode_table[SYSTEM_NORMAL_MODE];
		else  if((system_mode == SYSTEM_VIDEO_MODE) && (requested_speed > power_mode_table[SYSTEM_VIDEO_MODE]))
			new_speed = power_mode_table[SYSTEM_VIDEO_MODE];
		else  if((system_mode == SYSTEM_BROWSER_MODE) && (requested_speed > power_mode_table[SYSTEM_BROWSER_MODE]))
			new_speed = power_mode_table[SYSTEM_BROWSER_MODE];
	}

	return new_speed;
}

static unsigned int cpu_user_cap;

static inline void _cpu_user_cap_set_locked(void)
{
#ifndef CONFIG_TEGRA_CPU_CAP_EXACT_FREQ
	if (cpu_user_cap != 0) {
		int i;
		for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
			if (freq_table[i].frequency > cpu_user_cap)
				break;
		}
		i = (i == 0) ? 0 : i - 1;
		cpu_user_cap = freq_table[i].frequency;
	}
#endif
	tegra_cpu_set_speed_cap_locked(NULL);
}

void tegra_cpu_user_cap_set(unsigned int speed_khz)
{
	mutex_lock(&tegra_cpu_lock);

	cpu_user_cap = speed_khz;
	_cpu_user_cap_set_locked();

	mutex_unlock(&tegra_cpu_lock);
}

static int cpu_user_cap_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	mutex_lock(&tegra_cpu_lock);

	ret = param_set_uint(arg, kp);
	if (ret == 0)
		_cpu_user_cap_set_locked();

	mutex_unlock(&tegra_cpu_lock);
	return ret;
}

static int cpu_user_cap_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_uint(buffer, kp);
}

static struct kernel_param_ops cap_ops = {
	.set = cpu_user_cap_set,
	.get = cpu_user_cap_get,
};
module_param_cb(cpu_user_cap, &cap_ops, &cpu_user_cap, 0644);

/*static unsigned int user_cap_speed(unsigned int requested_speed)
{
	if ((cpu_user_cap) && (requested_speed > cpu_user_cap))
		return cpu_user_cap;
	return requested_speed;
}*/

#ifdef CONFIG_TEGRA_THERMAL_THROTTLE

static ssize_t show_throttle(struct cpufreq_policy *policy, char *buf)
{
	return sprintf(buf, "%u\n", tegra_is_throttling(NULL));
}

cpufreq_freq_attr_ro(throttle);

static ssize_t show_throttle_count(struct cpufreq_policy *policy, char *buf)
{
	int count;

	tegra_is_throttling(&count);
	return sprintf(buf, "%u\n", count);
}

static struct freq_attr _attr_throttle_count = {
	.attr = {.name = "throttle_count", .mode = 0444, },
	.show = show_throttle_count,
};

static struct attribute *new_attrs[] = {
	&_attr_throttle_count.attr,
	NULL
};

static struct attribute_group stats_attr_grp = {
	.attrs = new_attrs,
	.name = "stats"
};

#endif /* CONFIG_TEGRA_THERMAL_THROTTLE */

#ifdef CONFIG_TEGRA_EDP_LIMITS

static const struct tegra_edp_limits *cpu_edp_limits;
static int cpu_edp_limits_size;

static const unsigned int *system_edp_limits;
static bool system_edp_alarm;

static int edp_thermal_index;
static cpumask_t edp_cpumask;
static unsigned int edp_limit;

unsigned int tegra_get_edp_limit(int *get_edp_thermal_index)
{
	if (get_edp_thermal_index)
		*get_edp_thermal_index = edp_thermal_index;
	return edp_limit;
}

static unsigned int edp_predict_limit(unsigned int cpus)
{
	unsigned int limit = 0;

	BUG_ON(cpus == 0);
	if (cpu_edp_limits) {
		BUG_ON(edp_thermal_index >= cpu_edp_limits_size);
		limit = cpu_edp_limits[edp_thermal_index].freq_limits[cpus - 1];
	}
	if (system_edp_limits && system_edp_alarm)
		limit = min(limit, system_edp_limits[cpus - 1]);

	limit = min(limit, pwr_cap_limits[cpus - 1]);//pwr save

	return limit;
}

/* Must be called while holding cpu_tegra_lock */
static void edp_update_limit(void)
{
	unsigned int limit = edp_predict_limit(cpumask_weight(&edp_cpumask));
	BUG_ON(!mutex_is_locked(&tegra_cpu_lock));
#ifdef CONFIG_TEGRA_EDP_EXACT_FREQ
	edp_limit = limit;
#else
	unsigned int i;
	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table[i].frequency > limit) {
			break;
		}
	}
	BUG_ON(i == 0);	/* min freq above the limit or table empty */
	edp_limit = freq_table[i-1].frequency;
#endif
}

static unsigned int edp_governor_speed(unsigned int requested_speed)
{
	if ((!edp_limit) || (requested_speed <= edp_limit))
		return requested_speed;
	else
		return edp_limit;
}

int tegra_edp_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *max_state)
{
	*max_state = cpu_edp_limits_size - 1;
	return 0;
}

int tegra_edp_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *cur_state)
{
	*cur_state = edp_thermal_index;
	return 0;
}

int tegra_edp_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long cur_state)
{
	mutex_lock(&tegra_cpu_lock);
	edp_thermal_index = cur_state;

	/* Update cpu rate if cpufreq (at least on cpu0) is already started;
	   alter cpu dvfs table for this thermal zone if necessary */
	tegra_cpu_dvfs_alter(edp_thermal_index, &edp_cpumask, true, 0);
	if (target_cpu_speed[0]) {
		edp_update_limit();
		tegra_cpu_set_speed_cap_locked(NULL);
	}
	tegra_cpu_dvfs_alter(edp_thermal_index, &edp_cpumask, false, 0);
	mutex_unlock(&tegra_cpu_lock);

	return 0;
}

static struct thermal_cooling_device_ops tegra_edp_cooling_ops = {
	.get_max_state = tegra_edp_get_max_state,
	.get_cur_state = tegra_edp_get_cur_state,
	.set_cur_state = tegra_edp_set_cur_state,
};

static int __init edp_init(void)
{
	thermal_cooling_device_register(
				"cpu_edp",
				NULL,
				&tegra_edp_cooling_ops);
	return 0;
}
module_init(edp_init);

int tegra_system_edp_alarm(bool alarm)
{
	int ret = -ENODEV;

	mutex_lock(&tegra_cpu_lock);
	system_edp_alarm = alarm;

	/* Update cpu rate if cpufreq (at least on cpu0) is already started
	   and cancel emergency throttling after either edp limit is applied
	   or alarm is canceled */
	if (target_cpu_speed[0]) {
		edp_update_limit();
		ret = tegra_cpu_set_speed_cap_locked(NULL);
	}
	if (!ret || !alarm)
		tegra_edp_throttle_cpu_now(0);

	mutex_unlock(&tegra_cpu_lock);

	return ret;
}

bool tegra_cpu_edp_favor_up(unsigned int n, int mp_overhead)
{
	unsigned int current_limit, next_limit;

	if (n == 0)
		return true;

	if (n >= ARRAY_SIZE(cpu_edp_limits->freq_limits))
		return false;

	current_limit = edp_predict_limit(n);
	next_limit = edp_predict_limit(n + 1);

	return ((next_limit * (n + 1)) >=
		(current_limit * n * (100 + mp_overhead) / 100));
}

bool tegra_cpu_edp_favor_down(unsigned int n, int mp_overhead)
{
	unsigned int current_limit, next_limit;

	if (n <= 1)
		return false;

	if (n > ARRAY_SIZE(cpu_edp_limits->freq_limits))
		return true;

	current_limit = edp_predict_limit(n);
	next_limit = edp_predict_limit(n - 1);

	return ((next_limit * (n - 1) * (100 + mp_overhead) / 100)) >
		(current_limit * n);
}

static int tegra_cpu_edp_notify(
	struct notifier_block *nb, unsigned long event, void *hcpu)
{
	int ret = 0;
	unsigned int cpu_speed, new_speed;
	int cpu = (long)hcpu;

	switch (event) {
	case CPU_UP_PREPARE:
		mutex_lock(&tegra_cpu_lock);
		cpu_set(cpu, edp_cpumask);
		edp_update_limit();

		cpu_speed = tegra_getspeed(0);
		new_speed = edp_governor_speed(cpu_speed);
		if (new_speed < cpu_speed) {
			ret = tegra_cpu_set_speed_cap_locked(NULL);
			printk(KERN_DEBUG "cpu-tegra:%sforce EDP limit %u kHz"
				"\n", ret ? " failed to " : " ", new_speed);
		}
		if (!ret)
			ret = tegra_cpu_dvfs_alter(
				edp_thermal_index, &edp_cpumask, false, event);
		if (ret) {
			cpu_clear(cpu, edp_cpumask);
			edp_update_limit();
		}
		mutex_unlock(&tegra_cpu_lock);
		break;
	case CPU_DEAD:
		mutex_lock(&tegra_cpu_lock);
		cpu_clear(cpu, edp_cpumask);
		tegra_cpu_dvfs_alter(
			edp_thermal_index, &edp_cpumask, true, event);
		edp_update_limit();
		tegra_cpu_set_speed_cap_locked(NULL);
		mutex_unlock(&tegra_cpu_lock);
		break;
	}
	return notifier_from_errno(ret);
}

static struct notifier_block tegra_cpu_edp_notifier = {
	.notifier_call = tegra_cpu_edp_notify,
};

static void tegra_cpu_edp_init(bool resume)
{
	tegra_get_system_edp_limits(&system_edp_limits);
	tegra_get_cpu_edp_limits(&cpu_edp_limits, &cpu_edp_limits_size);

	if (!(cpu_edp_limits || system_edp_limits)) {
		if (!resume)
			pr_info("cpu-tegra: no EDP table is provided\n");
		return;
	}

	/* FIXME: use the highest temperature limits if sensor is not on-line?
	 * If thermal zone is not set yet by the sensor, edp_thermal_index = 0.
	 * Boot frequency allowed SoC to get here, should work till sensor is
	 * initialized.
	 */
	edp_cpumask = *cpu_online_mask;
	edp_update_limit();

	if (!resume) {
		register_hotcpu_notifier(&tegra_cpu_edp_notifier);
		pr_info("cpu-tegra: init EDP limit: %u MHz\n", edp_limit/1000);
	}
}

static void tegra_cpu_edp_exit(void)
{
	if (!(cpu_edp_limits || system_edp_limits))
		return;

	unregister_hotcpu_notifier(&tegra_cpu_edp_notifier);
}

static int pwr_cap_limit_set(const char *arg, const struct kernel_param *kp)
{
	unsigned int old_freq = *(unsigned int *)(kp->arg);
	unsigned int new_freq;
	int ret;

	mutex_lock(&tegra_cpu_lock);
	ret = param_set_uint(arg, kp);

	if (ret == 0) {
		new_freq = *(unsigned int *)(kp->arg);
		if (new_freq != old_freq) {
			edp_update_limit();
			tegra_cpu_set_speed_cap(NULL);
		}
}

	mutex_unlock(&tegra_cpu_lock);
	return ret;
}
static int pwr_cap_limit_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_uint(buffer, kp);
}

static struct kernel_param_ops pwr_cap_ops = {
	.set = pwr_cap_limit_set,
	.get = pwr_cap_limit_get,
};
module_param_cb(pwr_cap_limit_1, &pwr_cap_ops, &pwr_cap_limits[0], 0644);
module_param_cb(pwr_cap_limit_2, &pwr_cap_ops, &pwr_cap_limits[1], 0644);
module_param_cb(pwr_cap_limit_3, &pwr_cap_ops, &pwr_cap_limits[2], 0644);
module_param_cb(pwr_cap_limit_4, &pwr_cap_ops, &pwr_cap_limits[3], 0644);

#ifdef CONFIG_DEBUG_FS

static int pwr_mode_table_debugfs_show(struct seq_file *s, void *data)
{
	seq_printf(s, "-- CPU power mode table --\n");
	seq_printf(s, "Browser = %u \n Video = %u \n Power Saving=%u \n Balanced=%u \n Normal=%u \n \n",
			   power_mode_table[4],
			   power_mode_table[3],
			   power_mode_table[2],
			   power_mode_table[1],
			   power_mode_table[0]);
	return 0;
}

static int pwr_mode_table_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, pwr_mode_table_debugfs_show, inode->i_private);
}

static const struct file_operations pwr_mode_table_debugfs_fops = {
	.open		= pwr_mode_table_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int system_edp_alarm_get(void *data, u64 *val)
{
	*val = (u64)system_edp_alarm;
	return 0;
}
static int system_edp_alarm_set(void *data, u64 val)
{
	if (val > 1) {	/* emulate emergency throttling */
		tegra_edp_throttle_cpu_now(val);
		return 0;
	}
	return tegra_system_edp_alarm((bool)val);
}
DEFINE_SIMPLE_ATTRIBUTE(system_edp_alarm_fops,
			system_edp_alarm_get, system_edp_alarm_set, "%llu\n");

static int __init tegra_edp_debug_init(struct dentry *cpu_tegra_debugfs_root)
{
	if (!debugfs_create_file("edp_alarm", 0644, cpu_tegra_debugfs_root,
				 NULL, &system_edp_alarm_fops))
		return -ENOMEM;

	return 0;
}
#endif

#else	/* CONFIG_TEGRA_EDP_LIMITS */
#define edp_governor_speed(requested_speed) (requested_speed)
#define tegra_cpu_edp_init(resume)
#define tegra_cpu_edp_exit()
#define tegra_edp_debug_init(cpu_tegra_debugfs_root) (0)
#endif	/* CONFIG_TEGRA_EDP_LIMITS */

#ifdef CONFIG_DEBUG_FS

static struct dentry *cpu_tegra_debugfs_root;

static int __init tegra_cpu_debug_init(void)
{
	cpu_tegra_debugfs_root = debugfs_create_dir("cpu-tegra", 0);

	if (!cpu_tegra_debugfs_root)
		return -ENOMEM;

	if (tegra_edp_debug_init(cpu_tegra_debugfs_root))
		goto err_out;

	if (!debugfs_create_file("pwr_mode_table", 0644, cpu_tegra_debugfs_root, NULL, &pwr_mode_table_debugfs_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(cpu_tegra_debugfs_root);
	return -ENOMEM;
}

static void __exit tegra_cpu_debug_exit(void)
{
	debugfs_remove_recursive(cpu_tegra_debugfs_root);
}

late_initcall(tegra_cpu_debug_init);
module_exit(tegra_cpu_debug_exit);
#endif /* CONFIG_DEBUG_FS */

static int tegra_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

unsigned int tegra_getspeed(unsigned int cpu)
{
	unsigned long rate;

	if (cpu >= CONFIG_NR_CPUS)
		return 0;

	rate = clk_get_rate(cpu_clk) / 1000;
	return rate;
}

int tegra_update_cpu_speed(unsigned long rate)
{
	int ret = 0;
	struct cpufreq_freqs freqs;

	freqs.old = tegra_getspeed(0);
	freqs.new = rate;

	rate = clk_round_rate(cpu_clk, rate * 1000);
	if (!IS_ERR_VALUE(rate))
		freqs.new = rate / 1000;

	if (freqs.old == freqs.new)
		return ret;

	/*
	 * Vote on memory bus frequency based on cpu frequency
	 * This sets the minimum frequency, display or avp may request higher
	 */
	if (freqs.old < freqs.new) {
		ret = tegra_update_mselect_rate(freqs.new);
		if (ret) {
			pr_err("cpu-tegra: Failed to scale mselect for cpu"
			       " frequency %u kHz\n", freqs.new);
			return ret;
		}
		ret = clk_set_rate(emc_clk, tegra_emc_to_cpu_ratio(freqs.new));
		if (ret) {
			pr_err("cpu-tegra: Failed to scale emc for cpu"
			       " frequency %u kHz\n", freqs.new);
			return ret;
		}
	}

	for_each_online_cpu(freqs.cpu)
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

#ifdef CONFIG_CPU_FREQ_DEBUG
	printk(KERN_DEBUG "cpufreq-tegra: transition: %u --> %u\n",
	       freqs.old, freqs.new);
#endif

	ret = clk_set_rate(cpu_clk, freqs.new * 1000);
	if (ret) {
		pr_err("cpu-tegra: Failed to set cpu frequency to %d kHz\n",
			freqs.new);
		return ret;
	}

	for_each_online_cpu(freqs.cpu)
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	if (freqs.old > freqs.new) {
		clk_set_rate(emc_clk, tegra_emc_to_cpu_ratio(freqs.new));
		tegra_update_mselect_rate(freqs.new);
	}

	return 0;
}

unsigned int tegra_count_slow_cpus(unsigned long speed_limit)
{
	unsigned int cnt = 0;
	int i;

	for_each_online_cpu(i)
		if (target_cpu_speed[i] <= speed_limit)
			cnt++;
	return cnt;
}

unsigned int tegra_get_slowest_cpu_n(void) {
	unsigned int cpu = nr_cpu_ids;
	unsigned long rate = ULONG_MAX;
	int i;

	for_each_online_cpu(i)
		if ((i > 0) && (rate > target_cpu_speed[i])) {
			cpu = i;
			rate = target_cpu_speed[i];
		}
	return cpu;
}

unsigned long tegra_cpu_lowest_speed(void) {
	unsigned long rate = ULONG_MAX;
	int i;

	for_each_online_cpu(i)
		rate = min(rate, target_cpu_speed[i]);
	return rate;
}

unsigned long tegra_cpu_highest_speed(void) {
	unsigned long policy_max = ULONG_MAX;
	unsigned long rate = 0;
	int i;

	for_each_online_cpu(i) {
		rate = max(rate, target_cpu_speed[i]);
	}
	rate = min(rate, policy_max);
	return rate;
}

void tegra_cpu_set_volt_cap(unsigned int cap)
{
	mutex_lock(&tegra_cpu_lock);
	if (cap != volt_capped_speed) {
		volt_capped_speed = cap;
		tegra_cpu_set_speed_cap_locked(NULL);
	}
	mutex_unlock(&tegra_cpu_lock);
	if (cap)
		pr_debug("tegra_cpu:volt limit to %u Khz\n", cap);
	else
		pr_debug("tegra_cpu:volt limit removed\n");
}

static unsigned int volt_cap_speed(unsigned int requested_speed)
{
	if (volt_capped_speed && requested_speed > volt_capped_speed)
		return volt_capped_speed;
	return requested_speed;
}

/* Must be called with tegra_cpu_lock held */
int tegra_cpu_set_speed_cap_locked(unsigned int *speed_cap)
{
	int ret = 0;
	unsigned int new_speed = tegra_cpu_highest_speed();
	BUG_ON(!mutex_is_locked(&tegra_cpu_lock));
#ifdef CONFIG_TEGRA_EDP_LIMITS
	edp_update_limit();
#endif

	if (is_suspended)
		return -EBUSY;

	new_speed = ASUS_governor_speed(new_speed);
	new_speed = tegra_throttle_governor_speed(new_speed);
	new_speed = edp_governor_speed(new_speed);
	//new_speed = user_cap_speed(new_speed);
	new_speed = volt_cap_speed(new_speed);
	if (speed_cap)
		*speed_cap = new_speed;

	ret = tegra_update_cpu_speed(new_speed);
	if (ret == 0)
		tegra_auto_hotplug_governor(new_speed, false);
	return ret;
}

int tegra_cpu_set_speed_cap(unsigned int *speed_cap)
{
	int ret;
	mutex_lock(&tegra_cpu_lock);
	ret = tegra_cpu_set_speed_cap_locked(speed_cap);
	mutex_unlock(&tegra_cpu_lock);
	return ret;
}


int tegra_suspended_target(unsigned int target_freq)
{
	unsigned int new_speed = target_freq;

	if (!is_suspended)
		return -EBUSY;

	/* apply only "hard" caps */
	new_speed = tegra_throttle_governor_speed(new_speed);
	new_speed = edp_governor_speed(new_speed);

	return tegra_update_cpu_speed(new_speed);
}

static int tegra_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	int idx;
	unsigned int freq;
	unsigned int new_speed;
	int ret = 0;
	int cpu = 0;

	mutex_lock(&tegra_cpu_lock);

	ret = cpufreq_frequency_table_target(policy, freq_table, target_freq,
		relation, &idx);
	if (ret)
		goto _out;

	if((ret >= 0) && (idx >= 0) && (idx < freq_table_size))
	{
		freq = freq_table[idx].frequency;
	}
	else
	{
		printk("[warning] tegra_target ret=%d idx=%d cpu=%u\n", ret, idx, policy->cpu);
		goto _out;
	}

	cpu = policy->cpu;

	if( cpu >= 0 && cpu < nr_cpu_ids)
	{
		target_cpu_speed[cpu] = freq;
		ret = tegra_cpu_set_speed_cap_locked(&new_speed);
	}
	else
	{
		printk("[warning] tegra_target cpu=%u\n",policy->cpu);
	}

_out:
	mutex_unlock(&tegra_cpu_lock);

	return ret;
}


static int tegra_pm_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	mutex_lock(&tegra_cpu_lock);
	if (event == PM_SUSPEND_PREPARE) {
		is_suspended = true;
		pr_info("Tegra cpufreq suspend: setting frequency to %d kHz\n",
			freq_table[suspend_index].frequency);
		tegra_update_cpu_speed(freq_table[suspend_index].frequency);
		tegra_auto_hotplug_governor(
			freq_table[suspend_index].frequency, true);
	} else if (event == PM_POST_SUSPEND) {
		unsigned int freq;
		is_suspended = false;
		tegra_cpu_edp_init(true);
		tegra_cpu_set_speed_cap_locked(&freq);
		pr_info("Tegra cpufreq resume: restoring frequency to %d kHz\n",
			freq);
	}
	mutex_unlock(&tegra_cpu_lock);

	return NOTIFY_OK;
}

static struct notifier_block tegra_cpu_pm_notifier = {
	.notifier_call = tegra_pm_notify,
};

void rebuild_max_freq_table(unsigned int max_rate)
{
	power_mode_table[SYSTEM_NORMAL_MODE] = max_rate;
	power_mode_table[SYSTEM_BALANCE_MODE] = freq_table[freq_table_size - 3].frequency;
	power_mode_table[SYSTEM_PWRSAVE_MODE] = pwr_save_freq;
	power_mode_table[SYSTEM_VIDEO_MODE] = pwr_save_freq;
	power_mode_table[SYSTEM_BROWSER_MODE] = pwr_save_freq;
}

static int tegra_cpu_init(struct cpufreq_policy *policy)
{
	int idx, ret;
	unsigned int freq;

	if (policy->cpu >= CONFIG_NR_CPUS)
		return -EINVAL;

	cpu_clk = clk_get_sys(NULL, "cpu");
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

	emc_clk = clk_get_sys("cpu", "emc");
	if (IS_ERR(emc_clk)) {
		clk_put(cpu_clk);
		return PTR_ERR(emc_clk);
	}

	clk_prepare_enable(emc_clk);
	clk_prepare_enable(cpu_clk);

	cpufreq_frequency_table_cpuinfo(policy, freq_table);
	cpufreq_frequency_table_get_attr(freq_table, policy->cpu);

	/* clip boot frequency to table entry */
	freq = tegra_getspeed(policy->cpu);
	ret = cpufreq_frequency_table_target(policy, freq_table, freq,
		CPUFREQ_RELATION_H, &idx);
	if (!ret && (freq != freq_table[idx].frequency)) {
		ret = tegra_update_cpu_speed(freq_table[idx].frequency);
		if (!ret)
			freq = freq_table[idx].frequency;
	}
	policy->cur = freq;
	target_cpu_speed[policy->cpu] = policy->cur;

	/* FIXME: what's the actual transition time? */
	policy->cpuinfo.transition_latency = 300 * 1000;

	policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;
	cpumask_copy(policy->related_cpus, cpu_possible_mask);

	return 0;
}

static int tegra_cpu_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_cpuinfo(policy, freq_table);
	clk_disable_unprepare(emc_clk);
	clk_put(emc_clk);
	clk_put(cpu_clk);
	return 0;
}

static int tegra_cpufreq_policy_notifier(
	struct notifier_block *nb, unsigned long event, void *data)
{
#ifdef CONFIG_TEGRA_THERMAL_THROTTLE
	static int once = 1;
#endif
	int i, ret;
	struct cpufreq_policy *policy = data;

	if (event == CPUFREQ_NOTIFY) {
		ret = cpufreq_frequency_table_target(policy, freq_table,
			policy->max, CPUFREQ_RELATION_H, &i);
		policy_max_speed[policy->cpu] =
			ret ? policy->max : freq_table[i].frequency;

#ifdef CONFIG_TEGRA_THERMAL_THROTTLE
		if (once &&
		    sysfs_merge_group(policy->kobj, &stats_attr_grp) == 0)
			once = 0;
#endif
	}
	return NOTIFY_OK;
}

static struct notifier_block tegra_cpufreq_policy_nb = {
	.notifier_call = tegra_cpufreq_policy_notifier,
};

static struct freq_attr *tegra_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
#ifdef CONFIG_TEGRA_THERMAL_THROTTLE
	&throttle,
#endif
	NULL,
};

static struct cpufreq_driver tegra_cpufreq_driver = {
	.verify		= tegra_verify_speed,
	.target		= tegra_target,
	.get		= tegra_getspeed,
	.init		= tegra_cpu_init,
	.exit		= tegra_cpu_exit,
	.name		= "tegra",
	.attr		= tegra_cpufreq_attr,
};

static int __init tegra_cpufreq_init(void)
{
	int i, ret = 0;

	struct tegra_cpufreq_table_data *table_data =
		tegra_cpufreq_table_get();
	if (IS_ERR_OR_NULL(table_data))
		return -EINVAL;

	suspend_index = table_data->suspend_index;

	ret = tegra_throttle_init(&tegra_cpu_lock);
	if (ret)
		return ret;

	ret = tegra_auto_hotplug_init(&tegra_cpu_lock);
	if (ret)
		return ret;

	freq_table = table_data->freq_table;

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
	{
		freq_table_size++;
	}

	rebuild_max_freq_table( freq_table[freq_table_size - 1].frequency);
	printk("tegra_cpufreq_init freq_table_size=%u max rate=%u\n", freq_table_size, freq_table[freq_table_size-1].frequency);

	mutex_lock(&tegra_cpu_lock);
	tegra_cpu_edp_init(false);
	mutex_unlock(&tegra_cpu_lock);

	ret = register_pm_notifier(&tegra_cpu_pm_notifier);

	if (ret)
		return ret;

	ret = cpufreq_register_notifier(
		&tegra_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);

	if (ret)
		return ret;

	return cpufreq_register_driver(&tegra_cpufreq_driver);
}

static void __exit tegra_cpufreq_exit(void)
{
	tegra_throttle_exit();
	tegra_cpu_edp_exit();
	tegra_auto_hotplug_exit();
	cpufreq_unregister_driver(&tegra_cpufreq_driver);
	cpufreq_unregister_notifier(
		&tegra_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);
}


MODULE_AUTHOR("Colin Cross <ccross@android.com>");
MODULE_DESCRIPTION("cpufreq driver for Nvidia Tegra2");
MODULE_LICENSE("GPL");
module_init(tegra_cpufreq_init);
module_exit(tegra_cpufreq_exit);
