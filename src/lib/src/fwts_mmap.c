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
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fwts.h"

#define FWTS_DEFAULT_PAGE_SIZE  (4096)

/*
 *  fwts_page_size()
 *	determine system page size, guess if we can't
 *	get it from sysconf().
 */
size_t fwts_page_size(void)
{
	long page_size;

	page_size = sysconf(_SC_PAGESIZE);

	/* If sysconf() returns -1, default it 4K */
	return (size_t)(page_size == -1 ? FWTS_DEFAULT_PAGE_SIZE : page_size);
}

#ifdef FWTS_USE_DEVMEM
/*
 *  fwts_mmap()
 *	Try and map physical memory from offset address 'start' and length
 *	'size'. Return either the address or FWTS_MAP_FAILED if failed to mmap.
 */
void *fwts_mmap(const off_t start, const size_t size)
{
	int fd;
	int page_size;
	off_t offset;
	size_t length;
	void *mem;
	void *ret = FWTS_MAP_FAILED;

	page_size = fwts_page_size();
	offset = ((size_t)start) & (page_size - 1);
	length = (size_t)size + offset;

	if ((fd = open("/dev/mem", O_RDONLY)) < 0)
		return ret;

	if ((mem = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, start - offset)) != MAP_FAILED)
		ret = (void *)((uint8_t *)mem + offset);

	(void)close(fd);

	return ret;
}

/*
 *  fwts_munmap()
 *	Unmap memory mapped with fwts_mmap. Needs the mmap'd address and size.
 */
int fwts_munmap(void *mem, const size_t size)
{
	int page_size;
	off_t offset;

	page_size = fwts_page_size();
	offset = ((off_t)(mem)) & (page_size - 1);

	if (munmap((void *)((uint8_t *)mem - offset), size + offset) < 0)
		return FWTS_ERROR;

	return FWTS_OK;
}
#else /* FWTS_USE_DEVMEM */
void *fwts_mmap(const off_t start __attribute__ ((unused)),
		const size_t size __attribute__ ((unused)))
{
	return FWTS_MAP_FAILED;
}

int fwts_munmap(void *mem __attribute__ ((unused)),
		const size_t size __attribute__ ((unused)))                                                                        {
	return FWTS_ERROR;
}
#endif /* FWTS_USE_DEVMEM */
