/*
 * Copyright (C) 2013-2026 Canonical
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

#include <stdint.h>
#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

#include <sys/io.h>
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf jmpbuf;
static struct sigaction old_action;

/*
 *  If we hit a SIGSEGV then the port read
 *  failed and we longjmp back and return
 *  FWTS_ERROR
 */
static void segv_handler(int dummy)
{
	FWTS_UNUSED(dummy);

	fwts_sig_handler_restore(SIGSEGV, &old_action);
	siglongjmp(jmpbuf, 1);
}

/*
 *  fwts_inb()
 *	8 bit port read, handle access errors
 */
int fwts_inb(uint32_t port, uint8_t *value)
{
	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, segv_handler, &old_action);
	*value = inb(port);
	fwts_sig_handler_restore(SIGSEGV, &old_action);

	return FWTS_OK;
}

/*
 *  fwts_inw()
 *	16 bit port read, handle access errors
 */
int fwts_inw(uint32_t port, uint16_t *value)
{
	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, segv_handler, &old_action);
	*value = inw(port);
	fwts_sig_handler_restore(SIGSEGV, &old_action);

	return FWTS_OK;
}

/*
 *  fwts_inl()
 *	32 bit port read, handle access errors
 */
int fwts_inl(uint32_t port, uint32_t *value)
{
	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, segv_handler, &old_action);
	*value = inl(port);
	fwts_sig_handler_restore(SIGSEGV, &old_action);

	return FWTS_OK;
}

/*
 *  fwts_outb()
 *	8 bit port write, handle access errors
 */
int fwts_outb(uint32_t port, uint8_t value)
{
	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, segv_handler, &old_action);
	outb(port, value);
	fwts_sig_handler_restore(SIGSEGV, &old_action);

	return FWTS_OK;
}

/*
 *  fwts_outw()
 *	16 bit port write, handle access errors
 */
int fwts_outw(uint32_t port, uint16_t value)
{
	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, segv_handler, &old_action);
	outw(port, value);
	fwts_sig_handler_restore(SIGSEGV, &old_action);

	return FWTS_OK;
}

/*
 *  fwts_outl()
 *	32 bit port write, handle access errors
 */
int fwts_outl(uint32_t port, uint32_t value)
{
	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, segv_handler, &old_action);
	outl(port, value);
	fwts_sig_handler_restore(SIGSEGV, &old_action);

	return FWTS_OK;
}

#else

/*
 *  dummy versions of above, all return FWTS_ERROR
 *  for non-x86 platforms and any return values are
 *  set to ~0.
 */
int fwts_inb(uint32_t port, uint8_t *value)
{
	FWTS_UNUSED(port);

	*value = ~0;
	return FWTS_ERROR;
}

int fwts_inw(uint32_t port, uint16_t *value)
{
	FWTS_UNUSED(port);

	*value = ~0;
	return FWTS_ERROR;
}

int fwts_inl(uint32_t port, uint32_t *value)
{
	FWTS_UNUSED(port);

	*value = ~0;
	return FWTS_ERROR;
}

int fwts_outb(uint32_t port, uint8_t value)
{
	FWTS_UNUSED(port);
	FWTS_UNUSED(value);

	return FWTS_ERROR;
}

int fwts_outw(uint32_t port, uint16_t value)
{
	FWTS_UNUSED(port);
	FWTS_UNUSED(value);

	return FWTS_ERROR;
}

int fwts_outl(uint32_t port, uint32_t value)
{
	FWTS_UNUSED(port);
	FWTS_UNUSED(value);

	return FWTS_ERROR;
}

#endif
