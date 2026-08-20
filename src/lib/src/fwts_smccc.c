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


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "fwts.h"
#include "smccc_test.h"
#include "fwts_smccc.h"

#ifdef FWTS_ARCH_AARCH64

int smccc_fd = -1;

static const char *module_name = "smccc_test";
static const char *dev_name = "/dev/smccc_test";
static bool module_loaded;



int smccc_init(fwts_framework *fw)
{
	if (fwts_module_load(fw, module_name) != FWTS_OK) {
		module_loaded = false;
		smccc_fd = -1;
		return FWTS_ERROR;
	}

	smccc_fd = open(dev_name, O_RDWR);
	if (smccc_fd < 0) {
		smccc_fd = -1;
		fwts_log_error(fw, "Cannot open %s, errno=%d (%s)\n",
			dev_name, errno, strerror(errno));
	}

	return FWTS_OK;
}

int smccc_deinit(fwts_framework *fw)
{
	if (smccc_fd >= 0) {
		close(smccc_fd);
		smccc_fd = -1;
	}

	if (module_loaded && fwts_module_unload(fw, module_name) != FWTS_OK)
		return FWTS_ERROR;

	module_loaded = true;

	return FWTS_OK;
}

/*
 *  smccc_pci_conduit_name()
 *	map the conduit number to human readable string
 */
char *smccc_pci_conduit_name(struct smccc_test_arg *arg)
{
	static char unknown[32];

	switch (arg->conduit) {
	case FWTS_SMCCC_CONDUIT_NONE:
		return "SMCCC_CONDUIT_NONE";
	case FWTS_SMCCC_CONDUIT_HVC:
		return "SMCCC_CONDUIT_HVC";
	case FWTS_SMCCC_CONDUIT_SMC:
		return "SMCCC_CONDUIT_SMC";
	default:
		break;
	}

	snprintf(unknown, sizeof(unknown), "Unknown: 0x%x", arg->conduit);
	return unknown;
}

/*
 *  smccc_pci_conduit_check()
 *	check if conduit number is valid
 */
int smccc_pci_conduit_check(fwts_framework *fw, struct smccc_test_arg *arg)
{
	switch (arg->conduit) {
	case FWTS_SMCCC_CONDUIT_HVC:
		return FWTS_OK;
	case FWTS_SMCCC_CONDUIT_SMC:
		return FWTS_OK;
	default:
		fwts_log_error(fw, "Invalid SMCCC conduit used: %s\n",
			       smccc_pci_conduit_name(arg));
		return FWTS_ERROR;
	}
	return FWTS_OK;
}


#endif
