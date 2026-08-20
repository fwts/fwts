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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>

#include "fwts.h"

#include "fwts_iasl_interface.h"
#include "fwts_acpica.h"

/* For ACPICA interface */
static char *iasl_cached_table_filename[ACPI_MAX_TABLES];
static char *iasl_cached_table_name[ACPI_MAX_TABLES];

static bool iasl_init = false;
static int cached_max = 0;

/*
 *  fwts_iasl_dump_aml_to_file()
 *	write AML data of given length to file amlfile.
 */
static int fwts_iasl_dump_aml_to_file(
	fwts_framework *fw,
	const uint8_t *data,
	const int length,
	const char *filename)
{
	int fd;

	/* Dump the AML bytecode into a tempoary file so we can disassemble it */
	if ((fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) < 0) {
		fwts_log_error(fw, "Cannot create temporary file %s", filename);
		return FWTS_ERROR;
	}

	if (write(fd, data, length) != length) {
		fwts_log_error(fw, "Cannot write all data to temporary file");
		(void)close(fd);
		(void)unlink(filename);
		return FWTS_ERROR;
	}
	(void)close(fd);

	return FWTS_OK;
}

/*
 *  fwts_iasl_cache_tables_to_file()
 *	to disassemble an APCPI table we need to dump it
 *	to file. To save effort in saving these to file
 *	multiple times, we dump out all the tables and
 *	cache the references to these.
 */
static int fwts_iasl_cache_tables_to_file(fwts_framework *fw)
{
	pid_t pid = getpid();
	char tmpname[PATH_MAX];
	fwts_acpi_table_info *table;

	for (cached_max = 0; cached_max < ACPI_MAX_TABLES; cached_max++) {
		int ret = fwts_acpi_get_table(fw, cached_max, &table);
		if (ret != FWTS_OK)
			return ret;
		if (table == NULL)
			continue;

		snprintf(tmpname, sizeof(tmpname),
			"/tmp/fwts_tmp_table_%d_%s_%d.dsl",
			pid, table->name, cached_max);
		iasl_cached_table_filename[cached_max] = strdup(tmpname);
		iasl_cached_table_name[cached_max] = table->name;
		if (iasl_cached_table_filename[cached_max] == NULL) {
			fwts_log_error(fw, "Cannot allocate cached table file name.");
			return FWTS_ERROR;
		}
		if (fwts_iasl_dump_aml_to_file(fw, table->data, table->length, tmpname) != FWTS_OK) {
			free(iasl_cached_table_filename[cached_max]);
			iasl_cached_table_filename[cached_max] = NULL;
			iasl_cached_table_name[cached_max] = NULL;
		}
	}
	return FWTS_OK;
}

/*
 *  fwts_iasl_deinit()
 *	clean up cached files and references
 */
void fwts_iasl_deinit(void)
{
	int i;

	for (i = 0; i < cached_max; i++) {
		if (iasl_cached_table_filename[i]) {
			(void)unlink(iasl_cached_table_filename[i]);
			free(iasl_cached_table_filename[i]);
		}
		iasl_cached_table_filename[i] = NULL;
		iasl_cached_table_name[i] = NULL;
	}
	memset(iasl_cached_table_filename, 0, sizeof(iasl_cached_table_filename));
	cached_max = 0;
}

/*
 *  fwts_iasl_init()
 *	initialise iasl - cache DSDT and SSDT to file
 */
int fwts_iasl_init(fwts_framework *fw)
{
	int ret;

	cached_max = 0;
	fwts_iasl_deinit();	/* Ensure it is clean */

	memset(iasl_cached_table_filename, 0, sizeof(iasl_cached_table_filename));

	ret = fwts_iasl_cache_tables_to_file(fw);
	if (ret != FWTS_OK)
		return ret;

	iasl_init = true;

	return FWTS_OK;
}

/*
 *  fwts_iasl_disassemble_to_file()
 *	Disassemble a given table and dump disassembly to a file.
 */
static int fwts_iasl_disassemble_to_file(fwts_framework *fw,
	const fwts_acpi_table_info *info,
	const bool use_externals,
	const char *filename)
{
	if (!iasl_init)
		return FWTS_ERROR;

	fwts_acpica_set_fwts_framework(fw);

	if (fwts_iasl_disassemble_aml(
		iasl_cached_table_filename,
		iasl_cached_table_name,
		cached_max, info->index, use_externals, filename) < 0)
		return FWTS_ERROR;

	return FWTS_OK;
}

/*
 *  fwts_iasl_disassemble()
 *	Disassemble a given table and dump disassembly list of strings.
 *
 */
int fwts_iasl_disassemble(fwts_framework *fw,
	const fwts_acpi_table_info *info,
	const bool use_externals,
	fwts_list **iasl_output)
{
	char tmpfile[PATH_MAX];
	int pid = getpid();
	int ret;

	if (!iasl_init)
		return FWTS_ERROR;
	if (iasl_output == NULL)
		return FWTS_ERROR;

	*iasl_output = NULL;

	snprintf(tmpfile, sizeof(tmpfile),
		"/tmp/fwts_iasl_disassemble_%d_%s_%d.dsl",
		pid, info->name, info->index);

	if ((ret = fwts_iasl_disassemble_to_file(fw, info, use_externals, tmpfile)) != FWTS_OK)
		return ret;

	*iasl_output = fwts_file_open_and_read(tmpfile);
	(void)unlink(tmpfile);

	return *iasl_output ? FWTS_OK : FWTS_ERROR;
}


/*
 *  fwts_iasl_disassemble_all_to_file()
 * 	Disassemble DSDT and SSDT tables to separate files.
 */
int fwts_iasl_disassemble_all_to_file(
	fwts_framework *fw,
	const char *path)
{
	int i, j, ret;
	char filename[PATH_MAX + 18];
	char pathname[PATH_MAX];

	ret = fwts_iasl_init(fw);
	if (ret == FWTS_ERROR_NO_PRIV) {
		fprintf(stderr, "Need to have root privilege to read ACPI tables from memory! Re-run using sudo.\n");
		return FWTS_ERROR;
	}
	if (ret != FWTS_OK) {
		fprintf(stderr, "Could not initialise disassembler.\n");
		return FWTS_ERROR;
	}

	if (path == NULL)
		*pathname = '\0';
	else
		snprintf(pathname, sizeof(pathname), "%s/", path);

	for (i = 0, j = 0; i < cached_max; i++) {
		fwts_acpi_table_info *info;

		ret = fwts_acpi_get_table(fw, i, &info);
		if (ret != FWTS_OK)
			break;
		if (info && info->has_aml) {
			snprintf(filename, sizeof(filename), "%s%s%d.dsl",
				pathname, info->name, j);
			if (fwts_iasl_disassemble_to_file(fw, info, true, filename) != FWTS_OK)
				fprintf(stderr, "Could not disassemble %s\n", info->name);
			else
				printf("Disassembled %s to %s\n", info->name, filename);
			j++;
		}
	}
	fwts_iasl_deinit();

	return FWTS_OK;
}

/*
 *  fwts_iasl_reassemble()
 *	given a ACPI table go and disassemble it
 *	and re-assemble it.  Dump the disassembly into list iasl_disassembly and
 * 	any re-assembly errors into list iasl_errors.
 */
int fwts_iasl_reassemble(fwts_framework *fw,
	const fwts_acpi_table_info *info,
	fwts_list **iasl_disassembly,
	fwts_list **iasl_stdout,
	fwts_list **iasl_stderr)
{
	char tmpfile[PATH_MAX];
	char *stdout_output = NULL, *stderr_output = NULL;
	int pid = getpid();

	if ((!iasl_init) ||
	    (iasl_disassembly == NULL) ||
	    (iasl_stdout == NULL) ||
	    (iasl_stderr == NULL) ||
	    (info == NULL))
		return FWTS_ERROR;

	fwts_acpica_set_fwts_framework(fw);
	*iasl_disassembly = NULL;
	snprintf(tmpfile, sizeof(tmpfile), "/tmp/fwts_iasl_reassemble_%d.dsl", pid);

	if (fwts_iasl_disassemble_aml(
		iasl_cached_table_filename,
		iasl_cached_table_name,
		cached_max, info->index, true, tmpfile) < 0) {
		(void)unlink(tmpfile);
		return FWTS_ERROR;
	}

	/* Read in the disassembled text to return later */
	*iasl_disassembly = fwts_file_open_and_read(tmpfile);

	/* Now we have a disassembled source in tmpfile, so let's assemble it */
	if (fwts_iasl_assemble_aml(tmpfile, &stdout_output, &stderr_output) < 0) {
		(void)unlink(tmpfile);
		free(stdout_output);
		return FWTS_ERROR;
	}

	/* Remove these now we don't need them */
	(void)unlink(tmpfile);

	/* And remove aml file generated from ACPICA compiler */
	snprintf(tmpfile, sizeof(tmpfile), "/tmp/fwts_iasl_reassemble_%d.aml", pid);
	(void)unlink(tmpfile);

	*iasl_stdout = fwts_list_from_text(stdout_output);
	*iasl_stderr = fwts_list_from_text(stderr_output);
	free(stdout_output);

	return FWTS_OK;
}

const char *fwts_iasl_exception_level(uint8_t level)
{
	return fwts_iasl_exception_level__(level);
}
