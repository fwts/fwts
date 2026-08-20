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
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>

#include "fwts.h"

/*
 *  Typically we use around 50 or so low mem allocations in fwts
 *  so 509 is a good large prime size to ensure we get a good
 *  hash table hit rate
 */
#define HASH_ALLOC_SIZE	(509)

typedef struct hash_alloc {
	struct hash_alloc *next;/* next one in hash chain */
	void *addr;		/* allocation addr, NULL = not used */
	size_t size;		/* actual size of allocation */
} hash_alloc_t;

static hash_alloc_t *hash_allocs[HASH_ALLOC_SIZE];
static int hash_count;

/*
 *  hash_addr()
 *	turn a void * address into a hash index
 */
static inline size_t hash_addr(const void *addr)
{
	ptrdiff_t h = (ptrdiff_t)addr;

	h ^= h >> 17;
	h %= HASH_ALLOC_SIZE;

	return (size_t)h;
}

/*
 *  hash_alloc_add()
 *	add a allocated address into the hash of
 *	known allocations so we can keep track
 *	of valid allocs.
 */
static bool hash_alloc_add(void *addr, size_t size)
{
	size_t h = hash_addr(addr);
	hash_alloc_t *new = hash_allocs[h];

	while (new) {
		/* re-use old nullified records */
		if (!new->addr) {
			/* old and free, so re-use */
			new->addr = addr;
			new->size = size;
			hash_count++;
			return true;
		}
		/* something is wrong, already in use */
		if (new->addr == addr) {
			return false;
		}
		new = new->next;
	}

	/* not found, add a new record */
	new = malloc(sizeof(*new));
	if (!new)
		return false;

	new->addr = addr;
	new->size = size;
	new->next = hash_allocs[h];
	hash_allocs[h] = new;
	hash_count++;

	return true;
}

/*
 *  hash_alloc_find()
 *	find a hash_alloc_t of an allocation
 *	of a given address
 */
static hash_alloc_t *hash_alloc_find(const void *addr)
{
	size_t h = hash_addr(addr);
	hash_alloc_t *ha = hash_allocs[h];

	while (ha) {
		if (ha->addr == addr) {
			return ha;
		}
		ha = ha->next;
	}
	return NULL;
}

/*
 *  hash_alloc_garbage_collect()
 *	free all hash records when the hash
 *	is empty.
 */
static void hash_alloc_garbage_collect(void)
{
	size_t i;

	if (hash_count)
		return;

	for (i = 0; i < HASH_ALLOC_SIZE; i++) {
		hash_alloc_t *ha = hash_allocs[i];

		while (ha) {
			hash_alloc_t *next = ha->next;

			free(ha);
			ha = next;
		}
	}
	memset(hash_allocs, 0, sizeof(hash_allocs));
}

/*
 *  hash_alloc_free()
 *	free up a hash_alloc_t record
 */
static void hash_alloc_free(hash_alloc_t *ha)
{
	if (!ha)
		return;

	ha->addr = NULL;
	ha->size = 0;
	hash_count--;
	hash_alloc_garbage_collect();
}

/*
 * We implement a low memory allocator to allow us to allocate
 * memory < 2G limit for the ACPICA table handling.  On 64 bit
 * machines we have to ensure that cached copies of ACPI tables
 * have addresses that can be addressed by the legacy 32 bit
 * ACPI table pointers.
 *
 * This implementation is not intended to be a malloc replacement
 * for all of fwts - only just for the cached ACPICA table allocations.
 *
 * Hence this implementation is not necessarily an efficient solution,
 * as it will only be used for a handful of tables.
 */

/*
 * CHUNK_SIZE controls the gap between mappings. This creates gaps
 * between each low memory allocation so that we have some chance
 * of catching memory accesses that fall off the mapping.  Without
 * a gap, memory mappings potentially become contiguous and hence
 * memory access errors are harder to catch.   This is wasteful
 * in terms of address space, but fwts doesn't do too many low memory
 * mappings since they are just used for cached copies of ACPI tables.
 */
#define CHUNK_SIZE	(64*1024)

#define LIMIT_2GB	(0x80000000ULL)
#define LIMIT_START	(0x00010000ULL)

/*
 *  fwts_low_mmap_walkdown()
 *	try to allocate a free space under the 2GB limit by
 *	walking down memory in CHUNK_SIZE steps for an unmapped region
 */
static void *fwts_low_mmap_walkdown(const size_t requested_size)
{
	uint8_t *addr;
	size_t page_size = fwts_page_size();
	size_t sz = (requested_size + page_size) & ~(page_size - 1);
	size_t pages = sz / page_size;
	unsigned char vec[pages];
	static uint8_t *last_addr = (uint8_t *)LIMIT_2GB;

	if (requested_size == 0)	/* Illegal */
		return MAP_FAILED;

	for (addr = last_addr - sz; addr > (uint8_t *)LIMIT_START; addr -= CHUNK_SIZE) {
		void *mapping;

		/* Already mapped? */
		if (mincore((void *)addr, pages, vec) == 0)
			continue;

		/* Not mapped but mincore returned something unexpected? */
		if (errno != ENOMEM)
			continue;

		mapping = mmap((void *)addr, requested_size, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
		if (mapping != MAP_FAILED) {
			last_addr = mapping;
			return mapping;
		}
	}
	/* We've scanned all of memory, give up on subsequent calls */
	last_addr = (uint8_t *)LIMIT_START;

	return MAP_FAILED;
}

/*
 *  fwts_low_mmap()
 *	try to find a free space under the 2GB limit and mmap it.
 *	returns address of mmap'd region or MAP_FAILED if failed.
 */
static void *fwts_low_mmap(const size_t requested_size)
{
	FILE *fp;
	char pathname[1024];
	void *addr_start;
	void *addr_end;
	void *last_addr_end = NULL;
	void *first_addr_start = NULL;
	void *ret = MAP_FAILED;
	long prev_pos = 0;

	if (requested_size == 0)	/* Illegal */
		return MAP_FAILED;

	/*
	 *  If we can't access our own mappings then find a
	 *  free page by just walking down memory
 	 */
	fp = fopen("/proc/self/maps", "r");
	if (!fp)
		return fwts_low_mmap_walkdown(requested_size);

	while (!feof(fp)) {
		int n;
		long pos;

		n = fscanf(fp, "%p-%p %*s %*x %*s %*u %1023s\n",
		    &addr_start, &addr_end, pathname);

		/*  At same point of previous scanf, then force a loop exit */
		pos = ftell(fp);
		if (pos == prev_pos)
			break;
		prev_pos = pos;

		/*  We need exactly 3 parsed items of data */
		if (n != 3)
			continue;
		/*
		 *  Sanity check data
		 */
		if ((addr_start <= (void *)LIMIT_START) ||
		    (addr_start >= (void *)LIMIT_2GB) ||
		    (addr_end <= (void *)LIMIT_START) ||
		    (addr_end >= (void *)LIMIT_2GB) ||
		    (addr_end <= addr_start))
			continue;
		/*
		 *  Try and allocate under first mmap'd address space
		 */
		if (!first_addr_start) {
			size_t sz = (requested_size + CHUNK_SIZE) & ~(CHUNK_SIZE - 1);
			uint8_t *addr = (uint8_t *)addr_start - sz;

			/*
			 * If addr is over the 2GB limit and we know
			 * that this is the first mapping then we should
			 * be able to map a region below the 2GB limit as
			 * nothing is already mapped there
			 */
			if (addr > (uint8_t *)LIMIT_2GB)
				addr = (uint8_t *)LIMIT_2GB - sz;

			ret = mmap(addr, requested_size, PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
			if (ret != MAP_FAILED)
				break;	/* Success! */

			first_addr_start = addr_start;
		}

		/*
		 *  No allocation yet, so try now to squeeze one
		 *  in between already mapped regions
		 */
		if ((last_addr_end != NULL) &&
		    (last_addr_end < (void*)LIMIT_2GB)) {
			if (((uint8_t *)addr_start - (uint8_t *)last_addr_end) > (ptrdiff_t)requested_size) {
				void *addr = last_addr_end;

				ret = mmap(addr, requested_size, PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
				if (ret != MAP_FAILED)
					break;	/* Success! */
			}
		}

		/*
		 * We really don't want to mmap at the end of the heap
		 * and if we've reached the stack we've gone too far so
		 * abort
		 */
		if ((strncmp("[heap]", pathname, 6) == 0) ||
		    (strncmp("[stack]", pathname, 7) == 0)) {
			ret = MAP_FAILED;
			break;
		}

		last_addr_end = addr_end;
	}
	(void)fclose(fp);

	/*
	 *  The "intelligent" memory hole finding strategy failed,
	 *  so try walking down memory instead.
	 */
	if (ret == MAP_FAILED)
		ret = fwts_low_mmap_walkdown(requested_size);

	return ret;
}

/*
 *  fwts_low_calloc()
 * 	same as calloc(), but ensure the region is mapped
 *	into the low 2GB of memory. This is required for
 *	allocated ACPI tables that need to be addressed
 *	via legacy 32 bit pointers on 64 bit architectures.
 */
void *fwts_low_calloc(const size_t nmemb, const size_t size)
{
	const size_t n = nmemb * size;
	void *ret = MAP_FAILED;

	if (!nmemb || !size)
		return NULL;

	/* Check for size_t overflow */
	if ((n / size) != nmemb) {
		errno = ENOMEM;
		return NULL;
	}

#ifdef MAP_32BIT
	/* Not portable, only x86 */
	ret = mmap(NULL, n, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
#else
	if (sizeof(void *) == 4) {
		/* 32 bit mmap by default */
		ret = mmap(NULL, n, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	}
#endif
	/* 32 bit mmap failed, so bodge our own 32 bit mmap */
	if (ret == MAP_FAILED)
		ret = fwts_low_mmap(n);

	/* OK, really can't mmap, give up */
	if (ret == MAP_FAILED) {
		errno = ENOMEM;
		return NULL;
	}

	/* should be zero already, but pre-fault it in */
	memset(ret, 0, n);

	if (!hash_alloc_add(ret, n)) {
		(void)munmap(ret, n);
		errno = ENOMEM;
		return NULL;
	}

	return ret;
}

/*
 *  fwts_low_malloc()
 */
void *fwts_low_malloc(const size_t size)
{
	return fwts_low_calloc(1, size);
}

/*
 *  fwts_low_realloc()
 *	realloc memory - ptr must be NULL or
 *	allocated with fwts_low_* allocators
 */
void *fwts_low_realloc(const void *ptr, const size_t size)
{
	void *ret;
	hash_alloc_t *ha;

	if (!ptr)
		return fwts_low_malloc(size);

	ha = hash_alloc_find(ptr);
	if (!ha) {
		errno = ENOMEM;
		return NULL;
	}

	ret = fwts_low_malloc(size);
	if (!ret) {
		errno = ENOMEM;
		return NULL;
	}

	memcpy(ret, ha->addr, ha->size);
	(void)munmap(ha->addr, ha->size);
	hash_alloc_free(ha);

	return ret;
}

/*
 *  fwts_low_free()
 *	free memory allocated by fwts_low_calloc()
 */
void fwts_low_free(const void *ptr)
{
	hash_alloc_t *ha;
	/*
	 *  Sanity check the address we are about to
	 *  try and free. If it is not known about
	 *  the don't free it.
	 */
	if (!ptr)
		return;

	ha = hash_alloc_find(ptr);
	if (!ha) {
		/* Should never happen... */
		fprintf(stderr, "double free on %p\n", ptr);
		return;
	}
	(void)munmap(ha->addr, ha->size);
	hash_alloc_free(ha);
}
