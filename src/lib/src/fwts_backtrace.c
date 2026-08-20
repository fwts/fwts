/*
 * Copyright (C) 2010-2026 Canonical
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
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

#include "fwts.h"

#define BACK_TRACE_SIZE		(512)

static sigjmp_buf jmp_env;
static void *bt_buff[BACK_TRACE_SIZE];
static size_t bt_size = 0;

/*
 *  fwts_print_backtrace()
 *	parse symbol backtrace and dump it in a easy
 *	to read format.
 */
void fwts_print_backtrace(void)
{
	fprintf(stderr, "Backtrace:\n");
	if (bt_size) {
		char **bt_strings;
		size_t i;

		bt_strings = backtrace_symbols(bt_buff, bt_size);

		for (i = 0; i < bt_size; i++) {
			/*
			 *  convert trace into output in form:
			 *   0x00007fb04a493ec5 /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf5)
			 */
			char *addrstr = strstr(bt_strings[i], " [0x");
			if (addrstr) {
				uint64_t addr;

				*addrstr = '\0';
				addr = (uint64_t)strtoull(addrstr + 2, NULL, 16);
				fprintf(stderr, "0x%16.16" PRIx64 " %s\n", addr, bt_strings[i]);
			}
		}
		free(bt_strings);
	} else {
		fprintf(stderr, "  No data\n");
	}
	fprintf(stderr, "\n");
	fflush(stdout);
}

/*
 *  fwts_fault_handler()
 *	catch a signal, save stack dump, jmp back or abort
 */
static void fwts_fault_handler(int signum)
{
	static bool already_handled = false;

	/* Capture backtrace and jmp back */

	if (!already_handled) {
		already_handled = true;
		/* Capture backtrace from this stack context */
		bt_size = backtrace(bt_buff, BACK_TRACE_SIZE);
		/* Jmp back to the sigsetjmp context */
		siglongjmp(jmp_env, signum);
	}
	/* We've hit a fault before, so abort */
	_exit(EXIT_FAILURE);
}

/*
 *  fwts_sig_handler_set()
 *	helper to set signal handler
 */
void fwts_sig_handler_set(int signum, void (*handler)(int), struct sigaction *old_action)
{
	struct sigaction new_action;

	memset(&new_action, 0, sizeof new_action);
	new_action.sa_handler = handler;
	sigemptyset(&new_action.sa_mask);

        (void)sigaction(signum, &new_action, old_action);
}

/*
 *  fwts_sig_handler_restore()
 *	helper to restore signal handler
 */
void fwts_sig_handler_restore(int signum, struct sigaction *old_action)
{
        (void)sigaction(signum, old_action, NULL);
}

/*
 *  fwts_fault_catch()
 *	catch segfaults and bus errors, dump stack
 *	trace so we can see what's causing them
 */
int fwts_fault_catch(void)
{
	int ret;

	/* Trap segfaults and bus errors */
	fwts_sig_handler_set(SIGSEGV, fwts_fault_handler, NULL);
	fwts_sig_handler_set(SIGBUS, fwts_fault_handler, NULL);

	ret = sigsetjmp(jmp_env, 1);
	/*
	 *  We reach here with ret == SIGNUM if the fault handler
	 *  longjmps back to here.  Or we reach here with
	 *  ret == 0 if sigsetjmp has set the jmp_env up
	 *  correctly.
	 */
	if (ret) {
		fprintf(stderr, "\nCaught SIGNAL %d (%s), aborting.\n",
			ret, strsignal(ret));
		fwts_print_backtrace();
		exit(EXIT_FAILURE);
	}
	return FWTS_OK;
}
