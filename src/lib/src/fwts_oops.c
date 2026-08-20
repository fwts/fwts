/*
 * Copyright (C) 2011-2026 Canonical
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

#include <sys/klog.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fwts.h"

#define FWTS_OOPS_GOT_OOPS		(0x0001)
#define FWTS_OOPS_GOT_CALL_TRACE	(0x0002)
#define FWTS_OOPS_GOT_END_TRACE		(0x0004)
#define FWTS_OOPS_GOT_WARN_ON		(0x0008)

#define FWTS_OOPS_DUMPABLE		\
	(FWTS_OOPS_GOT_OOPS | FWTS_OOPS_GOT_CALL_TRACE | FWTS_OOPS_GOT_END_TRACE)

#define FWTS_WARN_ON_DUMPABLE		\
	(FWTS_OOPS_GOT_WARN_ON | FWTS_OOPS_GOT_CALL_TRACE | FWTS_OOPS_GOT_END_TRACE)

/*
 *  fwts_klog_stack_dump()
 *	for a given item in a kernel log list, scan for an oops or WARN_ON message.
 *	Increment oopses or warn_ons depending on what we find and dump out the stack
 *	trace to the fwts log
 */
static void fwts_klog_stack_dump(
	fwts_framework *fw,
	fwts_list_link *bug_item,
	int *oopses,
	int *warn_ons)
{
	fwts_list_link *item = bug_item;
	int lines = 0;
	int dumpable = 0;
	bool dumpstack = false;

	fwts_list_foreach_continue(item) {
		char *line = fwts_klog_remove_timestamp(fwts_list_data(char *, item));

		if (strstr(line, "Oops:"))
			dumpable |= FWTS_OOPS_GOT_OOPS;
		if (strstr(line, "kernel BUG at"))
			dumpable |= FWTS_OOPS_GOT_OOPS;
		if (strstr(line, "WARNING: at"))
			dumpable |= FWTS_OOPS_GOT_WARN_ON;
		if (strstr(line, "WARNING: CPU:"))
			dumpable |= FWTS_OOPS_GOT_WARN_ON;
		if (strstr(line, "Call Trace:"))
			dumpable |= FWTS_OOPS_GOT_CALL_TRACE;
		if (strstr(line, "--[ end trace")) {
			dumpable |= FWTS_OOPS_GOT_END_TRACE;
			break;
		}
		lines++;

		/*
		 * We are looking for an Oops message within 5 lines of a "BUG:"
		 * or we've got a WARN_ON then, OK, otherwise abort.
		 */
		if ((lines > 5) &&
		    (!(dumpable & (FWTS_OOPS_GOT_OOPS | FWTS_OOPS_GOT_WARN_ON))))
			return;

	}

	/* Sanity check: too many lines? it is a bit suspect */
	if (lines > 100)
		return;

	if (item == NULL)
		return;

	/* Found all the features that indicate an oops, so dump it */
	if ((dumpable & FWTS_OOPS_DUMPABLE) == FWTS_OOPS_DUMPABLE) {
		(*oopses)++;
		fwts_log_info(fw, "Found OOPS (%d):", *oopses);
		dumpstack = true;
	}

	/* Found all the features that indicate a WARN_ON, so dump it */
	if ((dumpable & FWTS_WARN_ON_DUMPABLE) == FWTS_WARN_ON_DUMPABLE) {
		(*warn_ons)++;
		fwts_log_info(fw, "Found WARNING (%d):", *warn_ons);
		dumpstack = true;
	}

	if (dumpstack) {
		while (bug_item != NULL && bug_item != item) {
			fwts_log_info_verbatim(fw, "  %s",
				fwts_klog_remove_timestamp(fwts_list_data(char *, bug_item)));
			bug_item = bug_item->next;
		}
		fwts_log_nl(fw);
	}
}

/*
 *  fwts_oos_check()
 *	scan kernel log list for any oops messages. The number of oops
 *	messages found is returned in 'oopses'.  Oops messages are logged to the
 *	fwts log.
 */
int fwts_oops_check(fwts_framework *fw, fwts_list *klog, int *oopses, int *warn_ons)
{
	fwts_list_link *item;

	/* Sanity check */
	if ((fw == NULL) || (oopses == NULL) ||
	    (warn_ons == NULL) || (klog == NULL))
		return FWTS_ERROR;

	*oopses = 0;
	*warn_ons = 0;

	fwts_list_foreach(item, klog) {
		char *line = fwts_klog_remove_timestamp(fwts_list_data(char *, item));
		if ((strncmp("BUG:", line, 4) == 0) ||
		    (strncmp("kernel BUG", line, 10) == 0) ||
		    (strncmp("WARNING:", line, 8) == 0))
			fwts_klog_stack_dump(fw, item, oopses, warn_ons);
	}

	return FWTS_OK;
}
