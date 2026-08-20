/*
 * Copyright (C) 2014-2026 Canonical
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
#include <string.h>
#include <signal.h>
#include <setjmp.h>

#include "fwts.h"

static sigjmp_buf jmpbuf;
static struct sigaction old_segv_action, old_bus_action;

/*
 *  If we hit a SIGSEGV or SIGBUS then the read
 *  failed and we longjmp back and return
 *  FWTS_ERROR
 */
static void sig_handler(int dummy)
{
	FWTS_UNUSED(dummy);

	fwts_sig_handler_restore(SIGSEGV, &old_segv_action);
	fwts_sig_handler_restore(SIGBUS, &old_bus_action);
	siglongjmp(jmpbuf, 1);
}

/*
 *  fwts_safe_memcpy()
 *	memcpy that catches SIGSEFV/SIGBUS. To be used when
 *	attempting to read BIOS tables from memory which
 *	may segfault or throw a bus error if the src
 *	address is corrupt
 */
int OPTIMIZE0 fwts_safe_memcpy(void *dst, const void *src, const size_t n)
{
	size_t i;

	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, sig_handler, &old_segv_action);
	fwts_sig_handler_set(SIGBUS, sig_handler, &old_bus_action);
	for (i = n; i; --i)
		*(char *)dst++ = *(char *)src++;
	fwts_sig_handler_restore(SIGSEGV, &old_segv_action);
	fwts_sig_handler_restore(SIGBUS, &old_bus_action);

	return FWTS_OK;
}

/*
 *  fwts_safe_memread()
 *	check we can safely read a region of memory. This catches
 *	SIGSEGV/SIGBUS errors and returns FWTS_ERROR if it is not
 *	readable or FWTS_OK if it's OK.
 */
int OPTIMIZE0 fwts_safe_memread(const void *src, const size_t n)
{
	static uint8_t buffer[256];
	const uint8_t *ptr, *end = (const uint8_t *)src + n;
	uint8_t *bufptr;
	const uint8_t *bufend = buffer + sizeof(buffer);

	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, sig_handler, &old_segv_action);
	fwts_sig_handler_set(SIGBUS, sig_handler, &old_bus_action);

	for (bufptr = buffer, ptr = src; ptr < end; ptr++) {
		/* Force data to be read */
		__builtin_prefetch(ptr, 0, 3);
		*bufptr = *ptr;
		bufptr++;
		bufptr = (bufptr >= bufend) ? buffer : bufptr;
	}

	fwts_sig_handler_restore(SIGSEGV, &old_segv_action);
	fwts_sig_handler_restore(SIGBUS, &old_bus_action);

	return FWTS_OK;
}

/*
 *  fwts_safe_memread32()
 *	check we can safely read a region of memory. This catches
 *	SIGSEGV/SIGBUS errors and returns FWTS_ERROR if it is not
 *	readable or FWTS_OK if it's OK.
 *
 *	n = number of of 32 bit words to check
 */
int OPTIMIZE0 fwts_safe_memread32(const void *src, const size_t n)
{
	static uint32_t buffer[256];
	const uint32_t *ptr, *end = (const uint32_t *)src + n;
	uint32_t *bufptr;
	const uint32_t *bufend = buffer + (sizeof(buffer) / sizeof(*buffer));

	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, sig_handler, &old_segv_action);
	fwts_sig_handler_set(SIGBUS, sig_handler, &old_bus_action);

	for (bufptr = buffer, ptr = src; ptr < end; ptr++) {
		/* Force data to be read */
		__builtin_prefetch(ptr, 0, 3);
		*bufptr = *ptr;
		bufptr++;
		bufptr = (bufptr >= bufend) ? buffer : bufptr;
	}

	fwts_sig_handler_restore(SIGSEGV, &old_segv_action);
	fwts_sig_handler_restore(SIGBUS, &old_bus_action);

	return FWTS_OK;
}

/*
 *  fwts_safe_memread64()
 *	check we can safely read a region of memory. This catches
 *	SIGSEGV/SIGBUS errors and returns FWTS_ERROR if it is not
 *	readable or FWTS_OK if it's OK.
 *
 *	n = number of of 64 bit words to check
 */
int OPTIMIZE0 fwts_safe_memread64(const void *src, const size_t n)
{
	static uint64_t buffer[256];
	const uint64_t *ptr, *end = (const uint64_t *)src + n;
	uint64_t *bufptr;
	const uint64_t *bufend = buffer + (sizeof(buffer) / sizeof(*buffer));

	if (sigsetjmp(jmpbuf, 1) != 0)
		return FWTS_ERROR;

	fwts_sig_handler_set(SIGSEGV, sig_handler, &old_segv_action);
	fwts_sig_handler_set(SIGBUS, sig_handler, &old_bus_action);

	for (bufptr = buffer, ptr = src; ptr < end; ptr++) {
		/* Force data to be read */
		__builtin_prefetch(ptr, 0, 3);
		*bufptr = *ptr;
		bufptr++;
		bufptr = (bufptr >= bufend) ? buffer : bufptr;
	}

	fwts_sig_handler_restore(SIGSEGV, &old_segv_action);
	fwts_sig_handler_restore(SIGBUS, &old_bus_action);

	return FWTS_OK;
}
