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
#ifndef _SMCCC_TEST_H_
#define _SMCCC_TEST_H_

#include <linux/types.h>

struct smccc_test_arg {
	__u32 size;
	__u32 conduit;
	__u32 w[8];
};

#define SMCCC_TEST_PCI_VERSION \
        _IOWR('p', 0x01, struct smccc_test_arg)
#define SMCCC_TEST_PCI_FEATURES \
        _IOWR('p', 0x02, struct smccc_test_arg)
#define SMCCC_TEST_PCI_READ \
        _IOWR('p', 0x03, struct smccc_test_arg)
#define SMCCC_TEST_PCI_WRITE \
        _IOWR('p', 0x04, struct smccc_test_arg)
#define SMCCC_TEST_PCI_GET_SEG_INFO \
        _IOWR('p', 0x05, struct smccc_test_arg)
#define SMCCC_TEST_VERSION_FUNCTION \
        _IOWR('p', 0x06, struct smccc_test_arg)
#define SMCCC_TEST_ARCH_FEATURES \
        _IOWR('p', 0x07, struct smccc_test_arg)
#define SMCCC_TEST_ARCH_SOC_ID \
        _IOWR('p', 0x08, struct smccc_test_arg)
#endif
