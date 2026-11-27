// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/kernel_stat.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/export.h>

s64 uptime_offset_sec = 0;
EXPORT_SYMBOL(uptime_offset_sec);

static int uptime_proc_show(struct seq_file *m, void *v)
{
	struct timespec64 uptime;
	struct timespec64 idle;
	u64 nsec;
	u32 rem;
	s64 spoofed_sec;
	int i;

	nsec = 0;
	for_each_possible_cpu(i)
		nsec += (__force u64) kcpustat_cpu(i).cpustat[CPUTIME_IDLE];

	ktime_get_boottime_ts64(&uptime);
	idle.tv_sec = div_u64_rem(nsec, NSEC_PER_SEC, &rem);
	idle.tv_nsec = rem;

	spoofed_sec = (s64)uptime.tv_sec + uptime_offset_sec;
	if (spoofed_sec < 0)
		spoofed_sec = 0;

	seq_printf(m, "%lu.%02lu %lu.%02lu\n",
			(unsigned long) spoofed_sec,
			(uptime.tv_nsec / (NSEC_PER_SEC / 100)),
			(unsigned long) idle.tv_sec,
			(idle.tv_nsec / (NSEC_PER_SEC / 100)));
	return 0;
}

static int uptime_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, uptime_proc_show, NULL);
}

static ssize_t uptime_proc_write(struct file *file, const char __user *buffer,
				 size_t count, loff_t *ppos)
{
	char *kbuf;
	s64 target_sec;
	struct timespec64 uptime;
	int ret;

	if (count == 0 || count > 64)
		return -EINVAL;

	kbuf = kzalloc(count + 1, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buffer, count)) {
		kfree(kbuf);
		return -EFAULT;
	}

	ret = kstrtoll(strim(kbuf), 10, &target_sec);
	kfree(kbuf);

	if (ret < 0 || target_sec < 0)
		return -EINVAL;

	ktime_get_boottime_ts64(&uptime);
	uptime_offset_sec = target_sec - (s64)uptime.tv_sec;

	return count;
}

static const struct file_operations uptime_proc_fops = {
	.open		= uptime_proc_open,
	.read		= seq_read,
	.write		= uptime_proc_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_uptime_init(void)
{
	proc_create("uptime", 0644, NULL, &uptime_proc_fops);
	return 0;
}
fs_initcall(proc_uptime_init);
