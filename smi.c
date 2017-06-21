/*
 * Copyright 2017 Seraphime Kirkovski
 * Subject to the GPLv.2
 *
 * Module for triggering SMI's on a particular core
 * Based on arch/x86/kernel/cpuid.c
 *
 * Inspired by Intel Software Developper Manual 3a 10.6.1
 */

#define pr_fmt(fmt) "smi: " fmt

#include <asm/delay.h>
#include <asm/ipi.h>

#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define SMI_MAJOR 73
#define SMI_SHORTCUT_ALL 1
#define SMI_SHORTCUT_ALLBUTSELF 2

static struct class *smi_class;
static enum cpuhp_state smi_cpuhp_state;

static int __smi_issue(int cpu)
{
	pr_notice("Sending SMI IPI to cpu %d from %d\n",
					cpu, raw_smp_processor_id());
	apic->send_IPI(cpu, APIC_DM_SMI);
	return 0;
}

static ssize_t smi_issue(struct file *file, const char __user *ubuff,
				size_t size, loff_t *off)
{
	char buff[64];
	bool do_current = false;
	int cpu = -1, r;

	r = simple_write_to_buffer(buff, sizeof(buff) - 1, off, ubuff, size);
	if (r <= 0)
		return r;
	buff[r] = 0;

	if (!strcmp(buff, "current")) {
		preempt_disable();
		cpu = smp_processor_id();
		do_current = true;
	}

	get_online_cpus();
	__smi_issue(cpu >= 0 ? cpu : iminor(file_inode(file)));
	put_online_cpus();

	if (do_current)
		preempt_enable();

	return r;
}

static const struct file_operations smi_fops = {
	.owner = THIS_MODULE,
	.write = smi_issue,
};


static int smi_device_create(unsigned int cpu)
{
	struct device *dev;

	dev = device_create(smi_class, NULL, MKDEV(SMI_MAJOR, cpu), NULL,
			    "cpu%d", cpu);
	return PTR_ERR_OR_ZERO(dev);
}

static int smi_device_destroy(unsigned int cpu)
{
	device_destroy(smi_class, MKDEV(SMI_MAJOR, cpu));
	return 0;
}

static char *smi_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "cpu/%u/smi", MINOR(dev->devt));
}

static int __init smi_init(void)
{
	int r;

	r = __register_chrdev(SMI_MAJOR, 0, NR_CPUS, "cpu/smi", &smi_fops);
	if (r < 0) {
		pr_err("Cannot regist chrdev for smi\n");
		return r;
	}

	smi_class = class_create(THIS_MODULE, "smi");
	if (IS_ERR(smi_class)) {
		pr_err("cannot create smi class\n");
		r = PTR_ERR(smi_class);
		goto unreg_chrdev;
	}
	smi_class->devnode = smi_devnode;

	smi_cpuhp_state = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "x86/smi:online",
				smi_device_create, smi_device_destroy);
	if (smi_cpuhp_state <= 0) {
		pr_err("cannot setup cpuhp state for smi\n");
		r = smi_cpuhp_state;
		goto destr_class;
	}

	return 0;
destr_class:
	class_destroy(smi_class);
unreg_chrdev:
	__unregister_chrdev(SMI_MAJOR, 0, NR_CPUS, "cpu/smi");
	return r;
}

static void __exit smi_exit(void)
{
	cpuhp_remove_state(smi_cpuhp_state);
	class_destroy(smi_class);
	__unregister_chrdev(SMI_MAJOR, 0, NR_CPUS, "cpu/smi");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Seraphime Kirkovski <kirkseraph@gmail.com>");
MODULE_DESCRIPTION("Expose an interface to issue SMIs via devtmpfs");
module_init(smi_init);
module_exit(smi_exit);
