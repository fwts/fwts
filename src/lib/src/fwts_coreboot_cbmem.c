 /*
 * Copyright (C) 2010-2026 Canonical
 * Copyright (C) 2017-2021 Google Inc.
 * Copyright (C) 2018-2021 9elements Cyber Security
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

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>


#define CURSOR_MASK ((1UL << 28) - 1)
#define OVERFLOW (1UL << 31)

#define LB_TAG_CBMEM_CONSOLE	0x0017
#define LB_TAG_FORWARD		0x0011

struct lb_record {
        uint32_t tag;           /* tag ID */
        uint32_t size;          /* size of record (in bytes) */
} __attribute__ ((packed));

struct lb_header {
        uint8_t  signature[4]; /* LBIO */
        uint32_t header_bytes;
        uint32_t header_checksum;
        uint32_t table_bytes;
        uint32_t table_checksum;
        uint32_t table_entries;
} __attribute__ ((packed));

struct lb_forward {
        uint32_t tag;
        uint32_t size;
        uint64_t forward;
} __attribute__ ((packed));

struct lb_cbmem_ref {
        uint32_t tag;
        uint32_t size;

        uint64_t cbmem_addr;
} __attribute__ ((packed));

struct cbmem_console {
	uint32_t size;
	uint32_t cursor;
	uint8_t  body[0];
} __attribute__ ((packed));

/* Return < 0 on error, 0 on success. */
static int parse_cbtable(const off_t address, const size_t table_size, off_t *cbmen_console_addr);

static void *map_memory(const off_t addr, const size_t size)
{
	void *mem;
	void *phy;

	phy = fwts_mmap(addr, size);
	if (phy == FWTS_MAP_FAILED)
		return NULL;

	mem = malloc(size);
	if (!mem) {
		fwts_munmap(phy, size);
		return NULL;
	}

	memcpy(mem, phy, size);
	fwts_munmap(phy, size);

	return mem;
}

/*
 * calculate ip checksum (16 bit quantities) on a passed in buffer. In case
 * the buffer length is odd last byte is excluded from the calculation
 */
static uint16_t ipchcksum(const void *addr, unsigned size)
{
	const uint16_t *p = addr;
	unsigned i, n = size / 2; /* don't expect odd sized blocks */
	uint32_t sum = 0;

	for (i = 0; i < n; i++)
		sum += p[i];

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	sum = ~sum & 0xffff;

	return (uint16_t) sum;
}

/*
 * This is a work-around for a nasty problem introduced by initially having
 * pointer sized entries in the lb_cbmem_ref structures. This caused problems
 * on 64bit x86 systems because coreboot is 32bit on those systems.
 * When the problem was found, it was corrected, but there are a lot of
 * systems out there with a firmware that does not produce the right
 * lb_cbmem_ref structure. Hence we try to autocorrect this issue here.
 */
static struct lb_cbmem_ref parse_cbmem_ref(const struct lb_cbmem_ref *cbmem_ref)
{
	struct lb_cbmem_ref ret;

	ret = *cbmem_ref;

	if (cbmem_ref->size < sizeof(*cbmem_ref))
		ret.cbmem_addr = (uint32_t)ret.cbmem_addr;

	return ret;
}

/*
 * Return < 0 on error, 0 on success, 1 if forwarding table entry found.
 */
static int parse_cbtable_entries(
	const void *lbtable,
	const size_t table_size,
	off_t *cbmem_console_addr)
{
	size_t i;
	int forwarding_table_found = 0;
	const struct lb_record *lbr_p;

	for (i = 0; i < table_size; i += lbr_p->size) {
		lbr_p = (struct lb_record*)((char *)lbtable + i);
		switch (lbr_p->tag) {
		case LB_TAG_CBMEM_CONSOLE: {
			*cbmem_console_addr = (off_t)parse_cbmem_ref((const struct lb_cbmem_ref *) lbr_p).cbmem_addr;
			if (*cbmem_console_addr)
				return 0;
			continue;
		}
		case LB_TAG_FORWARD: {
			int ret;
			/*
			 * This is a forwarding entry - repeat the
			 * search at the new address.
			 */
			struct lb_forward lbf_p =
				*(const struct lb_forward *) lbr_p;
			ret = parse_cbtable(lbf_p.forward, 0, cbmem_console_addr);

			/* Assume the forwarding entry is valid. If this fails
			 * then there's a total failure. */
			if (ret < 0)
				return -1;
			forwarding_table_found = 1;
		}
		default:
			break;
		}
	}

	return forwarding_table_found;
}

/*
 * Return < 0 on error, 0 on success.
 */
static int parse_cbtable(
	const off_t address,
	const size_t table_size,
	off_t *cbmem_console_table)
{
	void *buf;
	size_t req_size;
	size_t i;

	req_size = table_size;

	/* Default to 4 KiB search space. */
	if (req_size == 0)
		req_size = 4 * 1024;

	buf = map_memory(address, req_size);
	if (!buf)
		return -1;

	/* look at every 16 bytes */
	for (i = 0; i <= req_size - sizeof(struct lb_header); i += 16) {
		int ret;
		const struct lb_header *lbh = (struct lb_header *)((char *)buf + i);
		void *map;

		if (memcmp(lbh->signature, "LBIO", sizeof(lbh->signature)) ||
		    !lbh->header_bytes ||
		    ipchcksum(lbh, sizeof(*lbh))) {
			continue;
		}

		/* Map in the whole table to parse. */
		if (!(map = map_memory(address + i + lbh->header_bytes,
				 lbh->table_bytes))) {
			continue;
		}

		if (ipchcksum(map, lbh->table_bytes) !=
		    lbh->table_checksum) {
			free(map);
			continue;
		}

		ret = parse_cbtable_entries(map, lbh->table_bytes, cbmem_console_table);

		/* Table parsing failed. */
		if (ret < 0) {
			free(map);
			continue;
		}

		free(buf);
		free(map);

		return 0;
	}

	free(buf);

	return -1;
}

static ssize_t memory_read_from_buffer(
	void *to,
	size_t count,
	size_t *ppos,
	const void *from,
	const size_t available)
{
	size_t pos = *ppos;

	if (pos >= available)
		return 0;

	if (count > available - pos)
		count = available - pos;

	memcpy(to, (char *)from + pos, count);

	*ppos = pos + count;

	return count;
}

static ssize_t memconsole_coreboot_read(
	struct cbmem_console *con,
	char *buf,
	size_t pos,
	size_t count)
{
	uint32_t cursor = con->cursor & CURSOR_MASK;
	uint32_t flags = con->cursor & ~CURSOR_MASK;

	/* describes ring buffer segments in logical order */
	struct seg {
		uint32_t phys;	/* physical offset from start of mem buffer */
		uint32_t len;	/* length of segment */
	} seg[2] = { { 0, 0 }, { 0, 0 } };
	size_t done = 0;
	unsigned int i;

	if (flags & OVERFLOW) {
		if (cursor > count)	/* Shouldn't really happen, but... */
			cursor = 0;
		seg[0] = (struct seg){.phys = cursor, .len = count - cursor};
		seg[1] = (struct seg){.phys = 0, .len = cursor};
	} else {
		seg[0] = (struct seg){.phys = 0, .len = FWTS_MIN(cursor, count)};
	}

	for (i = 0; i < FWTS_ARRAY_SIZE(seg) && count > done; i++) {
		done += memory_read_from_buffer(buf + done, count - done, &pos,
			con->body + seg[i].phys, seg[i].len);
		pos -= seg[i].len;
	}

	return done;
}

char *fwts_coreboot_cbmem_console_dump(void)
{
	unsigned int j;
	off_t cbmem_console_addr = (off_t)-1;
	unsigned long long possible_base_addresses[] = { 0, 0xf0000 };
	struct cbmem_console *console_p;
	struct cbmem_console *console;
	char *coreboot_log;
	ssize_t count;

	/* Find and parse coreboot table */
	for (j = 0; j < FWTS_ARRAY_SIZE(possible_base_addresses); j++) {
		if (!parse_cbtable(possible_base_addresses[j], 0, &cbmem_console_addr))
			break;
	}
	if (cbmem_console_addr == (off_t)-1)
		return NULL;

	console_p = map_memory(cbmem_console_addr, sizeof(*console_p));
	if (console_p == NULL)
		return NULL;

	console = map_memory(cbmem_console_addr, console_p->size + sizeof(*console));
	if (console == NULL) {
		free(console_p);
		return NULL;
	}

	free(console_p);

	coreboot_log = malloc(console->size + 1);
	if (!coreboot_log) {
		free(console);
		return NULL;
	}

	coreboot_log[console->size] = '\0';

	count = memconsole_coreboot_read(console, coreboot_log, 0, console->size);
	free(console);

	if (count == 0) {
		free(coreboot_log);
		return NULL;
	}

	return coreboot_log;
}

#endif
