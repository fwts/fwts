/*
 * Copyright (C) 2015-2024 Canonical
 * Portions added (c) 2015, Al Stone <ahs3@redhat.com>
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
 */
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include "fwts_acpi_object_eval.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

/*
 * The Long, Sad, True Story of the MADT
 *
 * Once upon a time in ACPI 1.0, there was the MADT.  It was a nice table,
 * and it had two subtables all of its own.  But, it was also a pretty
 * busy table, too, so over time the MADT gathered up other nice little
 * subtables.  By the time ACPI 6.0 came around, the MADT had 16 of the
 * little guys.
 *
 * Now, the MADT kept a little counter around for the subtables.  In fact,
 * it kept two counters: one was the revision level, which was supposed to
 * change when new subtables came to be, or as the ones already around grew
 * up. The second counter was a type number, because the MADT needed a unique
 * type for each subtable so he could tell them apart.  But, sometimes the
 * MADT got so busy, he forgot to increment the revision level when he needed
 * to.  Fortunately, the type counter kept increasing since that's the only
 * way the MADT could find each little subtable.  It just wouldn't do to have
 * every subtable called Number 6.
 *
 * In the next valley over, a castle full of wizards was watching the MADT
 * and made a pact to keep their own counter.  Every time the MADT found a
 * new subtable, or a subtable grew up, the wizards promised they would
 * increment their counter.  Well, wizards being the forgetful sort, they
 * didn't always do that.  And, since there quite a lot of them, they
 * couldn't always remember who was supposed to keep track of the MADT,
 * especially if dinner was coming up soon.  Their counter was called the
 * spec version.
 *
 * Every now and then, the MADT would gather up all its little subtables
 * and take them in to the cobbler to get new boots.  This was a very, very
 * meticulous cobbler, so every time they came, he wrote down all the boot
 * sizes for all of the little subtables.  The cobbler would ask each subtable
 * for its length, check that against his careful notes, and then go get the
 * right boots.  Sometimes, a little subtable would change a bit, and their
 * length did not match what the cobbler had written down.  If the wizards
 * or the MADT had incremented their counters, the cobbler would breath a
 * sigh of relief and write down the new length as the right one.  But, if
 * none of the counters had changed, this would make the cobbler very, very
 * mad.  He couldn't tell if he had the right size boots or not for the
 * little subtable.  He would have to *guess* and this really bugged him.
 *
 * Well, when the cobbler got mad like this, he would go into hiding.  He
 * would not make or sell any boots.  He would not go out at all.  Pretty
 * soon, the coffee shop would have to close because the cobbler wasn't
 * coming by twice a day any more.  Then the grocery store would have to
 * close because he wouldn't eat much.  After a while, everyone would panic
 * and have to move from the village and go live with all their relatives
 * (usually the ones they didn't like very much).
 *
 * Eventually, the cobbler would work his way out of his bad mood, and
 * open up his boot business again.  Then, everyone else could move back
 * to the village and restart their lives, too.
 *
 * Fortunately, we have been able to collect up all the cobbler's careful
 * notes (and we wrote them down below).  We'll have to keep checking these
 * notes over time, too, just as the cobbler does.  But, in the meantime,
 * we can avoid the panic and the reboot since we can make sure that each
 * subtable is doing okay.  And that's what our tests below check.
 *
 *
 * FADT Major Version ->       1    3    4     4     5     5     6    6     6
 * FADT Minor Version ->       x    x    x     x     x     1     0    0     1
 * MADT revision ->            1    1    2     3     3     3     3    4     4
 * Spec Version ->            1.0  2.0  3.0b  4.0a  5.0b  5.1a  6.0  6.0a  6.1
 * Subtable Name	Type  Expected Length ->
 * Processor Local APIC  0x0    8    8    8     8     8     8     8    8     8
 * IO APIC               0x1   12   12   12    12    12    12    12   12    12
 * Int Src Override      0x2   10   10   10    10    10    10    10   10    10
 * NMI Src               0x3    8    8    8     8     8     8     8    8     8
 * Local APIC NMI Struct 0x4    6    6    6     6     6     6     6    6     6
 * Local APIC Addr Ovrrd 0x5        16   12    12    12    12    12   12    12
 * IO SAPIC              0x6        20   16    16    16    16    16   16    16
 * Local SAPIC           0x7         8  >16   >16   >16   >16   >16  >16   >16
 * Platform Int Src      0x8        16   16    16    16    16    16   16    16
 * Proc Local x2APIC     0x9                   16    16    16    16   16    16
 * Local x2APIC NMI      0xa                   12    12    12    12   12    12
 * GICC CPU I/F          0xb                         40    76    80   80    80
 * GICD                  0xc                         24    24    24   24    24
 * GICv2m MSI            0xd                               24    24   24    24
 * GICR                  0xe                               16    16   16    16
 * GIC ITS               0xf                                     20   20    20
 *
 * In the table, each length entry is what should be in the length
 * field of the subtable, and -- in general -- it should match the
 * size of the struct for the subtable.  Any value that is not set
 * (i.e., is zero) indicates that the subtable is not defined for
 * that version of the ACPI spec.
 *
 */

#define FADT_MAX_MAJOR_REVISION	((uint8_t)((FWTS_ACPI_VERSION_NOW & 0xF00) >> 8))
#define FADT_MAX_MINOR_REVISION	((uint8_t)(FWTS_ACPI_VERSION_NOW & 0xF0) >> 4)
#define MADT_MAX_REVISION	((uint8_t)6)

#define SUBTABLE_UNDEFINED	0x00
#define SUBTABLE_VARIABLE	0xff
#define NUM_SUBTABLE_TYPES	24
#define MAX_IO_APIC_ID		256 /* IO APIC ID field is 1 byte */

#define SBBR_ACPI_MAJOR_VERSION 6

struct acpi_madt_subtable_lengths {
	unsigned short major_version;	/* from revision in FADT header */
	unsigned short minor_version;	/* FADT field starting with 5.1 */
	unsigned short madt_version;	/* MADT revision */
	unsigned short num_types;	/* types possible for this version */
	unsigned short lengths[NUM_SUBTABLE_TYPES];
					/* subtable lengths, indexed by type */
};

static struct acpi_madt_subtable_lengths spec_info[] = {
	{ /* for ACPI 1.0b */
		.major_version = 1,
		.minor_version = 0,
		.madt_version = 1,
		.num_types = 5,
		.lengths = { 8, 12, 10, 8, 6 }
	},
	{ /* for ACPI 2.0 */
		.major_version = 3,
		.minor_version = 0,
		.madt_version = 1,
		.num_types = 9,
		.lengths = { 8, 12, 10, 8, 6, 16, 20, 8, 16 }
	},
	{ /* for ACPI 3.0b */
		.major_version = 4,
		.minor_version = 0,
		.madt_version = 2,
		.num_types = 9,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE, 16 }
	},
	{ /* for ACPI 4.0a */
		.major_version = 4,
		.minor_version = 0,
		.madt_version = 3,
		.num_types = 11,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12 }
	},
	{ /* for ACPI 5.0b */
		.major_version = 5,
		.minor_version = 0,
		.madt_version = 3,
		.num_types = 13,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12, 40, 24 }
	},
	{ /* for ACPI 5.1a */
		.major_version = 5,
		.minor_version = 1,
		.madt_version = 3,
		.num_types = 15,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12, 76, 24, 24, 16 }
	},
	{ /* for ACPI 6.0 */
		.major_version = 6,
		.minor_version = 0,
		.madt_version = 3,
		.num_types = 16,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12, 80, 24, 24, 16, 20 }
		/*
		 * The spec technically defines the GIC ITS node length to be
		 * 16, but this is a clear mistake as the fields add up to be
		 * length 20, and the length was corrected in the next revision.
		 * Test for 20, as that is what is expected to be used in real,
		 * implementations and using length 20 ensures that the test can
		 * correctly find the start of any following nodes.
		 */
	},
	{ /* for ACPI 6.0a */
		.major_version = 6,
		.minor_version = 0,
		.madt_version = 4,
		.num_types = 16,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12, 80, 24, 24, 16, 20 }
	},
	{ /* for ACPI 6.1 */
		.major_version = 6,
		.minor_version = 1,
		.madt_version = 4,
		.num_types = 16,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12, 80, 24, 24, 16, 20 }
	},
	{ /* for ACPI 6.2 */
		.major_version = 6,
		.minor_version = 2,
		.madt_version = 4,
		.num_types = 16,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12, 80, 24, 24, 16, 20 }
	},
	{ /* for ACPI 6.3 & 6.3a*/
		.major_version = 6,
		.minor_version = 3,
		.madt_version = 5,
		.num_types = 16,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12, 80, 24, 24, 16, 20 }
	},
	{ /* for ACPI 6.4 */
		.major_version = 6,
		.minor_version = 4,
		.madt_version = 5,
		.num_types = 16,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12, 80, 24, 24, 16, 20 }
	},
	{ /* for ACPI 6.5 */
		.major_version = 6,
		.minor_version = 5,
		.madt_version = 6,
		.num_types = 24,
		.lengths = { 8, 12, 10, 8, 6, 12, 16, SUBTABLE_VARIABLE,
			     16, 16, 12, 82, 24, 24, 16, 20, 16,
			     15, 23, 21, 13, 19, 17, 15 }
	},
	{ /* terminator */
		.major_version = 0,
		.minor_version = 0,
		.madt_version = 0,
		.num_types = 0,
		.lengths = { 0 }
	}
};

static struct acpi_madt_subtable_lengths *spec_data;
static uint8_t fadt_major;
static uint8_t fadt_minor;

static fwts_acpi_table_info *mtable;
static fwts_acpi_table_info *ftable;

static fwts_list msi_frame_ids;
static fwts_list its_ids;
static fwts_list processor_uids;

struct acpi_integer {
	ACPI_OBJECT_TYPE type;
	uint64_t value;
};

static ACPI_STATUS madt_processor_handler(ACPI_HANDLE ObjHandle, uint32_t level,
					  void *context, void **returnvalue)
{
	ACPI_OBJECT_TYPE acpi_type;
	ACPI_STATUS status;
	ACPI_OBJECT obj;
	struct acpi_buffer buf = {sizeof(ACPI_OBJECT), &obj};
	struct acpi_integer *listint;

	/* Prevent -Werror=unused-parameter from complaining */
	FWTS_UNUSED(level);
	FWTS_UNUSED(context);
	FWTS_UNUSED(returnvalue);

	listint = malloc(sizeof(struct acpi_integer));
	if (!listint)
		return (!AE_OK);

	status = AcpiGetType(ObjHandle, &acpi_type);
	if (ACPI_FAILURE(status)) {
		free(listint);
		return (!AE_OK);
	}

	switch(acpi_type) {
	case ACPI_TYPE_PROCESSOR:
		status = AcpiEvaluateObject(ObjHandle, NULL, NULL, &buf);
		if (ACPI_FAILURE(status)) {
			free(listint);
			return status;
		}
		listint->value = obj.Processor.ProcId;
		printf("Testing!!! Found Processor with ProcId: %u\n", obj.Processor.ProcId);
		break;
	case ACPI_TYPE_DEVICE:
		status = AcpiEvaluateObject(ObjHandle, "_UID", NULL, &buf);
		if (ACPI_FAILURE(status)) {
			free(listint);
			return status;
		}
		listint->value = obj.Integer.Value;
		printf("Testing!!! Found Device with UID: %u\n", obj.Integer.Value);
		break;
	default:
		free(listint);
		return (!AE_OK);
	}
	listint->type = acpi_type;
	fwts_list_append(&processor_uids, listint);

	return (AE_OK);
}

static ACPI_OBJECT_TYPE madt_find_processor_uid(fwts_framework *fw,
						uint64_t uid,
						char *table_name)
{
	char table_label[64];
	fwts_list_link *item;

	fwts_list_foreach(item, &processor_uids) {
		struct acpi_integer *listint = fwts_list_data(struct acpi_integer *, item);
		
		printf("Testing!!! Provided UID: %" PRIu64 ", Current listint->value: %" PRIu64 "\n", uid, listint->value);
		if (uid == listint->value) {
			fwts_passed(fw, "MADT %s has matching processor "
				    "UID %" PRIu64 ".", table_name, uid);
			return listint->type;
		}

		if ((uid == 0xFF) && (strcmp(table_name, "LAPICNMI") == 0)) {
			fwts_passed(fw, "MADT %s has processor "
					"UID 0x%" PRIx64 " which signifies "
					"that this applies to all processors "
					"in the machine.", table_name, uid);
			return listint->type;
		}
	}

	sprintf(table_label, "MADT%sUidMismatch", table_name);
	fwts_failed(fw, LOG_LEVEL_MEDIUM,
		    table_label, "%s has no matching processor UID %" PRIu64,
		    table_name, uid);
	return ACPI_NUM_TYPES;
}

static int madt_init(fwts_framework *fw)
{
	fwts_acpi_table_madt *madt;
	fwts_acpi_table_fadt *fadt;
	struct acpi_madt_subtable_lengths *ms = spec_info;

	/* find the ACPI tables needed */
	if (fwts_acpi_find_table(fw, "APIC", 0, &mtable) != FWTS_OK) {
		fwts_log_error(fw, "Cannot find ACPI MADT tables.");
		return FWTS_ERROR;
	}
	if (!mtable) {
		fwts_log_error(fw, "Cannot read ACPI MADT tables.");
		return FWTS_ERROR;
	}

	if (fwts_acpi_find_table(fw, "FACP", 0, &ftable) != FWTS_OK) {
		fwts_log_error(fw, "Cannot find ACPI FADT tables.");
		return FWTS_ERROR;
	}
	if (!ftable) {
		fwts_log_error(fw, "Cannot read ACPI FADT tables.");
		return FWTS_ERROR;
	}

	if (!mtable || mtable->length == 0) {
		fwts_log_error(fw, "Required ACPI MADT (APIC) table not found");
		return FWTS_ERROR;
	}
	if (!ftable || ftable->length == 0) {
		fwts_log_error(fw, "Required ACPI FADT (FACP) table not found");
		return FWTS_ERROR;
	}

	/* determine the reference data needed */
	madt = (fwts_acpi_table_madt *)mtable->data;
	fadt = (fwts_acpi_table_fadt *)ftable->data;

	fadt_major = fadt->header.revision;
	fadt_minor = 0;
	if (fw->flags & FWTS_FLAG_SBBR) {
		if (fadt_major < SBBR_ACPI_MAJOR_VERSION) {
			fwts_log_error(fw, "SBBR support starts with ACPI v6.0,"
					" Current revision is outdated: %" PRIu8 ".%" PRIu8,
					fadt_major, fadt_minor);
			return FWTS_ERROR;
		}
	}
	fwts_get_fadt_version(fw, &fadt_major, &fadt_minor);

	/* find the first occurrence for this version of MADT */
	while (ms->num_types != 0) {
		if (ms->madt_version == madt->header.revision)
			break;
		ms++;
	}

	/* now, find the largest FADT version supported */
	spec_data = NULL;
	while (ms->num_types && ms->madt_version == madt->header.revision) {
		if (ms->major_version <= fadt_major &&
		    ms->minor_version <= fadt_minor) {
			spec_data = ms;
		}
		ms++;
	}

	/*
	 * Initialize the MSI frame ID and ITS ID lists should we need
	 * them later
	 */
	fwts_list_init(&msi_frame_ids);
	fwts_list_init(&its_ids);
	fwts_list_init(&processor_uids);

	if (fwts_acpica_init(fw) != FWTS_OK)
		return FWTS_ERROR;

PRAGMA_PUSH
PRAGMA_NULL_PTR_MATH
	AcpiWalkNamespace(0x0c, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
			  madt_processor_handler, NULL, NULL, NULL);
PRAGMA_POP
	AcpiGetDevices("ACPI0007", madt_processor_handler, NULL, NULL);

	if (!spec_data)
		fwts_acpica_deinit();

	return (spec_data) ? FWTS_OK : FWTS_ERROR;
}

static int madt_checksum(fwts_framework *fw)
{
	uint8_t *data = mtable->data;
	ssize_t length = mtable->length;
	uint8_t checksum = 0;

	/* verify the table checksum */
	checksum = fwts_checksum(data, length);
	if (checksum == 0)
		fwts_passed(fw, "MADT checksum is correct");
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTChecksum",
			    "MADT checksum is incorrect: 0x%x", checksum);

	return FWTS_OK;
}

static int madt_revision(fwts_framework *fw)
{
	fwts_acpi_table_madt *madt = (fwts_acpi_table_madt *)mtable->data;
	struct acpi_madt_subtable_lengths *ms = spec_data;

	/* check the table revision */
	fwts_log_advice(fw, "Most recent FADT revision is %" PRIu8 ".%" PRIu8 ".",
			FADT_MAX_MAJOR_REVISION, FADT_MAX_MINOR_REVISION);
	fwts_log_advice(fw, "Most recent MADT revision is %" PRIu8 ".",
			MADT_MAX_REVISION);

	/* is the madt revision defined at all? */
	if (!ms->num_types) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTRevision",
			    "Undefined MADT revision being used: %" PRIu8,
			    madt->header.revision);
	} else {
			fwts_passed(fw, "MADT revision %" PRIu8 " is defined.",
				    madt->header.revision);
	}

	/* is the madt revision in sync with the fadt revision? */
	if (ms->major_version != fadt_major ||
	    ms->minor_version != fadt_minor) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTFADTRevisions",
			    "MADT revision is not in sync with "
			    "the FADT revision;\n"
			    "MADT %" PRIu8 " expects FADT %" PRIu8 ".%" PRIu8 " "
			    "but found %" PRIu8 ".%" PRIu8 " instead.",
			    madt->header.revision,
			    ms->major_version, ms->minor_version,
			    fadt_major, fadt_minor);
	} else {
		fwts_passed(fw, "MADT revision %" PRIu8 " is in sync "
				"with FADT revision %" PRIu8 ".%" PRIu8 ".",
			madt->header.revision, fadt_major, fadt_minor);
	}

	return FWTS_OK;
}

static int madt_arch_revision(fwts_framework *fw)
{
	fwts_acpi_table_madt *madt = (fwts_acpi_table_madt *)mtable->data;
	uint8_t minrev;
	const char *arch;

	/* initialize starting assumptions */
	if (fw->target_arch == FWTS_ARCH_ARM64) {
		minrev = 3;
		arch = "aarch64";
	} else {
		minrev = 1;
		arch = "intel";
	}

	/* check the supported revision for this architecture */
	if (madt->header.revision >= minrev)
		fwts_passed(fw, "MADT revision %" PRIu8 " meets the minimum needed "
			    "(%" PRIu8 ") for the %s architecture.",
			    madt->header.revision, minrev, arch);
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTArchRevision",
			    "MADT revision is %" PRIu8 ", must be >= %" PRIu8 " "
			"when running on %s",
			madt->header.revision, minrev, arch);

	return FWTS_OK;
}

static int madt_flags(fwts_framework *fw)
{
	fwts_acpi_table_madt *madt = (fwts_acpi_table_madt *)mtable->data;

	/* make sure the reserved bits in the flag field are zero */
	if (madt->flags & 0xfffffffe)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"MADTFlagsNonZero",
			"MADT flags field, bits 1..31 are reserved and "
			"should be zero, but are set as: %" PRIx32 ".\n",
			madt->flags);
	else
		fwts_passed(fw, "MADT flags reserved bits are not set.");

	return FWTS_OK;
}

static const char *madt_sub_names[] = {
	/* 0x00 */ "Processor Local APIC",
	/* 0x01 */ "I/O APIC",
	/* 0x02 */ "Interrupt Source Override",
	/* 0x03 */ "Non-maskable Interrupt Source (NMI)",
	/* 0x04 */ "Local APIC NMI",
	/* 0x05 */ "Local APIC Address Override",
	/* 0x06 */ "I/O SAPIC",
	/* 0x07 */ "Local SAPIC",
	/* 0x08 */ "Platform Interrupt Sources",
	/* 0x09 */ "Processor Local x2APIC",
	/* 0x0a */ "Local x2APIC NMI",
	/* 0x0b */ "GICC CPU Interface",
	/* 0x0c */ "GICD GIC Distributor",
	/* 0x0d */ "GICv2m MSI Frame",
	/* 0x0e */ "GICR Redistributor",
	/* 0x0f */ "GIC Interrupt Translation Service (ITS)",
	/* 0x10 */ "Multiprocessor Wakeup",
	/* 0x11 */ "CORE PIC",
	/* 0x12 */ "LIO PIC",
	/* 0x13 */ "HT PIC",
	/* 0x14 */ "EIO PIC",
	/* 0x15 */ "MSI PIC",
	/* 0x16 */ "BIO PIC",
	/* 0x17 */ "LPC PIC",
	/* 0x18 - 0x7f */ "Reserved. OSPM skips structures of the reserved type.",
	/* 0x80 - 0xff */ "Reserved for OEM use",
	NULL
};

/* check flags points to Table 5-48 Local APIC Flags */
static void check_madt_apic_flags(
	fwts_framework *fw,
	const uint8_t type,
	const uint32_t flags)
{
	fwts_acpi_table_madt *madt = (fwts_acpi_table_madt *) mtable->data;
	bool passed = true;
	uint8_t head = 2;

	if (madt->header.revision < 5)
		head = 1;

	fwts_acpi_reserved_bits("MADT", "Local APIC Flags", flags, head, 31, &passed);

	if (passed)
		fwts_passed(fw, "MADT %s flags field, bits %" PRIu8
			"..31 are reserved and properly set to zero.",
			madt_sub_names[type], head);
}

static int madt_local_apic(fwts_framework *fw,
			   fwts_acpi_madt_sub_table_header *hdr,
			   uint8_t *data)
{
	/* specific checks for subtable type 0: Processor Local APIC */
	fwts_acpi_madt_processor_local_apic *lapic =
		(fwts_acpi_madt_processor_local_apic *)data;

	/* only test table if enabled */
	if (!(lapic->flags & 0x1)) {
		fwts_skipped(fw, "MADT %s is disabled. Skipping test.",
			      madt_sub_names[hdr->type]);
		goto out;
	}

	madt_find_processor_uid(fw, lapic->acpi_processor_id, "LAPIC");
	check_madt_apic_flags (fw, hdr->type, lapic->flags);

out:
	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static unsigned char madt_io_apics[MAX_IO_APIC_ID];
static int madt_io_apic(fwts_framework *fw,
			fwts_acpi_madt_sub_table_header *hdr,
			uint8_t *data)
{
	/* specific checks for subtable type 1: I/O APIC */
	fwts_acpi_madt_io_apic *ioapic = (fwts_acpi_madt_io_apic *)data;

	if (ioapic->reserved != 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTIOAPICFlags",
			    "MADT %s flags are reserved, should be zero "
			    "but are set to: %" PRIx32 ".",
			    madt_sub_names[hdr->type], ioapic->reserved);
	else
		fwts_passed(fw,
			    "MADT %s flags are reserved, and set to zero.",
			    madt_sub_names[hdr->type]);

	if (ioapic->io_apic_phys_address == 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTIOAPICAddrZero",
			    "MADT %s address is zero, needs to be defined.",
			    madt_sub_names[hdr->type]);
	else
		fwts_passed(fw, "MADT %s address in non-zero.",
			    madt_sub_names[hdr->type]);

	if (!madt_io_apics[ioapic->io_apic_id])
		madt_io_apics[ioapic->io_apic_id] = 1;
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADIOSAPICNotUnique",
			    "There are multiple entries for id %" PRIu8 ".",
			    ioapic->io_apic_id);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_interrupt_override(fwts_framework *fw,
				   fwts_acpi_madt_sub_table_header *hdr,
				   uint8_t *data)
{
	/* specific checks for subtable type 2: Interrupt Source Override */
	fwts_acpi_madt_interrupt_override *int_override =
		(fwts_acpi_madt_interrupt_override *)data;

	if (int_override->bus != 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTIRQSrcISA",
			    "MADT %s bus should be 0 for ISA bus.",
			    madt_sub_names[hdr->type]);
	else
		fwts_passed(fw, "MADT %s Bus is 0 for ISA bus.",
			    madt_sub_names[hdr->type]);

	if (int_override->flags & 0xfffffff0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTIRQSrcFlags",
			    "MADT %s flags, bits 4..31 are reserved "
			    "and should be zero, but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type],
			    int_override->flags);
	else
		fwts_passed(fw,
			    "MADT %s flags, bits 4..31 are reserved "
			    "and set to zero.",
			    madt_sub_names[hdr->type]);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_nmi_source(fwts_framework *fw,
			   fwts_acpi_madt_sub_table_header *hdr,
			   uint8_t *data)
{
	/* specific checks for subtable type 3: NMI Source */
	fwts_acpi_madt_nmi *nmi = (fwts_acpi_madt_nmi *)data;

	if (nmi->flags & 0xfffffff0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTNMISrcFlags",
			    "MADT %s flags, bits 4..31 are reserved "
			    "and should be zero, but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type], nmi->flags);
	else
		fwts_passed(fw,
			    "MADT %s flags, bits 4..31 are reserved "
			    "and set to zero.",
			    madt_sub_names[hdr->type]);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_local_apic_nmi(fwts_framework *fw,
			       fwts_acpi_madt_sub_table_header *hdr,
			       uint8_t *data)
{
	/* specific checks for subtable type 4: Local APIC NMI */
	fwts_acpi_madt_local_apic_nmi *nmi =
		(fwts_acpi_madt_local_apic_nmi *)data;

	madt_find_processor_uid(fw, nmi->acpi_processor_id, "LAPICNMI");

	if (nmi->flags & 0xfffffff0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTLAPICNMIFlags",
			    "MADT %s flags, bits 4..31 are reserved "
			    "and should be zero, but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type], nmi->flags);
	else
		fwts_passed(fw,
			    "MADT %s flags, bits 4..31 are reserved "
			    "and set to zero.",
			    madt_sub_names[hdr->type]);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_lapic_addr_override(fwts_framework *fw,
				    fwts_acpi_madt_sub_table_header *hdr,
				    uint8_t *data)
{
	/* specific checks for subtable type 5: Local APIC Address Override */
	static int count;
	fwts_acpi_madt_local_apic_addr_override *laao =
		(fwts_acpi_madt_local_apic_addr_override *)data;

	count++;
	if (laao->reserved != 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTLAPICOVRReserved",
			    "MADT %s bytes 2..4 are reserved "
			    "and should be zero, but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type], laao->reserved);
	else
		fwts_passed(fw,
			    "MADT %s bytes 2..4 are reserved and set to zero.",
			    madt_sub_names[hdr->type]);

	if (laao->address == 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTLAPICOVRAddress",
			    "MADT %s address should not be set to zero.",
			    madt_sub_names[hdr->type]);
	else
		fwts_passed(fw,
			    "MADT %s address set to non-zero value.",
			    madt_sub_names[hdr->type]);

	if (count > 1)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTLAPICOVRCount",
			    "Only one MADT %s allowed, %d found.",
			    madt_sub_names[hdr->type], count);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static unsigned char madt_io_sapics[MAX_IO_APIC_ID];
static int madt_io_sapic(fwts_framework *fw,
			 fwts_acpi_madt_sub_table_header *hdr,
			 uint8_t *data)
{
	/* specific checks for subtable type 6: I/O SAPIC */
	fwts_acpi_madt_io_sapic *sapic = (fwts_acpi_madt_io_sapic *)data;

	if (sapic->reserved != 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTIOSAPICReserved",
			    "MADT %s byte offset 3 is reserved "
			    "and should be zero, but is set to: %" PRIx32 ".",
			    madt_sub_names[hdr->type], sapic->reserved);
	else
		fwts_passed(fw,
			    "MADT %s bytes 2..4 are reserved and set to zero.",
			    madt_sub_names[hdr->type]);

	if (sapic->address == 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADIOSAPICAddrZero",
			    "MADT %s address is zero, needs to be defined.",
			    madt_sub_names[hdr->type]);
	else
		fwts_passed(fw,
			    "MADT %s address set to non-zero value.",
			    madt_sub_names[hdr->type]);

	if (!madt_io_sapics[sapic->io_sapic_id])
		madt_io_sapics[sapic->io_sapic_id] = 1;
	else
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADIOSAPICNotUnique",
			    "There are multiple entries for id %" PRIu8 ".",
			    sapic->io_sapic_id);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_local_sapic(fwts_framework *fw,
			    fwts_acpi_madt_sub_table_header *hdr,
			    uint8_t *data)
{
	/* specific checks for subtable type 7: Processor Local SAPIC */
	fwts_acpi_madt_local_sapic *lsapic = (fwts_acpi_madt_local_sapic *)data;
	uint8_t tmp;
	int ii;
	ACPI_OBJECT_TYPE type;

	/*
	 * TODO: if using the SAPIC model, check that each processor in
	 * the system has a SAPIC record as required.  The table is not
	 * required to match hotplug additions, but should at least match
	 * initial boot state.
	 */


	if (hdr->length != (strlen(lsapic->uid_string) + 16))
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTLSAPICLength",
			    "MADT %s length of %" PRIu8 " bytes does not match "
			    "actual length of %zd bytes.",
			    madt_sub_names[hdr->type], hdr->length,
			    strlen(lsapic->uid_string) + 16);
	else
		fwts_passed(fw,
			    "MADT %s table length (%" PRIu8 ") is correct.",
			    madt_sub_names[hdr->type], hdr->length);

	/* only continue testing table if enabled */
	if (!(lsapic->flags & 0x1)) {
		fwts_skipped(fw, "MADT %s is disabled. Skipping test.",
			      madt_sub_names[hdr->type]);
		goto out;
	}

	/*
	 * There are three values that need to be checked for a valid
	 * processor UID: the ACPI processor ID, the UID value, and the UID
	 * string (which is an ascii representation of the UID value).  All
	 * three should have the same value in order for the test to pass.
	 */
	type = madt_find_processor_uid(fw, lsapic->acpi_processor_id, "LSAPIC");
	switch(type) {
	case ACPI_TYPE_PROCESSOR:
		break; /* nothing more to do */
	case ACPI_TYPE_DEVICE:
		if (lsapic->uid_value == lsapic->acpi_processor_id)
			fwts_passed(fw, "MADT LSAPIC UID value is %" PRIu32 ".",
				    lsapic->uid_value);
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "MADTLSAPICUidCompare",
				    "MADT LSAPIC UID value (%" PRIu32 ") does not "
				    "match ACPI processor ID (%" PRIu8 ").",
				    lsapic->uid_value,
				    lsapic->acpi_processor_id);
		if (atoi(lsapic->uid_string) == lsapic->acpi_processor_id)
			fwts_passed(fw, "MADT ACPI processor id string is %s.",
				    lsapic->uid_string);
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "MADTLSAPICUidCompare",
				    "MADT LSAPIC UID string (%s) does not "
				    "match UID value (%" PRIu8 ").",
				    lsapic->uid_string,
				    lsapic->acpi_processor_id);
		break;
	default:
		/* Error already reported in madt_find_processor_uid() call */
		fwts_warning(fw, "MADT LSAPIC UID value (%" PRIu32 ") and UID string "
			     "(%s) may be incorrect.",
			     lsapic->uid_value, lsapic->uid_string);
		break;
	}

	for (tmp = 0, ii = 0; ii < 3; tmp |= lsapic->reserved[ii], ii++)
		;

	if (tmp)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTLSAPICReservedNonZero",
			    "MADT %s reserved field, byte offsets 5..8 are "
			    "reserved and should be zero, but are set "
			    "to: 0x%2x %2x %2x.",
			    madt_sub_names[hdr->type],
			    lsapic->reserved[0], lsapic->reserved[1],
			    lsapic->reserved[2]);
	else
		fwts_passed(fw,
			    "MADT %s reserved field, byte offsets 5..8 are "
			    "reserved and properly set to zero.",
			    madt_sub_names[hdr->type]);

	check_madt_apic_flags (fw, hdr->type, lsapic->flags);

	for (tmp = 0, ii = 0; ii < (int)strlen(lsapic->uid_string); ii++)
		if (isascii(lsapic->uid_string[ii]))
			tmp++;
		else
			break;
	if (tmp < strlen(lsapic->uid_string))
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTLSAPICUIDNonAscii",
			    "MADT %s UID string has non-ASCII character "
			    "in position %" PRIu8 ".",
			    madt_sub_names[hdr->type], tmp);
	else
		fwts_passed(fw,
			    "MADT %s UID string is an ASCII string.",
			    madt_sub_names[hdr->type]);

out:
	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_platform_int_source(fwts_framework *fw,
				    fwts_acpi_madt_sub_table_header *hdr,
				    uint8_t *data)
{
	/* specific checks for subtable type 8: Platform Interrupt Sources */
	fwts_acpi_madt_platform_int_source *src =
				(fwts_acpi_madt_platform_int_source *)data;

	if (src->flags & 0xfffffff0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTPlatIRQSrcFlags",
			    "MADT %s flags, bits 4..31 are reserved and "
			    "should be zero, but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type], src->flags);
	else
		fwts_passed(fw,
			    "MADT %s flags, bits 4..31 are reserved and "
			    "set to zero.",
			    madt_sub_names[hdr->type]);

	if (src->type < 1 || src->type > 3)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTPlatIRQType",
			    "MADT %s type field is %" PRIu8 ", must be 1..3.",
			    madt_sub_names[hdr->type], src->type);
	else
		fwts_passed(fw,
			    "MADT %s type field is %" PRIu8 "and in 1..3.",
			    madt_sub_names[hdr->type], src->type);

	/* TODO: verify Processor ID actually exists.   */

	/* TODO: verify Processor EID actually exists.  */

	if (src->io_sapic_vector == 0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTPlatIRQIOSAPICVector",
			    "MADT %s IO SAPIC Vector is "
			    "zero, appears not to be defined.",
			    madt_sub_names[hdr->type]);
	else
		fwts_passed(fw,
			    "MADT %s IO SAPIC Vector is non-zero.",
			    madt_sub_names[hdr->type]);

	/* TODO: verify the GSI is properly set? */

	if (src->pis_flags & 0xfffffffe)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTPlatIRQSrcFlagsNonZero",
			    "MADT %s, Platform Interrupt Source flag "
			    "bits 1..31 are reserved and should be zero, "
			    "but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type], src->pis_flags);
	else
		fwts_passed(fw,
			    "MADT %s, Platform Interrupt Source flag "
			    "bits 1..31 are reserved and set to zero.",
			    madt_sub_names[hdr->type]);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_local_x2apic(fwts_framework *fw,
			     fwts_acpi_madt_sub_table_header *hdr,
			     uint8_t *data)
{
	/* specific checks for subtable type 9: Processor Local x2APIC */
	fwts_acpi_madt_local_x2apic *lx2apic =
					(fwts_acpi_madt_local_x2apic *)data;

	/* only test table if enabled */
	if (!(lx2apic->flags & 0x1)) {
		fwts_skipped(fw, "MADT %s is disabled. Skipping test.",
			      madt_sub_names[hdr->type]);
		goto out;
	}

	if (lx2apic->reserved)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTLX2APICReservedNonZero",
			    "MADT %s reserved field, byte offsets 2..3 are "
			    "reserved and should be zero, but are set "
			    "to: 0x%8x.",
			    madt_sub_names[hdr->type], lx2apic->reserved);
	else
		fwts_passed(fw,
			    "MADT %s reserved field, byte offsets 2..3 are "
			    "reserved and properly set to zero.",
			    madt_sub_names[hdr->type]);

	check_madt_apic_flags (fw, hdr->type, lx2apic->flags);
	madt_find_processor_uid(fw, lx2apic->processor_uid, "X2APIC");

out:
	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_local_x2apic_nmi(fwts_framework *fw,
				 fwts_acpi_madt_sub_table_header *hdr,
				 uint8_t *data)
{
	/* specific checks for subtable type 0xa: Local x2APIC NMI */
	fwts_acpi_madt_local_x2apic_nmi *nmi =
					(fwts_acpi_madt_local_x2apic_nmi *)data;

	if (nmi->flags & 0xfffffff0)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTLAPICX2APICNMIFlags",
			    "MADT %s, flags, bits 4..31 are reserved and "
			    "should be zero, but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type], nmi->flags);
	else
		fwts_passed(fw,
			    "MADT %s flags field, bits 4..31 are "
			    "reserved and properly set to zero.",
			    madt_sub_names[hdr->type]);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_gicc(fwts_framework *fw,
		     fwts_acpi_madt_sub_table_header *hdr,
		     uint8_t *data)
{
	/* specific checks for subtable type 0xb: GICC */
	fwts_acpi_madt_gic *gic = (fwts_acpi_madt_gic *)data;
	uint32_t mask;
	int start;

	if (gic->reserved)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "MADTGICCReservedNonZero",
			    "MADT %s reserved field should be zero, but is "
			    "instead 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], gic->reserved);
	else
		fwts_passed(fw,
			    "MADT %s reserved field properly set to zero.",
			    madt_sub_names[hdr->type]);

	madt_find_processor_uid(fw, gic->processor_uid, "GICC");

	mask = 0xfffffffc;
	start = 2;
	if (hdr->length == 80) {	/* ACPI 6.0 */
		mask = 0xfffffff8;
		start = 3;
	}
	if (hdr->length == 82) {	/* ACPI 6.5 */
		mask = 0xfffffff0;
		start = 4;
	}
	if (gic->flags & mask)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTGICFLags",
			    "MADT %s, flags, bits %d..31 are reserved "
			    "and should be zero, but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type], start, gic->flags);
	else
		fwts_passed(fw,
			    "MADT %s, flags, bits %d..31 are reserved and "
			    "properly set to zero.",
			    madt_sub_names[hdr->type], start);

	if (hdr->length == 82) {	/* ACPI 6.5 */
		/* If the Enabled bit is set, Online Capable bit is reserved and must be zero. */
		if ((gic->flags & 1) && (gic->flags & (1 << 3)))
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "MADTGICBadFLag",
				    "MADT %s, flags, if Enable bit is set, the Online Capable bit "
				    "is reserved and must be zero.",madt_sub_names[hdr->type]);
	}

	if (gic->parking_protocol_version != 0 &&
	    gic->parking_protocol_version != 1)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTGICCParkingVersion",
			    "MADT %s, protocol versions defined are 0..1 but "
			    "%" PRIu32 " is being used.",
			    madt_sub_names[hdr->type],
			    gic->parking_protocol_version);
	else
		fwts_passed(fw,
			    "MADT %s, is using a defined parking protocol "
			    "version.",
			    madt_sub_names[hdr->type]);

	/*
	 * TODO: is it even possible to verify the MPIDR is valid?  Or,
	 * is there sufficient variation that it is not predictable?
	 */

	if (hdr->length == 80) {	/* added in ACPI 6.0 */
		uint8_t tmp;

		if (fwts_get_acpi_version(fw) >= FWTS_ACPI_VERSION_63)
			tmp = gic->reserved2[0];
		else
			tmp = gic->reserved2[0] | gic->reserved2[1] | gic->reserved2[2];

		if (tmp)
			fwts_failed(fw, LOG_LEVEL_LOW,
				    "MADTGICCReserved2NonZero",
				    "MADT %s second reserved field must "
				    "be zero.", madt_sub_names[hdr->type]);
		else
			fwts_passed(fw,
				    "MADT %s second reserved field properly "
				    "set to zero.",
				    madt_sub_names[hdr->type]);
	}

	/*
	 * TODO: the local GICC corresponding to the boot processor must
	 * be the the first entry in the interrupt controller structure
	 * list.
	 */

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_gicd(fwts_framework *fw,
		     fwts_acpi_madt_sub_table_header *hdr,
		     uint8_t *data)
{
	/* specific checks for subtable type 0xc: GIC Distributor */
	fwts_acpi_madt_gicd *gicd = (fwts_acpi_madt_gicd *)data;

	uint32_t gicd_reserve2 = gicd->reserved2[0] +
				 (gicd->reserved2[1] << 4) +
				 (gicd->reserved2[2] << 8);

	if (gicd->reserved)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "MADTGICDReservedNonZero",
			    "MADT %s reserved field should be zero, "
			    "instead got 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], gicd->reserved);
	else
		fwts_passed(fw,
			    "MADT %s reserved field properly set to zero.",
			    madt_sub_names[hdr->type]);

	/* TODO: is the physical base address required to be non-zero? */

	if (gicd->system_vector_base)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "MADTGICDSystemVectorBaseNonZero",
			    "MADT %s system vector base field should be zero, "
			    "instead got 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type],
			    gicd->system_vector_base);
	else
		fwts_passed(fw,
			    "MADT %s system vector base	field is properly set "
			    "to zero.", madt_sub_names[hdr->type]);

	if (gicd->gic_version != 0 && gicd->gic_version > 4)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "SPECMADTGICDVersion",
			    "MADT %s GIC version field should be in 0..4, "
			    "but instead have 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], gicd->gic_version);
	else
		fwts_passed(fw,
			    "MADT %s GIC version field is in 0..4.",
			    madt_sub_names[hdr->type]);

	if (gicd_reserve2)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "MADTGICDReserved2NonZero",
			    "MADT %s second reserved field should be zero, "
			    "instead got 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], gicd_reserve2);
	else
		fwts_passed(fw,
			    "MADT %s second reserved field is properly set "
			    "to zero.", madt_sub_names[hdr->type]);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_gic_msi_frame(fwts_framework *fw,
			      fwts_acpi_madt_sub_table_header *hdr,
			      uint8_t *data)
{
	/* specific checks for subtable type 0xd: GIC MSI Frame */
	fwts_acpi_madt_gic_msi *gic_msi = (fwts_acpi_madt_gic_msi *)data;
	fwts_list_link *item;
	bool found;

	/*
	 * TODO: is there some way to test that the entries found are
	 * for only non-secure MSI frames?
	 */

	if (gic_msi->reserved)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "MADTGICMSIReservedNonZero",
			    "MADT %s reserved field should be zero, "
			    "instead got 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], gic_msi->reserved);
	else
		fwts_passed(fw,
			    "MADT %s reserved field properly set to zero.",
			    madt_sub_names[hdr->type]);

	/*
	 * Check MSI Frame ID against previously found IDs to see if it
	 * is unique.  According to the spec, they must be.
	 */
	found = false;
	fwts_list_foreach(item, &msi_frame_ids) {
		uint32_t *frame_id = fwts_list_data(uint32_t *, item);

		if (*frame_id == gic_msi->frame_id)
			found = true;
	}
	if (found) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTGICMSINonUniqueFrameId",
			    "MADT %s Frame ID 0x%" PRIx32 " is not unique "
			    "and has already be defined in a previous %s.",
			    madt_sub_names[hdr->type],
			    gic_msi->frame_id,
			    madt_sub_names[hdr->type]);
	} else {
		fwts_list_append(&msi_frame_ids, &(gic_msi->frame_id));
		fwts_passed(fw,
			    "MADT %s Frame ID 0x%" PRIx32 " is unique "
			    "as is required.",
			    madt_sub_names[hdr->type],
			    gic_msi->frame_id);
	}

	/*
	 * TODO: can the physical base address be tested, or is zero
	 * allowed?
	 */

	if (gic_msi->flags & 0xfffffffe)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTGICMSIFLags",
			    "MADT %s, flags, bits 1..31 are reserved "
			    "and should be zero, but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type],
			    gic_msi->flags);
	else
		fwts_passed(fw,
			    "MADT %s, flags, bits 1..31 are reserved "
			    "and properly set to zero.",
			    madt_sub_names[hdr->type]);

	/*
	 * TODO: can we check the SPI Count and SPI Base against the MSI_TYPER
	 * register in the frame at this point?  Or is this something that
	 * can only been done when running on the arch we're testing for?
	 */

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_gicr(fwts_framework *fw,
		     fwts_acpi_madt_sub_table_header *hdr,
		     uint8_t *data)
{
	/* specific checks for subtable type 0xe: GICR */
	fwts_acpi_madt_gicr *gicr = (fwts_acpi_madt_gicr *)data;

	/*
	 * TODO: GICR structures should only be used when GICs implement
	 * version 3 or higher.
	 */

	if (gicr->reserved)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "MADTGICRReservedNonZero",
			    "MADT %s reserved field should be zero, "
			    "instead got 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type],
			    gicr->reserved);
	else
		fwts_passed(fw,
			    "MADT %s reserved field properly set to zero.",
			    madt_sub_names[hdr->type]);

	/*
	 * TODO: can Discovery Range Base Address ever be zero?
	 * Or, can we assume it must be non-zero?
	 */

	if (gicr->discovery_range_length == 0)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "SPECMADTGICRZeroLength",
			    "MADT %s discovery range length should be > 0.",
			    madt_sub_names[hdr->type]);
	else
		fwts_passed(fw,
			    "MADT %s discovery range length of %" PRIu32 " > 0.",
			    madt_sub_names[hdr->type],
			    gicr->discovery_range_length);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_gic_its(fwts_framework *fw,
			     fwts_acpi_madt_sub_table_header *hdr,
			     uint8_t *data)
{
	/* specific checks for subtable type 0xf: GIC ITS */
	fwts_acpi_madt_gic_its *gic_its = (fwts_acpi_madt_gic_its *)data;
	fwts_list_link *item;
	bool found;

	if (gic_its->reserved)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "SPECMADTGICITSReservedNonZero",
			    "MADT %s first reserved field should be zero, "
			    "instead got 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], gic_its->reserved);
	else
		fwts_passed(fw,
			    "MADT %s first reserved field is properly set "
			    "to zero.",
			    madt_sub_names[hdr->type]);

	/*
	 * Check ITS ID against previously found IDs to see if it
	 * is unique.  According to the spec, they must be.
	 */
	found = false;
	fwts_list_foreach(item, &its_ids) {
		uint32_t *its_id = fwts_list_data(uint32_t *, item);

		if (*its_id == gic_its->its_id)
			found = true;
	}
	if (found) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTGICITSNonUniqueId",
			    "MADT %s ITS ID 0x%" PRIx32 " is not unique "
			    "and has already be defined in a previous %s.",
			    madt_sub_names[hdr->type],
			    gic_its->its_id,
			    madt_sub_names[hdr->type]);
	} else {
		fwts_list_append(&its_ids, &(gic_its->its_id));
		fwts_passed(fw,
			    "MADT %s ITS ID 0x%" PRIx32 " is unique "
			    "as is required.",
			    madt_sub_names[hdr->type],
			    gic_its->its_id);
	}

	/*
	 * TODO: can the physical base address be tested, or is zero
	 * allowed?
	 */

	if (gic_its->reserved2)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "SPECMADTGICITSReserved2NonZero",
			    "MADT %s second reserved field should be zero, "
			    "instead got 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], gic_its->reserved2);
	else
		fwts_passed(fw,
			    "MADT %s second reserved field is properly set "
			    "to zero.",
			    madt_sub_names[hdr->type]);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_mp_wakup(fwts_framework *fw,
			     fwts_acpi_madt_sub_table_header *hdr,
			     uint8_t *data)
{
	/* specific checks for subtable type 0x10: Multiprocessor Wakeup */
	fwts_acpi_madt_mp_wakeup *mp_wakeup = (fwts_acpi_madt_mp_wakeup *) data;

	if (mp_wakeup->mail_box_version != 0)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "SPECMADTMPWAKEUPVersion",
			    "MADT %s mailbox version should be in 0, "
			    "but instead have 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], mp_wakeup->mail_box_version);
	else
		fwts_passed(fw,
			    "MADT %s mailbox version is in 0.",
			    madt_sub_names[hdr->type]);

	if (mp_wakeup->reserved)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "SPECMADTMPWAKEUPReservedNonZero",
			    "MADT %s reserved field should be zero, "
			    "instead got 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], mp_wakeup->reserved);
	else
		fwts_passed(fw,
			    "MADT %s reserved field is properly set "
			    "to zero.",
			    madt_sub_names[hdr->type]);

	if (mp_wakeup->mail_box_address % 0x1000)
		fwts_failed(fw, LOG_LEVEL_HIGH,
			    "SPECMADTMPWAKEUPAddress",
			    "MADT %s mailbox address should be 4K bytes aligned, "
			    "but instead have 0x%" PRIx64 ".",
			    madt_sub_names[hdr->type], mp_wakeup->mail_box_address);
	else
		fwts_passed(fw,
			    "MADT %s mailbox version is in 0.",
			    madt_sub_names[hdr->type]);



	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_core_pic(fwts_framework *fw,
			     fwts_acpi_madt_sub_table_header *hdr,
			     uint8_t *data)
{

	fwts_acpi_madt_core_pic *core_pic = (fwts_acpi_madt_core_pic *)data;

	if (core_pic->version > 1)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "SPECMADTPicVersion",
			    "MADT %s version should not over 1, "
			    "but instead have 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], core_pic->version);
	else
		fwts_passed(fw,
			    "MADT %s version is properly set.",
			    madt_sub_names[hdr->type]);

	if (core_pic->flag & 0xfffffffe)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTPicFlag",
			    "MADT %s flags, bits 1..31 are reserved "
			    "and should be zero, but are set as: %" PRIx32 ".",
			    madt_sub_names[hdr->type], core_pic->flag);
	else
		fwts_passed(fw,
			    "MADT %s flags, bits 1..31 are reserved "
			    "and set to zero.",
			    madt_sub_names[hdr->type]);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static int madt_general_pic(fwts_framework *fw,
			     fwts_acpi_madt_sub_table_header *hdr,
			     uint8_t *data)
{

	uint8_t *version = (uint8_t *)data;

	if (*version > 1)
		fwts_failed(fw, LOG_LEVEL_LOW,
			    "SPECMADTPicVersion",
			    "MADT %s version should not over 1, "
			    "but instead have 0x%" PRIx32 ".",
			    madt_sub_names[hdr->type], *version);
	else
		fwts_passed(fw,
			    "MADT %s version is properly set.",
			    madt_sub_names[hdr->type]);

	return (hdr->length - sizeof(fwts_acpi_madt_sub_table_header));
}

static void madt_ioapic_sapic_compare(fwts_framework *fw,
				     int num_ioapics,
				     int num_iosapics)
{
	bool failed;
	int ioapic;

	if (!num_iosapics)
		return; /* Nothing to do */

	/*
	 * If both I/O APIC and I/O SAPIC subtables exist, there must be at
	 * least as many I/O SAPIC subtables as I/O APIC subtables.
	 */
	if (num_iosapics < num_ioapics)
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "MADTIOAPICSAPICCOMPARELessThan ",
			    "The number of IO SAPICS (%d) is less than "
			    "the number of IO APICS (%d).",
			    num_iosapics, num_ioapics);
	else
		fwts_passed(fw, "The number of IO SAPICS (%d) is at least"
			    "equal to the number of IO APICS (%d)",
			    num_iosapics, num_ioapics);

	/* Every I/O APIC must have a corresponding I/O SAPIC. */
	failed = false;
	for (ioapic = 0; ioapic < MAX_IO_APIC_ID; ioapic++)
		if (madt_io_apics[ioapic] &&
		    (madt_io_apics[ioapic] != madt_io_sapics[ioapic])) {
			failed = true;
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "MADTIOAPICSAPICCOMPARENoSAPIC ",
				    "IO APIC %d does not have a IO "
				    "SAPIC entry",
				    ioapic);
		}

	if (!failed)
		fwts_passed(fw, "Each IO APIC entry has a corresponding"
			    "SAPIC entry.");
}

static int madt_subtables(fwts_framework *fw)
{
	fwts_acpi_table_madt *madt = (fwts_acpi_table_madt *)mtable->data;
	fwts_acpi_madt_sub_table_header *hdr;
	fwts_acpi_madt_local_sapic *lsapic;
	struct acpi_madt_subtable_lengths *ms = spec_data;
	uint8_t *data = mtable->data;
	ssize_t length = mtable->length;
	int ii = 0;
	int proper_len;
	int num_ioapics = 0;
	int num_iosapics = 0;

	/*
	 * check the correctness of each subtable type, and whether or
	 * not the subtable is allowed for this revision of the MADT
	 */

	data += sizeof(fwts_acpi_table_madt);
	length -= sizeof(fwts_acpi_table_madt);

	if (!ms->num_types) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			    "SPECMADTRevision",
			    "Undefined MADT revision being used: %" PRIu8,
			    madt->header.revision);
	} else {
		fwts_passed(fw, "MADT revision %" PRIu8 " is defined.",
			    madt->header.revision);
	}

	while (length > (ssize_t)sizeof(fwts_acpi_madt_sub_table_header)) {
		ssize_t skip = 0;
		int len = 0;
		int type;
		int offset = 0;

		hdr = (fwts_acpi_madt_sub_table_header *)data;
		ii++;

		data += sizeof(fwts_acpi_madt_sub_table_header);
		offset = (int)(mtable->length - length);
		length -= sizeof(fwts_acpi_madt_sub_table_header);

		/*
		 * set initial type value, will be overridden for OEM and
		 * reserved entries
		 */
		type = hdr->type;

		/* check for OEM and reserved entries */
		if (hdr->type >= NUM_SUBTABLE_TYPES) {
			if (hdr->type < 0x80)
				type = FWTS_MADT_RESERVED;
			else
				type = FWTS_MADT_OEM;
			len = hdr->length;
		} else {
			/* this subtable is defined */
			len = ms->lengths[hdr->type];
		}

		if (!len) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "SPECMADTSubType",
				    "Undefined MADT subtable type for this "
				    "version of the MADT: %" PRIu8 " (%s)",
				    hdr->type, madt_sub_names[type]);
		} else {
			fwts_passed(fw,
				    "MADT subtable type %" PRIu8 " (%s) is defined.",
				    hdr->type, madt_sub_names[type]);
		}

		if (!(fw->flags & FWTS_FLAG_SBBR)) {
			bool passed = true;

			/* verify that the length is what we expect */
			if (len == SUBTABLE_VARIABLE) {
				if (hdr->type == FWTS_MADT_LOCAL_SAPIC) {
					lsapic = (fwts_acpi_madt_local_sapic *)hdr;
					proper_len =
					     sizeof(fwts_acpi_madt_local_sapic) +
					     strlen(lsapic->uid_string) + 1;

					if (proper_len != hdr->length)
						passed = false;
				}
			} else {
				if (hdr->length != len)
					passed = false;
			}
			if (passed) {
				fwts_passed(fw,
					    "Subtable %d (offset 0x%x) of "
					    "type %" PRIu8 " (%s) is the correct length: %" PRIu8,
					    ii, offset, hdr->type,
					    madt_sub_names[type],
					    hdr->length);
			} else {
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					    "SPECMADTSubLen",
					    "Subtable %d (offset 0x%x) of "
					    "type %" PRIu8 " (%s) is %d bytes "
					    "long but should be %d bytes",
					    ii, offset, hdr->type,
					    madt_sub_names[type],
					    hdr->length, len);
			}
		}

		/* perform checks specific to subtable types */
		switch (type) {
		case FWTS_MADT_LOCAL_APIC:
			skip = madt_local_apic(fw, hdr, data);
			break;

		case FWTS_MADT_IO_APIC:
			skip = madt_io_apic(fw, hdr, data);
			num_ioapics++;
			break;

		case FWTS_MADT_INTERRUPT_OVERRIDE:
			skip = madt_interrupt_override(fw, hdr, data);
			break;

		case FWTS_MADT_NMI_SOURCE:
			skip = madt_nmi_source(fw, hdr, data);
			break;

		case FWTS_MADT_LOCAL_APIC_NMI:
			skip = madt_local_apic_nmi(fw, hdr, data);
			break;

		case FWTS_MADT_LOCAL_APIC_OVERRIDE:
			skip = madt_lapic_addr_override(fw, hdr, data);
			break;

		case FWTS_MADT_IO_SAPIC:
			skip = madt_io_sapic(fw, hdr, data);
			num_iosapics++;
			break;

		case FWTS_MADT_LOCAL_SAPIC:
			skip = madt_local_sapic(fw, hdr, data);
			break;

		case FWTS_MADT_INTERRUPT_SOURCE:
			skip = madt_platform_int_source(fw, hdr, data);
			break;

		case FWTS_MADT_LOCAL_X2APIC:
			skip = madt_local_x2apic(fw, hdr, data);
			break;

		case FWTS_MADT_LOCAL_X2APIC_NMI:
			skip = madt_local_x2apic_nmi(fw, hdr, data);
			break;

		case FWTS_MADT_GIC_C_CPU_INTERFACE:
			skip = madt_gicc(fw, hdr, data);
			break;

		case FWTS_MADT_GIC_D_GOC_DISTRIBUTOR:
			skip = madt_gicd(fw, hdr, data);
			break;

		case FWTS_MADT_GIC_V2M_MSI_FRAME:
			skip = madt_gic_msi_frame(fw, hdr, data);
			break;

		case FWTS_MADT_GIC_R_REDISTRIBUTOR:
			skip = madt_gicr(fw, hdr, data);
			break;

		case FWTS_MADT_GIC_ITS:
			skip = madt_gic_its(fw, hdr, data);
			break;

		case FWTS_MADT_MP_WAKEUP:
			skip = madt_mp_wakup(fw, hdr, data);
			break;

		case FWTS_MADT_CORE_PIC:
			skip = madt_core_pic(fw, hdr, data);
			break;

		case FWTS_MADT_LIO_PIC:
		case FWTS_MADT_HT_PIC:
		case FWTS_MADT_EIO_PIC:
		case FWTS_MADT_MSI_PIC:
		case FWTS_MADT_BIO_PIC:
		case FWTS_MADT_LPC_PIC:
			skip = madt_general_pic(fw, hdr, data);
			break;

		case FWTS_MADT_RESERVED:
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "SPECMADTSubReservedID",
				    "MADT subtable %d (offset 0x%x) is "
				    "using the reserved value 0x%" PRIx8 " for a "
				    "type.  Subtable type values 0x%" PRIx8 "..0x7f "
				    "are reserved; 0x80..0xff can be "
				    "used by OEMs.",
				    ii, offset, hdr->type, FWTS_MADT_RESERVED);
			skip = (hdr->length -
				sizeof(fwts_acpi_madt_sub_table_header));
			break;
		case FWTS_MADT_OEM:
			/* OEM entries must be assumed to be valid */
			skip = (hdr->length -
				sizeof(fwts_acpi_madt_sub_table_header));
			break;
		default:
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				    "SPECMADTSubReservedID",
				    "MADT subtable %d (offset 0x%x) is "
				    "using value 0x%" PRIx8 " for a type.  This "
				    "value is out of the expected range "
				    "of 0x00 .. 0xff.",
				    ii, offset, hdr->type);
			skip = (hdr->length -
				sizeof(fwts_acpi_madt_sub_table_header));
			break;
		}

		if (hdr->length == 0) {
			fwts_log_error(fw, "INTERNAL ERROR: "
				       "zero length subtable means something "
				       "is seriously broken. Subtable %d "
				       "(offset 0x%0x) has the problem.",
				       ii, offset);
			break;
		}
		data   += skip;
		length -= skip;
	}

	if (!(fw->flags & FWTS_FLAG_SBBR)) {
		/* run comparison tests */
		madt_ioapic_sapic_compare(fw, num_ioapics, num_iosapics);
	}

	return FWTS_OK;
}

static int madt_deinit(fwts_framework *fw)
{
	fwts_acpica_deinit();

	/* only minor clean up needed */
	fwts_list_free_items(&msi_frame_ids, NULL);
	fwts_list_free_items(&its_ids, NULL);
	fwts_list_free_items(&processor_uids, NULL);

	return (fw) ? FWTS_ERROR : FWTS_OK;
}

static fwts_framework_minor_test madt_tests[] = {
	{ madt_checksum, "MADT checksum test." },
	{ madt_revision, "MADT revision test." },
	{ madt_arch_revision, "MADT architecture minimum revision test." },
	{ madt_flags, "MADT flags field reserved bits test." },
	{ madt_subtables, "MADT subtable tests." },
	{ NULL, NULL }
};

static fwts_framework_ops madt_ops = {
	.description = "MADT Multiple APIC Description Table (spec compliant).",
	.init        = madt_init,
	.deinit      = madt_deinit,
	.minor_tests = madt_tests
};

FWTS_REGISTER("madt", &madt_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_ACPI | FWTS_FLAG_COMPLIANCE_ACPI | FWTS_FLAG_SBBR)

#endif
