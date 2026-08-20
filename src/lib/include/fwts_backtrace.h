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
#ifndef __FWTS_BACKTRACE__
#define __FWTS_BACKTRACE__

#include <signal.h>

void fwts_print_backtrace(void);
int fwts_fault_catch(void);
void fwts_sig_handler_set(int signum, void (*handler)(int), struct sigaction *old_action);
void fwts_sig_handler_restore(int signum, struct sigaction *old_action);

#endif
