/*
 * SMCCC test driver
 *
 * Copyright (C) 2021-2025 Canonical Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 *  USA.
 */

#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/*
 * ARM SMCCC kernel test driver
 *   https://developer.arm.com/documentation/den0115/latest
 */

#if defined(__aarch64__) && (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
#include <linux/arm-smccc.h>
#endif

#include "smccc_test.h"

#define MODULE_NAME 		"smccc_test"
#define SMCCC_TEST_VERSION	(0x00000001)

MODULE_AUTHOR("Colin Ian King");
MODULE_DESCRIPTION("SMCCC Test Driver");
MODULE_LICENSE("GPL");

#if defined(__aarch64__) && (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))

/* PCI ECAM conduit (defined by ARM DEN0115A) */
#define SMCCC_PCI_CALL_VAL(val)			\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,	\
			   ARM_SMCCC_SMC_32,	\
			   ARM_SMCCC_OWNER_STANDARD, val)

#ifndef SMCCC_PCI_VERSION
#define SMCCC_PCI_VERSION	SMCCC_PCI_CALL_VAL(0x0130)
#endif

#ifndef SMCCC_PCI_FEATURES
#define SMCCC_PCI_FEATURES	SMCCC_PCI_CALL_VAL(0x131)
#endif

#ifndef SMCCC_PCI_READ
#define SMCCC_PCI_READ		SMCCC_PCI_CALL_VAL(0x132)
#endif

#ifndef SMCCC_PCI_WRITE
#define SMCCC_PCI_WRITE		SMCCC_PCI_CALL_VAL(0x133)
#endif

#ifndef SMCCC_PCI_SEG_INFO
#define SMCCC_PCI_SEG_INFO	SMCCC_PCI_CALL_VAL(0x134)
#endif

#ifndef SMCCC_VERSION
#define SMCCC_VERSION	0x80000000
#endif

#ifndef SMCCC_ARCH_FEATURES
#define SMCCC_ARCH_FEATURES	0x80000001
#endif

#ifndef SMCCC_ARCH_SOC_ID
#define SMCCC_ARCH_SOC_ID	0x80000002
#endif

/*
 *  smccc_test_copy_to_user()
 *	copy arm_res a* registers to user space test_arg w array
 */
static int smccc_test_copy_to_user(unsigned long arg, struct arm_smccc_res *arm_res, int conduit)
{
	struct smccc_test_arg test_arg = { }, __user *test_arg_user;

	test_arg_user = (struct smccc_test_arg __user *)arg;

	test_arg.size = sizeof(test_arg);
	test_arg.conduit = conduit;
	test_arg.w[0] = arm_res->a0;
	test_arg.w[1] = arm_res->a1;
	test_arg.w[2] = arm_res->a2;
	test_arg.w[3] = arm_res->a3;

	if (copy_to_user(test_arg_user, &test_arg, sizeof(*test_arg_user)))
		return -EFAULT;

	return 0;
}

/*
 *  smccc_test_copy_from_user()
 *	copy user space test_arg data to test_arg
 */
static int smccc_test_copy_from_user(struct smccc_test_arg *test_arg, unsigned long arg)
{
	struct smccc_test_arg __user *user;

	user = (struct smccc_test_arg __user *)arg;
	return copy_from_user(test_arg, user, sizeof(*test_arg));
}

static long smccc_test_pci_version(unsigned long arg)
{
	struct arm_smccc_res arm_res = { };
	int conduit;

	conduit = arm_smccc_1_1_invoke(SMCCC_PCI_VERSION, 0, 0, 0, 0, 0, 0, 0, &arm_res);

	return smccc_test_copy_to_user(arg, &arm_res, conduit);
}

static long smccc_test_pci_features(unsigned long arg)
{
	struct arm_smccc_res arm_res = { };
	struct smccc_test_arg test_arg;
	int ret, conduit;

	ret = smccc_test_copy_from_user(&test_arg, arg);
	if (ret)
		return ret;
	conduit = arm_smccc_1_1_invoke(SMCCC_PCI_FEATURES, test_arg.w[0], 0, 0, 0, 0, 0, 0, &arm_res);

	return smccc_test_copy_to_user(arg, &arm_res, conduit);
}

static long smccc_test_pci_get_seg_info(unsigned long arg)
{
	struct arm_smccc_res arm_res = { };
	struct smccc_test_arg test_arg;
	int ret, conduit;

	ret = smccc_test_copy_from_user(&test_arg, arg);
	if (ret)
		return ret;

	conduit = arm_smccc_1_1_invoke(SMCCC_PCI_SEG_INFO, test_arg.w[1], 0, 0, 0, 0, 0, 0, &arm_res);

	return smccc_test_copy_to_user(arg, &arm_res, conduit);
}

static long smccc_test_version(unsigned long arg)
{
	struct arm_smccc_res arm_res = { };
	int conduit;

	conduit = arm_smccc_1_1_invoke(SMCCC_VERSION, 0, 0, 0, 0, 0, 0, 0, &arm_res);

	return smccc_test_copy_to_user(arg, &arm_res, conduit);
}

static long smccc_test_arch_features(unsigned long arg)
{
	struct arm_smccc_res arm_res = { };
	struct smccc_test_arg test_arg;
	int ret, conduit;

	ret = smccc_test_copy_from_user(&test_arg, arg);
	if (ret)
		return ret;

	conduit = arm_smccc_1_1_invoke(SMCCC_ARCH_FEATURES, test_arg.w[1], 0, 0, 0, 0, 0, 0, &arm_res);

	return smccc_test_copy_to_user(arg, &arm_res, conduit);
}

static long smccc_test_arch_soc_id(unsigned long arg)
{
	struct arm_smccc_res arm_res = { };
	struct smccc_test_arg test_arg;
	int ret, conduit;

	ret = smccc_test_copy_from_user(&test_arg, arg);
	if (ret)
		return ret;

	conduit = arm_smccc_1_1_invoke(SMCCC_ARCH_SOC_ID, test_arg.w[1], 0, 0, 0, 0, 0, 0, &arm_res);

	return smccc_test_copy_to_user(arg, &arm_res, conduit);
}

static long smccc_test_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	u32 size;
	u32 __user *user_size = (u32 __user *)arg;

	if (get_user(size, user_size))
		return -EFAULT;
	if (size != sizeof(struct smccc_test_arg))
		return -EINVAL;

	switch (cmd) {
	case SMCCC_TEST_PCI_VERSION:
		return smccc_test_pci_version(arg);
	case SMCCC_TEST_PCI_FEATURES:
		return smccc_test_pci_features(arg);
	case SMCCC_TEST_PCI_READ:
		/* TODO */
		return -ENOTSUPP;
	case SMCCC_TEST_PCI_WRITE:
		/* TODO */
		return -ENOTSUPP;
	case SMCCC_TEST_PCI_GET_SEG_INFO:
		return smccc_test_pci_get_seg_info(arg);
	case SMCCC_TEST_VERSION_FUNCTION:
		return smccc_test_version(arg);
	case SMCCC_TEST_ARCH_FEATURES:
		return smccc_test_arch_features(arg);
	case SMCCC_TEST_ARCH_SOC_ID:
		return smccc_test_arch_soc_id(arg);
	default:
		break;
	}

	return -ENOTTY;
}

static int smccc_test_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int smccc_test_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations smccc_test_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= smccc_test_ioctl,
	.open		= smccc_test_open,
	.release	= smccc_test_close,
	.llseek		= no_llseek,
};

static struct miscdevice smccc_test_dev = {
	MISC_DYNAMIC_MINOR,
	"smccc_test",
	&smccc_test_fops
};

static int __init smccc_test_init(void)
{
	int ret;

	ret = arm_smccc_get_version();
	pr_info(MODULE_NAME ": ARM SMCCC version %d.%d.%d\n",
		(ret >> 16) & 0xff, (ret >> 8) & 0xff, ret & 0xff);

	ret = misc_register(&smccc_test_dev);
	if (ret) {
		pr_err(MODULE_NAME ": can't misc_register on minor=%d\n",
			MISC_DYNAMIC_MINOR);
		return ret;
	}

	return 0;
}

static void __exit smccc_test_exit(void)
{
	misc_deregister(&smccc_test_dev);
}

#else

static int __init smccc_test_init(void)
{
	pr_info(MODULE_NAME ": ARM SMCCC not supported on this kernel and architecture\n");

	return -ENODEV;
}

static void __exit smccc_test_exit(void)
{
}

#endif

module_init(smccc_test_init);
module_exit(smccc_test_exit);
