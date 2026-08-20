/*
 * Copyright (C) 2026 Arm Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */
#ifndef __FWTS_SMCCC_H__
#define __FWTS_SMCCC_H__

#include <stdint.h>
#include "fwts.h"

/* SMCCC conduit types */
enum {
	FWTS_SMCCC_CONDUIT_NONE,
	FWTS_SMCCC_CONDUIT_SMC,
	FWTS_SMCCC_CONDUIT_HVC,
};

extern int smccc_fd;

int smccc_init(fwts_framework *fw);
int smccc_deinit(fwts_framework *fw);
char *smccc_pci_conduit_name(struct smccc_test_arg *arg);
int smccc_pci_conduit_check(fwts_framework *fw, struct smccc_test_arg *arg);

#endif
