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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <sys/klog.h>

#include "fwts.h"

/*
 *  Utilities for the fwts --dump option
 */

/*
 *  dump_data()
 *	dump to path/filename a chunk of data of length len
 */
static int dump_data(const char *filename, char *data, const size_t len)
{
	FILE *fp;

	if ((fp = fopen(filename, "w")) == NULL)
		return FWTS_ERROR;

	if ((fwrite(data, sizeof(char), len, fp) != len)) {
		(void)fclose(fp);
		return FWTS_ERROR;
	}

	(void)fclose(fp);
	return FWTS_OK;
}

/*
 *  dump_dmesg()
 *	read kernel log, dump to path/filename
 */
static int dump_dmesg(void)
{
	int len;
	char *data;
	int ret;

	if ((len = klogctl(10, NULL, 0)) < 0)
		return FWTS_ERROR;

        if ((data = calloc(1, len)) == NULL)
		return FWTS_ERROR;

        if (klogctl(3, data, len) < 0) {
		free(data);
		return FWTS_ERROR;
	}
	ret = dump_data("dmesg.log", data, strlen(data));

	free(data);

	return ret;
}


/*
 *  dump_exec()
 *  	Execute command, dump output to path/filename
 */
static int dump_exec(const char *filename, const char *command)
{
	int fd;
	pid_t pid;
	ssize_t len;
	char *data;
	int ret;

	if (fwts_pipe_open_ro(command, &pid, &fd) < 0)
		return FWTS_ERROR;

	if (fwts_pipe_read(fd, &data, &len) != 0) {
		fwts_pipe_close(fd, pid);
		return FWTS_ERROR;
	}

	fwts_pipe_close(fd, pid);

	ret = dump_data(filename, data, len);

	free(data);

	return ret;
}

#ifdef FWTS_ARCH_INTEL
/*
 *  dump_dmidecode()
 *	run dmidecode, dump output to path/filename
 */
static int dump_dmidecode(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	return dump_exec("dmidecode.log", FWTS_DMIDECODE_PATH);
}
#endif

/*
 *  dump_lspci()
 *	run lspci, dump output to path/filename
 */
static int dump_lspci(fwts_framework *fw)
{
	char command[1024];

	snprintf(command, sizeof(command), "%s -vv -nn", fw->lspci);

	return dump_exec("lspci.log", command);
}

#if defined(FWTS_HAS_ACPI)
/*
 *  dump_acpi_table()
 *	hex dump of a ACPI table
 */
static int dump_acpi_table(fwts_acpi_table_info *table, FILE *fp)
{
	char buffer[128];
	size_t n;

	fprintf(fp, "%s @ 0x%lx\n", table->name, (unsigned long)table->addr);

	for (n = 0; n < table->length; n += 16) {
		int left = table->length - n;
		fwts_dump_raw_data(buffer, sizeof(buffer), table->data + n, n, left > 16 ? 16 : left);
		fprintf(fp, "%s\n", buffer);
	}
	fprintf(fp, "\n");

	return FWTS_OK;
}

/*
 *  dump_acpi_tables()
 *	hex dump all ACPI tables
 */
static int dump_acpi_tables(fwts_framework *fw)
{
	FILE *fp;
	int i;

	if ((fp = fopen("acpidump.log", "w")) == NULL)
		return FWTS_ERROR;

	for (i = 0;; i++) {
		fwts_acpi_table_info *table;

		int ret = fwts_acpi_get_table(fw, i, &table);
		if (ret != FWTS_OK) {
			fprintf(stderr, "Cannot read ACPI tables.\n");
			(void)fclose(fp);
			return ret;
		}
		if (table == NULL)
			break;

		dump_acpi_table(table, fp);
	}
	(void)fclose(fp);

	return FWTS_OK;
}
#endif

/*
 *  dump_readme()
 *	dump README file containing some system info
 */
static int dump_readme(void)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	char *str, buf[64];
	FILE *fp;

	if ((fp = fopen("README.txt", "w")) == NULL)
		return FWTS_ERROR;

	(void)strftime(buf, sizeof(buf), "%a %b %e %T %Y", tm);
	fprintf(fp, "This is output captured by fwts on %s.\n\n", buf);

	fwts_framework_show_version(fp, "fwts");

	if ((str = fwts_get("/proc/version")) != NULL) {
		fprintf(fp, "Version: %s", str);
		free(str);
	}

	if ((str = fwts_get("/proc/version_signature")) != NULL) {
		fprintf(fp, "Signature: %s", str);
		free(str);
	}

	(void)fclose(fp);

	return FWTS_OK;
}

/*
 *  dump_cpuinfo()
 *	read cpuinfo, dump to path/filename
 */
static int dump_cpuinfo(void)
{
	FILE *source, *target;
	char buffer[1024];
	size_t bytes;

	if ((source = fopen("/proc/cpuinfo", "r")) == NULL)
		return FWTS_ERROR;

	if ((target = fopen("cpuinfo.log", "w")) == NULL) {
		(void)fclose(source);
		return FWTS_ERROR;
	}

	while (0 < (bytes = fread(buffer, 1, sizeof(buffer), source)))
		fwrite(buffer, 1, bytes, target);

	(void)fclose(source);
	(void)fclose(target);

	return FWTS_OK;
}

/*
 *  fwts_dump_info()
 *	dump various system specific information:
 *	kernel log, dmidecode output, lspci output,
 *	ACPI tables
 */
int fwts_dump_info(fwts_framework *fw)
{
	char path[PATH_MAX+1];

	if (getcwd(path, PATH_MAX) == NULL)
		strcpy(path, "./");

	if (access(path, W_OK) < 0) {
		fprintf(stderr, "No write access to %s.\n", path);
		return FWTS_ERROR;
	}

	if (dump_readme() != FWTS_OK)
		fprintf(stderr, "Failed to dump README.txt.\n");
	else
		printf("Created README.txt\n");

	if (dump_dmesg() != FWTS_OK)
		fprintf(stderr, "Failed to dump kernel log.\n");
	else
		printf("Dumping dmesg to dmesg.log\n");

#ifdef FWTS_ARCH_INTEL
	if (fwts_check_root_euid(fw, false) == FWTS_OK) {
		if (dump_dmidecode(fw) != FWTS_OK)
			fprintf(stderr, "Failed to dump output from dmidecode.\n");
		else
			printf("Dumped DMI data to dmidecode.log\n");
	} else
		fprintf(stderr, "Need root privilege to dump DMI tables.\n");
#endif

	if (dump_lspci(fw) != FWTS_OK)
		fprintf(stderr, "Failed to dump output from lspci.\n");
	else
		printf("Dumped lspci data to lspci.log\n");

#if defined(FWTS_HAS_ACPI)
	switch (dump_acpi_tables(fw)) {
		case FWTS_OK:
			printf("Dumped ACPI tables to acpidump.log\n");
			break;
		case FWTS_ERROR_NO_PRIV:
			fprintf(stderr, "Need root privilege to dump ACPI tables.\n");
			break;
		default:
			fprintf(stderr, "Failed to dump ACPI tables.\n");
			break;
	}
#endif

	if (dump_cpuinfo() != FWTS_OK)
		fprintf(stderr, "Failed to dump cpuinfo.\n");
	else
		printf("Dumping cpuinfo to cpuinfo.log\n");

	return FWTS_OK;
}
