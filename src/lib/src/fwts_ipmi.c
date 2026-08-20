/*
 * Copyright (C) 2010-2026 Canonical
 * Some of this work - Copyright (C) 2016-2021 IBM
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
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/ipmi.h>

#include "fwts.h"

static unsigned int fwts_ipmi_msg_id = 1;
static const char *fwts_ipmi_devnode = "/dev/ipmi0";

bool fwts_ipmi_present(int fwts_ipmi_flags)
{
	return !access(fwts_ipmi_devnode, fwts_ipmi_flags);
}

int fwts_ipmi_exec_query(
	fwts_ipmi_rsp *fwts_base_rsp,
	struct ipmi_req *fwts_ipmi_req)
{

	int fd = 0, fwts_pollfd_rc = 0, fwts_send_rc = 0, fwts_recv_rc = 0;
	uint8_t recv_data[IPMI_MAX_MSG_LENGTH];
	struct ipmi_recv fwts_ipmi_recv;
	struct ipmi_addr fwts_ipmi_addr;
	struct pollfd fwts_pfd;

	if ((fd = open(fwts_ipmi_devnode, O_RDWR)) < 0)
		return FWTS_ERROR;

	fwts_send_rc = ioctl(fd, IPMICTL_SEND_COMMAND, (char *)fwts_ipmi_req);
	if (fwts_send_rc != 0) {
		(void)close(fd);
		return FWTS_ERROR;
	}

	fwts_pfd.events = POLLIN | POLLPRI;
	fwts_pfd.fd = fd;
	fwts_pollfd_rc = poll(&fwts_pfd, 1, 5000);
	if (fwts_pollfd_rc <= 0) {
		(void)close(fd);
		return FWTS_ERROR;
	}

	memset(&recv_data, 0, sizeof(recv_data));
	fwts_ipmi_recv.msg.data = recv_data;
	fwts_ipmi_recv.msg.data_len = sizeof (recv_data);
	fwts_ipmi_recv.addr = (unsigned char *)&fwts_ipmi_addr;
	fwts_ipmi_recv.addr_len = sizeof (fwts_ipmi_addr);
	fwts_recv_rc = ioctl(fd, IPMICTL_RECEIVE_MSG_TRUNC, &fwts_ipmi_recv);
	if (fwts_recv_rc != 0) {
		(void)close(fd);
		return FWTS_ERROR;
	}

	memcpy(fwts_base_rsp, fwts_ipmi_recv.msg.data, sizeof(fwts_ipmi_rsp));

	/* Future completion_code non-zero with good results to pass back */
	if (fwts_base_rsp->completion_code != 0) {
		(void)close(fd);
		return FWTS_ERROR;
	}

	(void)close(fd);
	return FWTS_OK;
}

int fwts_ipmi_base_query(fwts_ipmi_rsp *fwts_base_rsp)
{

	struct ipmi_req fwts_ipmi_req;
	struct ipmi_system_interface_addr fwts_ipmi_bmc_addr;

	memset(&fwts_ipmi_bmc_addr, 0, sizeof(fwts_ipmi_bmc_addr));

	fwts_ipmi_bmc_addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
	fwts_ipmi_bmc_addr.channel = IPMI_BMC_CHANNEL;
	fwts_ipmi_bmc_addr.lun = 0;

	memset(&fwts_ipmi_req, 0, sizeof(fwts_ipmi_req));

	fwts_ipmi_req.addr = (unsigned char *)&fwts_ipmi_bmc_addr;
	fwts_ipmi_req.addr_len = sizeof (fwts_ipmi_bmc_addr);
	fwts_ipmi_req.msgid = fwts_ipmi_msg_id ++;
	fwts_ipmi_req.msg.netfn = IPMI_NETFN_APP_REQUEST;
	fwts_ipmi_req.msg.cmd = IPMI_GET_DEVICE_ID_CMD;
	fwts_ipmi_req.msg.data_len = 0;
	fwts_ipmi_req.msg.data = NULL;

	if (fwts_ipmi_exec_query(fwts_base_rsp, &fwts_ipmi_req))
		return FWTS_ERROR;

	return FWTS_OK;
}
