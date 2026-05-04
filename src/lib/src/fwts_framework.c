/*
 * Copyright (C) 2010-2026 Canonical
 * Some of this work - Copyright (C) 2016-2025 IBM
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <sys/utsname.h>
#include <sys/time.h>

#include "fwts.h"
#include "fwts_pm_method.h"

#ifdef HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

typedef struct {
	const char *title;		/* Test category */
	fwts_framework_flags flag;	/* Mask of category */
} fwts_categories;

typedef struct {
	const char *name;
	const fwts_log_level filter_level;
} fwts_log_levels;

/* Suffix ".log", ".xml", etc gets automatically appended */
#define RESULTS_LOG	"results"

#define FWTS_FLAG_RUN_ALL			\
	(fwts_framework_flags)			\
	(FWTS_FLAG_BATCH |			\
	 FWTS_FLAG_INTERACTIVE |		\
	 FWTS_FLAG_BATCH_EXPERIMENTAL |		\
	 FWTS_FLAG_INTERACTIVE_EXPERIMENTAL |	\
	 FWTS_FLAG_POWER_STATES |		\
	 FWTS_FLAG_UTILS |			\
	 FWTS_FLAG_UNSAFE |			\
	 FWTS_FLAG_UEFI |			\
	 FWTS_FLAG_ACPI |			\
	 FWTS_FLAG_COMPLIANCE_ACPI |		\
	 FWTS_FLAG_SBBR |			\
	 FWTS_FLAG_EBBR)

static const fwts_categories categories[] = {
	{ "ACPI",			FWTS_FLAG_ACPI },
	{ "Batch",			FWTS_FLAG_BATCH },
	{ "Interactive",		FWTS_FLAG_INTERACTIVE },
	{ "Batch Experimental",		FWTS_FLAG_BATCH_EXPERIMENTAL },
	{ "Interactive Experimental",	FWTS_FLAG_INTERACTIVE_EXPERIMENTAL },
	{ "Power States",		FWTS_FLAG_POWER_STATES },
	{ "SBBR",			FWTS_FLAG_SBBR },
	{ "EBBR",			FWTS_FLAG_EBBR },
	{ "Utilities",			FWTS_FLAG_UTILS },
	{ "Unsafe",			FWTS_FLAG_UNSAFE },
	{ "UEFI",			FWTS_FLAG_UEFI },
	{ "ACPI Spec Compliance",	FWTS_FLAG_COMPLIANCE_ACPI },
	{ NULL,				0 },
};

static const fwts_log_levels log_levels[] = {
	{ "critical",			LOG_LEVEL_CRITICAL },
	{ "high",			LOG_LEVEL_CRITICAL | LOG_LEVEL_HIGH },
	{ "medium",			LOG_LEVEL_CRITICAL | LOG_LEVEL_HIGH | LOG_LEVEL_MEDIUM },
	{ "low",			LOG_LEVEL_CRITICAL | LOG_LEVEL_HIGH | LOG_LEVEL_MEDIUM | LOG_LEVEL_LOW },
	{ "info",			LOG_LEVEL_CRITICAL | LOG_LEVEL_HIGH | LOG_LEVEL_MEDIUM | LOG_LEVEL_LOW | LOG_LEVEL_INFO },
	{ "all",			LOG_LEVEL_ALL },
	{ NULL,				0 },
};

static fwts_list tests_to_skip;

static fwts_option fwts_framework_options[] = {
	{ "stdout-summary", 	"",   0, "Output SUCCESS or FAILED to stdout at end of tests." },
	{ "help", 		"h?", 0, "Get help." },
	{ "results-output", 	"r:", 1, "Output results to a named file. Filename can also be stout or stderr, e.g. --results-output=myresults.log,  -r stdout." },
	{ "results-no-separators", "", 0, "No horizontal separators in results log." },
	{ "log-filter", 	"",   1, "Define filters to dump out specific log fields: --log-filter=RES,SUM - dump out results and summary, --log-filter=ALL,~INF - dump out all fields except info fields." },
	{ "log-fields", 	"",   0, "Show available log filtering fields." },
	{ "log-format", 	"",   1, "Define output log format:  e.g. --log-format=\"%date %time [%field] (%owner): \".  Fields are: %time - time, %field - filter field, %owner - name of test, %level - failure error level, %line - log line number." },
	{ "show-progress", 	"p",  0, "Output test progress report to stderr." },
	{ "show-tests", 	"s",  0, "Show available tests." },
	{ "klog", 		"k:", 1, "Specify kernel log file rather than reading it from the kernel, e.g. --klog=dmesg.log" },
	{ "olog",		"o:", 1, "Specify Other logs to be analyzed, main usage is for custom log analysis, best to use custom json file for pattern matching, e.g. -o /var/log/my_opal_msglog --json-data-file=olog.json --json-data-path=/home/myuser, on PPC this will default to dumping the OPAL msglog for analysis." },
	{ "log-width", 		"w:", 1, "Define the output log width in characters." },
	{ "lspci", 		"",   1, "Specify path to lspci, e.g. --lspci=path." },
	{ "batch", 		"b",  0, "Run non-Interactive tests." },
	{ "interactive", 	"i",  0, "Just run Interactive tests." },
	{ "force-clean", 	"f",  0, "Force a clean results log file." },
	{ "version", 		"v",  0, "Show version (" FWTS_VERSION ")." },
	{ "dump", 		"d",  0, "Dump out dmesg, dmidecode, lspci, ACPI tables to logs." },
	{ "table-path", 	"t:", 1, "Path to ACPI tables dumped by acpidump and then acpixtract, e.g. --table-path=/some/path/to/acpidumps" },
	{ "batch-experimental", "",   0, "Run Batch Experimental tests." },
	{ "interactive-experimental", "", 0, "Just run Interactive Experimental tests." },
	{ "power-states", 	"P",  0, "Test S3, S4 power states." },
	{ "all", 		"a",  0, "Run all tests." },
	{ "show-progress-dialog","D", 0, "Output test progress for use in dialog tool." },
	{ "skip-test", 		"S:", 1, "Skip listed tests, e.g. --skip-test=s3,nx,method" },
	{ "quiet", 		"q",  0, "Run quietly." },
	{ "dumpfile", 		"",   1, "Load ACPI tables using file generated by acpidump, e.g. --dumpfile=acpidump.dat" },
	{ "show-tests-full", 	"",   0, "Show available tests including all minor tests." },
	{ "utils", 		"u",  0, "Run Utility 'tests'." },
	{ "json-data-path", 	"j:", 1, "Specify path to fwts json data files - default is /usr/share/fwts." },
	{ "json-data-file",	"J:", 1, "Specify the file to use for pattern matching on --olog, you may need to specify the json-data-path also if non-default location." },
	{ "disassemble-aml", 	"",   2, "Disassemble AML from DSDT and SSDT tables." },
	{ "log-type",		"",   1, "Specify log type (plaintext, json, html or xml)." },
	{ "unsafe",		"U",  0, "Unsafe tests (tests that can potentially cause kernel oopses)." },
	{ "filter-error-discard", "", 1, "Discard errors that match any of the specified labels." },
	{ "filter-error-keep",	"",   1, "Keep errors that match any of the specified labels." },
	{ "acpica-debug",	"",   0, "Enable ACPICA debug/warning messages." },
	{ "acpica",		"",   1, "Enable ACPICA run time options." },
	{ "uefitests",		"",   0, "Run UEFI tests." },
	{ "rsdp",		"R:", 1, "Specify the physical address of the ACPI RSDP." },
	{ "pm-method",  "",   1, "Select the power method to use. Accepted values are \"logind\", \"pm-utils\", \"sysfs\""},
	{ "show-tests-categories","", 0, "Show tests and associated categories." },
	{ "acpitests",		"",   0, "Run general ACPI tests." },
	{ "acpicompliance",	"",   0, "Run ACPI tests for spec compliance." },
	{ "log-level",		"",   1, "Specify error level to report failed test messages," },
	{ "arch",		"",   1, "Specify arch of the tables being tested (defaults to current host)." },
	{ "sbbr",		"",   0, "Run ARM SBBR tests." },
	{ "ifv",		"",   0, "Run tests in firmware-vendor modes." },
	{ "clog",		"",   1, "Specify a coreboot logfile dump" },
	{ "ebbr",		"",   0, "Run EBBR tests." },
	{ NULL, NULL, 0, NULL }
};

static fwts_list fwts_framework_test_list = FWTS_LIST_INIT;

static const char *fwts_copyright[] = {
	"Some of this work - Copyright (c) 1999 - 2026, Intel Corp. All rights reserved.",
	"Some of this work - Copyright (c) 2010 - 2026, Canonical.",
	"Some of this work - Copyright (c) 2016 - 2026, IBM.",
	"Some of this work - Copyright (c) 2017 - 2026, ARM Ltd.",
	NULL
};

/*
 *  fwts_framework_compare_priority()
 *	used to register tests sorted on run priority
 */
static int fwts_framework_compare_priority(void *data1, void *data2)
{
	fwts_framework_test *test1 = (fwts_framework_test *)data1;
	fwts_framework_test *test2 = (fwts_framework_test *)data2;

	return (test1->priority - test2->priority);
}

/*
 * fwts_framework_test_add()
 *    register a test, called by FWTS_REGISTER() macro.
 *    this is called very early, so any errors need to
 *    be reported to stderr because the logging engine
 *    is not set up yet.
 */
void fwts_framework_test_add(
	const char *name,
	fwts_framework_ops *ops,
	const fwts_priority priority,
	const fwts_framework_flags flags,
	const fwts_firmware_feature fw_features)
{
	fwts_framework_test *new_test;

	if (flags & ~(FWTS_FLAG_RUN_ALL | FWTS_FLAG_ROOT_PRIV)) {
		fprintf(stderr, "Test %s flags must be a bit field in 0x%x, got 0x%x\n",
			name, FWTS_FLAG_RUN_ALL, flags);
		exit(EXIT_FAILURE);
	}

	/* This happens early, so if it goes wrong, bail out */
	if ((new_test = calloc(1, sizeof(fwts_framework_test))) == NULL) {
		fprintf(stderr, "FATAL: Could not allocate memory adding tests to test framework\n");
		exit(EXIT_FAILURE);
	}

	/* Total up minor tests in this test */
	for (ops->total_tests = 0;
	     ops->minor_tests[ops->total_tests].test_func != NULL;
	     ops->total_tests++)
		;

	new_test->name = name;
	new_test->ops  = ops;
	new_test->priority = priority;
	new_test->flags = flags;
	new_test->fw_features = fw_features;

	/* Add test, sorted on run order priority */
	fwts_list_add_ordered(&fwts_framework_test_list, new_test, fwts_framework_compare_priority);

	/* Add any options and handler, if they exists */
	if (ops->options && ops->options_handler) {
		if (fwts_args_add_options(ops->options, ops->options_handler,
			ops->options_check) == FWTS_ERROR) {
			fprintf(stderr, "FATAL: Could not allocate memory "
				"for getopt options handler.");
			exit(EXIT_FAILURE);
		}
	}
}

/*
 *  fwts_framework_compare_name()
 *	for sorting tests in name order
 */
int fwts_framework_compare_test_name(void *data1, void *data2)
{
	fwts_framework_test *test1 = (fwts_framework_test *)data1;
	fwts_framework_test *test2 = (fwts_framework_test *)data2;

	return strcmp(test1->name, test2->name);
}

/*
 *  fwts_framework_show_tests_brief()
 *	dump out registered tests in brief form
 */
static void fwts_framework_show_tests_brief(void)
{
	fwts_list sorted;
	fwts_list_link *item;
	size_t n = 0;
	size_t width = (size_t)fwts_tty_width(fileno(stderr), 80);

	fwts_list_init(&sorted);

	fwts_list_foreach(item, &fwts_framework_test_list) {
		fwts_list_add_ordered(&sorted,
			fwts_list_data(fwts_framework_test *, item),
			fwts_framework_compare_test_name);
	}

	fwts_list_foreach(item, &sorted) {
		fwts_framework_test *test = fwts_list_data(fwts_framework_test*, item);
		size_t len = strlen(test->name) + 1;
		if ((n + len) > width)  {
			fprintf(stderr, "\n");
			n = 0;
		}

		fprintf(stderr, "%s ", test->name);
		n += len;
	}
	fwts_list_free_items(&sorted, NULL);
	fprintf(stderr, "\n\nuse: fwts --show-tests or fwts --show-tests-full for more information.\n");
}

/*
 *  fwts_framework_show_tests_categories()
 *	dump out registered tests in brief form with categories
 */
static void fwts_framework_show_tests_categories(void)
{
	fwts_list sorted;
	fwts_list_link *item;

	fwts_list_init(&sorted);

	fwts_list_foreach(item, &fwts_framework_test_list) {
		fwts_list_add_ordered(&sorted,
			fwts_list_data(fwts_framework_test *, item),
			fwts_framework_compare_test_name);
	}

	fwts_list_foreach(item, &sorted) {
		fwts_framework_test *test = fwts_list_data(fwts_framework_test*, item);
		int i, n = 0;

		printf("%-17.17s", test->name);

		for (i = 0; categories[i].title != NULL; i++) {
			if (categories[i].flag & test->flags) {
				const char *src = (const char *)categories[i].title;
				char *dst;
				size_t len = strlen(src) + 1;
				char buf[len];

				for (dst = buf; *src; src++, dst++)
					*dst = tolower(*src);
				*dst = '\0';

				printf("%s%s",
					n == 0 ? " " : ", ", buf);
				n++;
			}
		}
		putchar('\n');
	}
	fwts_list_free_items(&sorted, NULL);
}

/*
 *  fwts_framework_show_tests()
 *	dump out registered tests.
 */
static void fwts_framework_show_tests(fwts_framework *fw, const bool full)
{
	fwts_list_link *item;
	fwts_list sorted;
	int i;
	bool need_nl = false;
	int total = 0;

	/* Dump out tests registered under all categories */
	for (i = 0; categories[i].title != NULL; i++) {

		/* If no category flags are set, or category matches user requested
		   category go and dump name and purpose of tests */
		if (((fw->flags & FWTS_FLAG_RUN_ALL) == 0) ||
		    ((fw->flags & FWTS_FLAG_RUN_ALL) & categories[i].flag)) {
			fwts_framework_test *test;

			fwts_list_init(&sorted);
			fwts_list_foreach(item, &fwts_framework_test_list) {

				test = fwts_list_data(fwts_framework_test *, item);
				if ((test->flags & FWTS_FLAG_RUN_ALL) & categories[i].flag)
					fwts_list_add_ordered(&sorted, test,
						fwts_framework_compare_test_name);
			}

			if (fwts_list_len(&sorted) > 0) {
				if (need_nl)
					printf("\n");
				need_nl = true;
				printf("%s%s:\n", categories[i].title,
					(categories[i].flag & FWTS_FLAG_UTILS) ? "" : " tests");

				fwts_list_foreach(item, &sorted) {
					test = fwts_list_data(fwts_framework_test *, item);
					if (full) {
						int j;
						printf(" %-15.15s (%d test%s):\n",
							test->name, test->ops->total_tests,
							test->ops->total_tests > 1 ? "s" : "");
						for (j = 0; j < test->ops->total_tests; j++)
							printf("  %s\n", test->ops->minor_tests[j].name);
						total += test->ops->total_tests;
					}
					else {
						printf(" %-15.15s %s\n", test->name,
							test->ops->description ? test->ops->description : "");
					}
				}
			}
			fwts_list_free_items(&sorted, NULL);
		}
	}
}

/*
 *  fwts_framework_strtrunc()
 *	truncate overlong string
 */
static void fwts_framework_strtrunc(char *dest, const char *src, size_t max)
{
	if (src) {
		strlcpy(dest, src, max);
		if ((max > 3) && (strlen(src) > max)) {
			dest[max-1] = '\0';
			dest[max-2] = '.';
			dest[max-3] = '.';
		}
	} else
		strncpy(dest, "", max);
}

/*
 *  fwts_framework_format_results()
 *	format results into human readable summary.
 */
static void fwts_framework_format_results(char *buffer, int buflen, fwts_results const *results, bool include_zero_results)
{
	size_t n = 0;

	if (buflen)
		*buffer = 0;

	if ((include_zero_results || (results->passed > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%u passed", results->passed);
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->failed > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%s%u failed", n > 0 ? ", " : "", results->failed);
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->warning > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%s%u warning%s",
			n > 0 ? ", " : "", results->warning,
			results->warning > 1 ? "s" : "");
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->aborted > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%s%u aborted", n > 0 ? ", " : "", results->aborted);
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->skipped > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%s%u skipped", n > 0 ? ", " : "", results->skipped);
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->infoonly > 0)) && (buflen > 0)) {
		snprintf(buffer, buflen, "%s%u info only", n > 0 ? ", " : "", results->infoonly);
	}
}

static void fwts_framework_minor_test_progress_clear_line(void)
{
	int width = fwts_tty_width(fileno(stderr), 80);
	if (width > 256)
		width = 256;
	fprintf(stderr, "%*.*s\r", width-1, width-1, "");
}

/*
 *  fwts_framework_minor_test_progress()
 *	output per test progress report or progress that can be pipe'd into
 *	dialog --guage
 *
 */
void fwts_framework_minor_test_progress(fwts_framework *fw, const int percent, const char *message)
{
	float major_percent;
	float minor_percent;
	float process_percent;
	float progress;
	int width = fwts_tty_width(fileno(stderr), 80);
	if (width > 256)
		width = 256;

	if (percent >= 0 && percent <= 100)
		fw->minor_test_progress = percent;

	major_percent = fw->major_tests_total ? (float)100.0 / (float)fw->major_tests_total : 100.0;
	minor_percent = fw->current_major_test->ops->total_tests ?
			((float)major_percent / (float)fw->current_major_test->ops->total_tests) : 0.0;
	process_percent = ((float)minor_percent / 100.0);

	progress = (float)(fw->current_major_test_num-1) * major_percent;
	progress += (float)(fw->current_minor_test_num-1) * minor_percent;
	progress += (float)(percent) * process_percent;

	/* Feedback required? */
	if (fw->show_progress) {
		char buf[1024];
		char truncbuf[256];
		static int index;

		snprintf(buf, sizeof(buf), "%s %s",fw->current_minor_test_name, message);
		fwts_framework_strtrunc(truncbuf, buf, width-9);

		fprintf(stderr, "  %-*.*s: %5.1f%% %c\r",
			width-13, width-13, truncbuf, progress, "/-\\|"[index++ & 3]);
		fflush(stderr);
	}

	/* Output for the dialog tool, dialog --title "fwts" --gauge "" 12 80 0 */
	if (fw->flags & FWTS_FLAG_SHOW_PROGRESS_DIALOG) {
		char buffer[128];
		static struct timeval start_time;
		struct timeval now_time;
		int duration;

		gettimeofday(&now_time, NULL);
		if (!start_time.tv_sec)
			start_time = now_time;
		duration = (int)(now_time.tv_sec - start_time.tv_sec);

		fwts_framework_format_results(buffer, sizeof(buffer), &fw->total, true);

		fprintf(stdout, "XXX\n");
		fprintf(stdout, "%d\n", percent);
		fprintf(stdout, "%s.\n", buffer);
		fprintf(stdout, "%5.2f%% total run complete (%d seconds).\n",
			progress, duration);
		fprintf(stdout, "%s\n\n", fw->current_major_test->ops->description ?
			fw->current_major_test->ops->description : "");
		fprintf(stdout, "Running test #%d of %d: %s\n",
			fw->current_major_test_num,
			fw->major_tests_total,
			fw->current_minor_test_name);
		fprintf(stdout, "XXX\n");
		fflush(stdout);
	}
}

/*
 *  fwts_framework_underline()
 *	underlining into log
 */
static inline void fwts_framework_underline(fwts_framework *fw, const int ch)
{
	fwts_log_underline(fw->results, ch);
}

static int fwts_framework_test_summary(fwts_framework *fw)
{
	char buffer[128];

	fwts_results const *results = &fw->current_major_test->results;

	fwts_framework_underline(fw,'=');
	fwts_framework_format_results(buffer, sizeof(buffer), results, true);
	fwts_log_summary(fw, "%s.", buffer);
	fwts_framework_underline(fw,'=');

	if (fw->flags & FWTS_FLAG_STDOUT_SUMMARY) {
		/* Report in order of failure precedence */
		if (results->failed > 0) {
			/* We intentionally report the highest logged error level */
			if (fw->failed_level & LOG_LEVEL_CRITICAL)
				printf("%s_CRITICAL\n", fwts_log_field_to_str_upper(LOG_FAILED));
			else if (fw->failed_level & LOG_LEVEL_HIGH)
				printf("%s_HIGH\n", fwts_log_field_to_str_upper(LOG_FAILED));
			else if (fw->failed_level & LOG_LEVEL_MEDIUM)
				printf("%s_MEDIUM\n", fwts_log_field_to_str_upper(LOG_FAILED));
			else if (fw->failed_level & LOG_LEVEL_LOW)
				printf("%s_LOW\n", fwts_log_field_to_str_upper(LOG_FAILED));
			else printf("%s\n", fwts_log_field_to_str_upper(LOG_FAILED));
		} else if (results->skipped > 0)
			printf("%s\n", fwts_log_field_to_str_upper(LOG_SKIPPED));
		else if (results->warning > 0)
			printf("%s\n", fwts_log_field_to_str_upper(LOG_WARNING));
		else if (results->aborted > 0)
			printf("%s\n", fwts_log_field_to_str_upper(LOG_ABORTED));
		else
			printf("%s\n", fwts_log_field_to_str_upper(LOG_PASSED));
	}

	fwts_log_newline(fw->results);

	return FWTS_OK;
}

static int fwts_framework_total_summary(fwts_framework *fw)
{
	char buffer[128];

	fwts_framework_format_results(buffer, sizeof(buffer), &fw->total, true);
	fwts_log_summary(fw, "%s.", buffer);

	return FWTS_OK;
}

static int fwts_framework_run_test(fwts_framework *fw, fwts_framework_test *test)
{
	fwts_framework_minor_test *minor_test;
	int ret;

	fw->current_major_test = test;
	fw->current_minor_test_name = "";

	test->was_run = true;
	fw->total_run++;

	fwts_results_zero(&fw->current_major_test->results);

	fw->failed_level = 0;

	fwts_log_section_begin(fw->results, test->name);
	fwts_log_set_owner(fw->results, test->name);

	fw->current_minor_test_num = 1;
	fw->show_progress = (fw->flags & FWTS_FLAG_SHOW_PROGRESS) &&
			    (FWTS_TEST_INTERACTIVE(test->flags) == 0);

	/* Not a utility test?, then we require a test summary at end of the test run */
	if (!(test->flags & FWTS_FLAG_UTILS))
		fw->print_summary = true;

	if (test->ops->description) {
		fwts_log_heading(fw, "%s: %s", test->name, test->ops->description);
		fwts_framework_underline(fw,'-');
		if (fw->show_progress) {
			char buf[70];
			fwts_framework_strtrunc(buf, test->ops->description, sizeof(buf));
			fprintf(stderr, "Test: %-70.70s\n", buf);
		}
	}

	fwts_framework_minor_test_progress(fw, 0, "");

	if ((test->flags & FWTS_FLAG_ROOT_PRIV) &&
	    (fwts_check_root_euid(fw, true) != FWTS_OK)) {
		fwts_log_error(fw, "Aborted test, insufficient privilege.");
		fw->current_major_test->results.aborted += test->ops->total_tests;
		fw->total.aborted += test->ops->total_tests;
		if (fw->show_progress) {
			fwts_framework_minor_test_progress_clear_line();
			fprintf(stderr, " Test aborted\n");
		}
		goto done;
	}

	if (!fwts_firmware_has_features(test->fw_features)) {
		int missing = test->fw_features & ~fwts_firmware_features();
		static const char *msg = "Test skipped, missing features";

		fwts_log_info(fw, "Test skipped, missing features: %s",
			fwts_firmware_feature_string(missing));
		fw->current_major_test->results.skipped +=
			test->ops->total_tests;
		fw->total.skipped += test->ops->total_tests;
		if (fw->show_progress) {
			fwts_framework_minor_test_progress_clear_line();
			fprintf(stderr, "  %s: %s\n",
				msg, fwts_firmware_feature_string(missing));
		}
		goto done;
	}

	if ((test->ops->init) &&
	    ((ret = test->ops->init(fw)) != FWTS_OK)) {
		char *msg = NULL;

		/* Init failed or skipped, so abort */
		if (ret == FWTS_SKIP) {
			fw->current_major_test->results.skipped += test->ops->total_tests;
			fw->total.skipped += test->ops->total_tests;
			msg = "Test skipped";
		} else {
			fwts_log_error(fw, "Aborted test, initialisation failed.");
			fw->current_major_test->results.aborted += test->ops->total_tests;
			fw->total.aborted += test->ops->total_tests;
			msg = "Test aborted";
		}
		if (fw->show_progress) {
			fwts_framework_minor_test_progress_clear_line();
			fprintf(stderr, " %s.\n", msg);
		}
		goto done;
	}

	fwts_log_section_begin(fw->results, "subtests");
	for (minor_test = test->ops->minor_tests;
		*minor_test->test_func != NULL;
		minor_test++, fw->current_minor_test_num++) {

		fwts_log_section_begin(fw->results, "subtest");
		fw->current_minor_test_name = minor_test->name;

		fwts_results_zero(&fw->minor_tests);

		if (minor_test->name != NULL) {
			fwts_log_section_begin(fw->results, "subtest_info");
			fwts_log_info(fw, "Test %d of %d: %s",
				fw->current_minor_test_num,
				test->ops->total_tests, minor_test->name);
			fwts_log_section_end(fw->results);	/* subtest_info */
		}

		fwts_log_section_begin(fw->results, "subtest_results");
		fwts_framework_minor_test_progress(fw, 0, "");

		ret = (*minor_test->test_func)(fw);

		/* Something went horribly wrong, abort all other tests too */
		if (ret == FWTS_ABORTED)  {
			int aborted = test->ops->total_tests - (fw->current_minor_test_num - 1);
			fw->current_major_test->results.aborted += aborted;

			fwts_log_section_end(fw->results);	/* subtest_results */
			fwts_log_nl(fw);
			fwts_log_section_end(fw->results);	/* subtest */
			break;
		}
		fwts_framework_minor_test_progress(fw, 100, "");
		fwts_framework_summate_results(&fw->current_major_test->results, &fw->minor_tests);

		if (fw->show_progress) {
			char resbuf[128];
			char namebuf[55];
			fwts_framework_minor_test_progress_clear_line();
			fwts_framework_format_results(resbuf, sizeof(resbuf), &fw->minor_tests, false);
			fwts_framework_strtrunc(namebuf, minor_test->name, sizeof(namebuf));
			fprintf(stderr, "  %-55.55s %s\n", namebuf,
				*resbuf ? resbuf : "     ");
		}
		fwts_log_section_end(fw->results);	/* subtest_results */
		fwts_log_nl(fw);
		fwts_log_section_end(fw->results);	/* subtest */
	}
	fwts_log_section_end(fw->results);	/* subtests */

	fwts_framework_summate_results(&fw->total, &fw->current_major_test->results);

	if (test->ops->deinit)
		test->ops->deinit(fw);

done:
	if (!(test->flags & FWTS_FLAG_UTILS)) {
		fwts_log_section_begin(fw->results, "results");
		fwts_framework_test_summary(fw);
		fwts_log_section_end(fw->results);	/* results */
	}

	fwts_log_section_end(fw->results);		/* test->name */
	fwts_log_set_owner(fw->results, "fwts");

	return FWTS_OK;
}

/*
 *  fwts_framework_tests_run()
 *
 */
static void fwts_framework_tests_run(fwts_framework *fw, fwts_list *tests_to_run)
{
	fwts_list_link *item;

	fw->current_major_test_num = 1;
	fw->major_tests_total  = fwts_list_len(tests_to_run);

	fwts_list_foreach(item, tests_to_run) {
		fwts_framework_test *test = fwts_list_data(fwts_framework_test *, item);
		fwts_framework_run_test(fw, test);
		fw->current_major_test_num++;
	}
}

/*
 *  fwts_framework_test_find()
 *	find a named test, return test if found, NULL otherwise
 */
static fwts_framework_test *fwts_framework_test_find(const char *name)
{
	fwts_list_link *item;

	fwts_list_foreach(item, &fwts_framework_test_list) {
		fwts_framework_test *test = fwts_list_data(fwts_framework_test *, item);
		if (strcmp(name, test->name) == 0)
			return test;
	}

	return NULL;
}

bool fwts_error_filtered_out(fwts_framework *fw, const char *label)
{
	fwts_list_link *item;

	/*
	 *  Has the user specified errors to discard?  If we find any matches
	 *  then flag as wanting to filter out.
	 */
	if (fwts_list_len(&fw->errors_filter_discard) > 0) {
		fwts_list_foreach(item, &fw->errors_filter_discard) {
			if (strcmp(label, fwts_list_data(char *, item)) == 0)
				return true;	/* Discard */
		}
		return false;	/* No matches, won't discard */
	}

	/*
	 *  Has the user specified errors to keep?  If we find any matches
	 *  then flag as wanting to keep.
	 */
	if (fwts_list_len(&fw->errors_filter_keep) > 0) {
		fwts_list_foreach(item, &fw->errors_filter_keep) {
			if (strcmp(label, fwts_list_data(char *, item)) == 0)
				return false;	/* Don't discard */
		}
		return true;	/* Not found, so discard */
	}

	/*
	 *  User not specified any filters?  Don't discard
	 */
	return false;
}

/*
 *  fwts_error_inc()
 *	Increment the error count if we're not filtering out this error (based on label).
 */
void fwts_error_inc(fwts_framework *fw, const char *label, int *count)
{
	if (!fwts_error_filtered_out(fw, label))
		(*count)++;
}

/*
 *  fwts_framework_log()
 *	log a test result
 */
void fwts_framework_log(
	fwts_framework *fw,
	fwts_log_field field,
	const char *label,
	fwts_log_level level,
	uint32_t *count,
	const char *fmt, ...)
{
	char buffer[4096];
	char prefix[256];
	char *str = fwts_log_field_to_str_upper(field);
	bool do_count = !FWTS_LEVEL_IGNORE(fw, level);

	if (fmt) {
		va_list ap;

		va_start(ap, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, ap);
		va_end(ap);
	} else
		*buffer = '\0';

	switch (field) {
	case LOG_ADVICE:
		/* If the previous LOG_FAILED message was filtered out, ignore following advice */
		if (fw->error_filtered_out) {
			do_count = false;
		} else {
			fwts_log_nl(fw);
			snprintf(prefix, sizeof(prefix), "%s: ", str);
			fwts_log_printf(fw, field, level, str, label, prefix, "%s", buffer);
			fwts_log_nl(fw);
		}
		break;
	case LOG_FAILED:
		if (fwts_error_filtered_out(fw, label)) {
			fw->error_filtered_out = true;
			do_count = false;
		} else {
			fw->error_filtered_out = false;

			fw->failed_level |= level;
			fwts_summary_add(fw, fw->current_major_test->name, level, buffer);
			snprintf(prefix, sizeof(prefix), "%s [%s] %s: Test %d, ",
				str, fwts_log_level_to_str(level), label, fw->current_minor_test_num);
			fwts_log_printf(fw, field, level, str, label, prefix, "%s", buffer);
		}
		break;
	case LOG_PASSED:
	case LOG_WARNING:
	case LOG_SKIPPED:
	case LOG_ABORTED:
		snprintf(prefix, sizeof(prefix), "%s: Test %d, ",
			str, fw->current_minor_test_num);
		fwts_log_printf(fw, field, level, str, label, prefix, "%s", buffer);
		break;
	case LOG_INFOONLY:
		break;	/* no-op */
	default:
		break;
	}

	/* Only increment stats if we've reported something */
	if (do_count && (count != NULL))
		(*count)++;
}

/*
 *  fwts_framework_show_version()
 *	dump version of fwts
 */
void fwts_framework_show_version(FILE *fp, const char *name)
{
	fprintf(fp, "%s, Version %s, %s\n", name, FWTS_VERSION, FWTS_DATE);
}


/*
 *  fwts_framework_strdup()
 *	dup a string. if it's already allocated, free previous allocation before duping
 */
static void fwts_framework_strdup(char **ptr, const char *str)
{
	if (ptr == NULL)
		return;

	free(*ptr);
	*ptr = strdup(str);
}

/*
 *  fwts_framework_syntax()
 *	dump some help
 */
static void fwts_framework_syntax(char * const *argv)
{
	int i;

	printf("Usage %s: [OPTION] [TEST]\n", argv[0]);

	fwts_args_show_options();

	/* Tag on copyright info */
	printf("\n");
	for (i = 0; fwts_copyright[i]; i++)
		printf("%s\n", fwts_copyright[i]);
}

/*
 * fwts_framework_heading_info()
 *	log basic system info so we can track the tests
 */
static void fwts_framework_heading_info(
	fwts_framework *fw,
	fwts_list *tests_to_run,
	const int argc,
	char * const *argv)
{
	struct tm tm;
	time_t now;
	struct utsname buf;
	char *tests = NULL;
	char *args = NULL;
	size_t len;
	int i;
	fwts_list_link *item;

	time(&now);
	localtime_r(&now, &tm);

	uname(&buf);

	fwts_log_info(fw, "Results generated by fwts: Version %s (%s).", FWTS_VERSION, FWTS_DATE);
	fwts_log_nl(fw);
	for (i = 0; fwts_copyright[i]; i++)
		fwts_log_info(fw, "%s", fwts_copyright[i]);
	fwts_log_nl(fw);

	fwts_log_info(fw, "This test run on %2.2d/%2.2d/%-2.2d at %2.2d:%2.2d:%2.2d on host %s %s %s %s %s.",
		tm.tm_mday, tm.tm_mon + 1, (tm.tm_year+1900) % 100,
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		buf.sysname, buf.nodename, buf.release, buf.version, buf.machine);
	fwts_log_nl(fw);

	for (len = 1, i = 1; i < argc; i++)
		len += strlen(argv[i]) + 1;

	if ((args = calloc(len, sizeof(char))) != NULL) {
		for (i = 1; i < argc; i++) {
			strcat(args, " ");
			strcat(args, argv[i]);
		}
		fwts_log_info(fw, "Command: \"fwts %s\".", args);
		free(args);
	}

	len = 1;
	fwts_list_foreach(item, tests_to_run) {
		fwts_framework_test *test = fwts_list_data(fwts_framework_test *, item);
		len += strlen(test->name) + 1;
	}

	if ((tests = calloc(len, sizeof(char))) != NULL) {
		fwts_list_foreach(item, tests_to_run) {
			fwts_framework_test *test = fwts_list_data(fwts_framework_test *, item);
			if (item != fwts_list_head(tests_to_run))
				strcat(tests, " ");
			strcat(tests, test->name);
		}

		fwts_log_info(fw, "Running tests: %s.",
			fwts_list_len(tests_to_run) == 0 ? "None" : tests);
		fwts_log_newline(fw->results);
		free(tests);
	}
}

/*
 *  fwts_framework_skip_test()
 *	try to find a test in list of tests to be skipped, return NULL of cannot be found
 */
static fwts_framework_test *fwts_framework_skip_test(fwts_framework_test *test)
{
	fwts_list_link *item;

	fwts_list_foreach(item, &tests_to_skip)
		if (test == fwts_list_data(fwts_framework_test *, item))
			return test;

	return NULL;
}

/*
 *  fwts_framework_skip_test_parse()
 *	parse optarg of comma separated list of tests to skip
 */
static int fwts_framework_skip_test_parse(char *arg)
{
	char *str;
	char *token;

	for (str = arg; (token = strtok(str, ",")) != NULL; str = NULL) {
		fwts_framework_test *test;

		if ((test = fwts_framework_test_find(token)) == NULL) {
			fprintf(stderr, "No such test '%s'\n", token);
			return FWTS_ERROR;
		} else
			fwts_list_append(&tests_to_skip, test);
	}

	return FWTS_OK;
}

static int fwts_framework_filter_error_parse(char *arg, fwts_list *list)
{
	char *str;
	char *token;

	for (str = arg; (token = strtok(str, ",")) != NULL; str = NULL) {
		if (fwts_list_append(list, token) == NULL) {
			fprintf(stderr, "Out of memory parsing argument %s\n", arg);
			fwts_list_free_items(list, NULL);
			return FWTS_ERROR;
		}
	}

	return FWTS_OK;
}

/*
 *  fwts_framework_log_type_parse()
 *	parse optarg of comma separated log types
 */
static int fwts_framework_log_type_parse(fwts_framework *fw, char *arg)
{
	char *str;
	char *token;

	fw->log_type = 0;

	for (str = arg; (token = strtok(str, ",")) != NULL; str = NULL) {
		if (!strcmp(token, "plaintext"))
			fw->log_type |= LOG_TYPE_PLAINTEXT;
		else if (!strcmp(token, "json"))
			fw->log_type |= LOG_TYPE_JSON;
		else if (!strcmp(token, "xml"))
			fw->log_type |= LOG_TYPE_XML;
		else if (!strcmp(token, "html"))
			fw->log_type |= LOG_TYPE_HTML;
		else {
			fprintf(stderr, "--log-type can be plaintext, xml, html or json.\n");
			return FWTS_ERROR;
		}
	}

	if (!fw->log_type)
		fw->log_type = LOG_TYPE_PLAINTEXT;

	return FWTS_OK;
}

/*
 *  fwts_framework_acpica_parse()
 *	parse optarg of comma separated acpica mode flags
 */
static int fwts_framework_acpica_parse(fwts_framework *fw, char *arg)
{
	char *str;
	char *token;

	fw->acpica_mode = 0;

	for (str = arg; (token = strtok(str, ",")) != NULL; str = NULL) {
		if (!strcmp(token, "serialized"))
			fw->acpica_mode |= FWTS_ACPICA_MODE_SERIALIZED;
		else if (!strcmp(token, "slack"))
			fw->acpica_mode |= FWTS_ACPICA_MODE_SLACK;
		else if (!strcmp(token, "ignore-errors"))
			fw->acpica_mode |= FWTS_ACPICA_MODE_IGNORE_ERRORS;
		else if (!strcmp(token, "disable-auto-repair"))
			fw->acpica_mode |= FWTS_ACPICA_MODE_DISABLE_AUTO_REPAIR;
		else {
			fprintf(stderr, "--acpica can be serialized, slack, ignore-errors or disable-auto-repair\n");
			return FWTS_ERROR;
		}
	}

	if (!fw->log_type)
		fw->log_type = LOG_TYPE_PLAINTEXT;

	return FWTS_OK;
}

/*
 *  fwts_framework_pm_method_parse()
 *	parse optarg of pm-method mode flag
 */
static int fwts_framework_pm_method_parse(fwts_framework *fw, const char *arg)
{
#if FWTS_ENABLE_LOGIND
	if (strcmp(arg, "logind") == 0)
		fw->pm_method = FWTS_PM_LOGIND;
	else
#endif
	if (strcmp(arg, "pm-utils") == 0)
		fw->pm_method = FWTS_PM_PMUTILS;
	else if (strcmp(arg, "sysfs") == 0)
		fw->pm_method = FWTS_PM_SYSFS;
	else {
#if FWTS_ENABLE_LOGIND
		fprintf(stderr, "--pm-method only supports logind, pm-utils and sysfs methods\n");
#else
		fprintf(stderr, "--pm-method only supports pm-utils and sysfs methods\n");
#endif
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 *  fwts_framework_ll_parse()
 *	parse log level option
 */
static int fwts_framework_ll_parse(fwts_framework *fw, const char *arg)
{
	int i;

	for (i = 0; log_levels[i].name; i++) {
		if (!strcmp(arg, log_levels[i].name)) {
			fw->filter_level = log_levels[i].filter_level;
			return FWTS_OK;
		}
	}
	fprintf(stderr, "--log-level supports levels:");
	for (i = 0; log_levels[i].name; i++)
		fprintf(stderr, " %s", log_levels[i].name);
	fprintf(stderr, "\n");

	return FWTS_ERROR;
}

/*
 *  fwts_framework_an_parse()
 *	parse arch (architecture) name option
 */
static int fwts_framework_an_parse(fwts_framework *fw, const char *arg)
{
	fw->target_arch = fwts_arch_get_arch(arg);
	if (fw->target_arch == FWTS_ARCH_OTHER) {
		char *names = fwts_arch_names();

		fprintf(stderr, "--arch can be one of: %s\n", names ? names : "<unknown>");
		free(names);
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

int fwts_framework_options_handler(fwts_framework *fw, int argc, char * const argv[], int option_char, int long_index)
{
	FWTS_UNUSED(argc);

	switch (option_char) {
	case 0:
		switch (long_index) {
		case 0: /* --stdout-summary */
			fw->flags |= FWTS_FLAG_STDOUT_SUMMARY;
			break;
		case 1: /* --help */
			fwts_framework_syntax(argv);
			return FWTS_COMPLETE;
		case 2: /* --results-output */
			fwts_framework_strdup(&fw->results_logname, optarg);
			break;
		case 3: /* --results-no-separators */
			fwts_log_filter_unset_field(LOG_SEPARATOR);
			break;
		case 4: /* --log-filter */
			fwts_log_filter_unset_field(~0);
			fwts_log_set_field_filter(optarg);
			break;
		case 5: /* --log-fields */
			fwts_log_print_fields();
			return FWTS_COMPLETE;
		case 6: /* --log-format */
			fwts_log_set_format(optarg);
			break;
		case 7: /* --show-progress */
			fw->flags = (fw->flags &
					~(FWTS_FLAG_QUIET |
					  FWTS_FLAG_SHOW_PROGRESS_DIALOG))
					| FWTS_FLAG_SHOW_PROGRESS;
			break;
		case 8: /* --show-tests */
			fw->flags |= FWTS_FLAG_SHOW_TESTS;
			break;
		case 9: /* --klog */
			fwts_framework_strdup(&fw->klog, optarg);
			break;
		case 10: /* --olog */
			fwts_framework_strdup(&fw->olog, optarg);
			break;
		case 11: /* --log-width=N */
			fwts_log_set_line_width(atoi(optarg));
			break;
		case 12: /* --lspci=pathtolspci */
			fwts_framework_strdup(&fw->lspci, optarg);
			break;
		case 13: /* --batch */
			fw->flags |= FWTS_FLAG_BATCH;
			break;
		case 14: /* --interactive */
			fw->flags |= FWTS_FLAG_INTERACTIVE;
			break;
		case 15: /* --force-clean */
			fw->flags |= FWTS_FLAG_FORCE_CLEAN;
			break;
		case 16: /* --version */
			fwts_framework_show_version(stdout, argv[0]);
			return FWTS_COMPLETE;
		case 17: /* --dump */
			fw->flags |= FWTS_FLAG_DUMP;
			break;
		case 18: /* --table-path */
			fwts_framework_strdup(&fw->acpi_table_path, optarg);
			break;
		case 19: /* --batch-experimental */
			fw->flags |= FWTS_FLAG_BATCH_EXPERIMENTAL;
			break;
		case 20: /* --interactive-experimental */
			fw->flags |= FWTS_FLAG_INTERACTIVE_EXPERIMENTAL;
			break;
		case 21: /* --power-states */
			fw->flags |= FWTS_FLAG_POWER_STATES;
			break;
		case 22: /* --all */
			fw->flags |= FWTS_FLAG_RUN_ALL;
			break;
		case 23: /* --show-progress-dialog */
			fw->flags = (fw->flags &
					~(FWTS_FLAG_QUIET |
					  FWTS_FLAG_SHOW_PROGRESS))
					| FWTS_FLAG_SHOW_PROGRESS_DIALOG;
			break;
		case 24: /* --skip-test */
			if (fwts_framework_skip_test_parse(optarg) != FWTS_OK)
				return FWTS_COMPLETE;
			break;
		case 25: /* --quiet */
			fw->flags = (fw->flags &
					~(FWTS_FLAG_SHOW_PROGRESS |
					  FWTS_FLAG_SHOW_PROGRESS_DIALOG))
					| FWTS_FLAG_QUIET;
			break;
		case 26: /* --dumpfile */
			fwts_framework_strdup(&fw->acpi_table_acpidump_file, optarg);
			break;
		case 27: /* --show-tests-full */
			fw->flags |= FWTS_FLAG_SHOW_TESTS_FULL;
			break;
		case 28: /* --utils */
			fw->flags |= FWTS_FLAG_UTILS;
			break;
		case 29: /* --json-data-path */
			fwts_framework_strdup(&fw->json_data_path, optarg);
			break;
		case 30: /* --json-data-file */
			fwts_framework_strdup(&fw->json_data_file, optarg);
			break;
		case 31: /* --disassemble-aml */
#if defined(FWTS_HAS_ACPI)
			fwts_iasl_disassemble_all_to_file(fw, optarg);
			return FWTS_COMPLETE;
#else
			fprintf(stderr, "option not available on this architecture\n");
			return FWTS_ERROR;
#endif
		case 32: /* --log-type */
			if (fwts_framework_log_type_parse(fw, optarg) != FWTS_OK)
				return FWTS_ERROR;
			break;
		case 33: /* --unsafe */
			fw->flags |= FWTS_FLAG_UNSAFE;
			break;
		case 34: /* --filter-error-discard */
			if (fwts_framework_filter_error_parse(optarg, &fw->errors_filter_discard) != FWTS_OK)
				return FWTS_ERROR;
			break;
		case 35: /* --filter-error-keep */
			if (fwts_framework_filter_error_parse(optarg, &fw->errors_filter_keep) != FWTS_OK)
				return FWTS_ERROR;
			break;
		case 36: /* --acpica-debug */
			fw->flags |= FWTS_FLAG_ACPICA_DEBUG;
			break;
		case 37: /* --acpica */
			if (fwts_framework_acpica_parse(fw, optarg) != FWTS_OK)
				return FWTS_ERROR;
			break;
		case 38: /* --uefitests */
			fw->flags |= FWTS_FLAG_UEFI;
			break;
		case 39: /* --rsdp */
			fw->rsdp = (void *)strtoul(optarg, NULL, 0);
			break;
		case 40: /* --pm-method */
			if (fwts_framework_pm_method_parse(fw, optarg) != FWTS_OK)
				return FWTS_ERROR;
			break;
		case 41: /* --show-tests-categories */
			fw->flags |= FWTS_FLAG_SHOW_TESTS_CATEGORIES;
			break;
		case 42: /* --acpitests */
			fw->flags |= FWTS_FLAG_ACPI;
			break;
		case 43: /* --acpicompliance */
			fw->flags |= FWTS_FLAG_COMPLIANCE_ACPI;
			break;
		case 44: /* --log-level */
			if (fwts_framework_ll_parse(fw, optarg) != FWTS_OK)
				return FWTS_ERROR;
			break;
		case 45: /* --arch */
			if (fwts_framework_an_parse(fw, optarg) != FWTS_OK)
				return FWTS_ERROR;
			break;
		case 46: /* --sbbr */
#if defined(FWTS_ARCH_AARCH64)
			fw->flags |= FWTS_FLAG_SBBR;
			break;
#else
			fprintf(stderr, "option not available on this architecture\n");
			return FWTS_ERROR;
#endif
		case 47: /* --ifv */
			fw->flags |= FWTS_FLAG_FIRMWARE_VENDOR;
			break;
		case 48: /* --coreboot-log */
			fwts_framework_strdup(&fw->clog, optarg);
			break;
		case 49: /* --ebbr */
#if defined(FWTS_ARCH_AARCH64) || defined(FWTS_ARCH_RISCV)
			fw->flags |= FWTS_FLAG_EBBR;
			break;
#else
			fprintf(stderr, "option not available on this architecture\n");
			return FWTS_ERROR;
#endif
		}
		break;
	case 'a': /* --all */
		fw->flags |= FWTS_FLAG_RUN_ALL;
		break;
	case 'b': /* --batch */
		fw->flags |= FWTS_FLAG_BATCH;
		break;
	case 'd': /* --dump */
		fw->flags |= FWTS_FLAG_DUMP;
		break;
	case 'D': /* --show-progress-dialog */
		fw->flags = (fw->flags &
				~(FWTS_FLAG_QUIET |
				  FWTS_FLAG_SHOW_PROGRESS))
				| FWTS_FLAG_SHOW_PROGRESS_DIALOG;
		break;
	case 'f':
		fw->flags |= FWTS_FLAG_FORCE_CLEAN;
		break;
	case 'h':
		fwts_framework_syntax(argv);
		return FWTS_COMPLETE;
	case '?':
		fprintf(stderr, "for help, use fwts -h\n");
		return FWTS_COMPLETE;
	case 'i': /* --interactive */
		fw->flags |= FWTS_FLAG_INTERACTIVE;
		break;
	case 'j': /* --json-data-path */
		fwts_framework_strdup(&fw->json_data_path, optarg);
		break;
	case 'J': /* --json-data-file */
		fwts_framework_strdup(&fw->json_data_file, optarg);
		break;
	case 'k': /* --klog */
		fwts_framework_strdup(&fw->klog, optarg);
		break;
	case 'l': /* --lp-flags */
		break;
	case 'o': /* --olog */
		fwts_framework_strdup(&fw->olog, optarg);
		break;
	case 'p': /* --show-progress */
		fw->flags = (fw->flags &
				~(FWTS_FLAG_QUIET |
				  FWTS_FLAG_SHOW_PROGRESS_DIALOG))
				| FWTS_FLAG_SHOW_PROGRESS;
			break;
	case 'P': /* --power-states */
		fw->flags |= FWTS_FLAG_POWER_STATES;
		break;
	case 'q': /* --quiet */
		fw->flags = (fw->flags &
				~(FWTS_FLAG_SHOW_PROGRESS |
				  FWTS_FLAG_SHOW_PROGRESS_DIALOG))
				| FWTS_FLAG_QUIET;
		break;
	case 'r': /* --results-output */
		fwts_framework_strdup(&fw->results_logname, optarg);
		break;
	case 's': /* --show-tests */
		fw->flags |= FWTS_FLAG_SHOW_TESTS;
		break;
	case 'S': /* --skip-test */
		if (fwts_framework_skip_test_parse(optarg) != FWTS_OK)
			return FWTS_COMPLETE;
		break;
	case 't': /* --table-path */
		fwts_framework_strdup(&fw->acpi_table_path, optarg);
		break;
	case 'u': /* --utils */
		fw->flags |= FWTS_FLAG_UTILS;
		break;
	case 'U': /* --unsafe */
		fw->flags |= FWTS_FLAG_UNSAFE;
		break;
	case 'v': /* --version */
		fwts_framework_show_version(stdout, argv[0]);
		return FWTS_COMPLETE;
	case 'w': /* --log-width=N */
		fwts_log_set_line_width(atoi(optarg));
		break;
	case 'R': /* --rsdp=addr */
		fw->rsdp = (void *)strtoul(optarg, NULL, 0);
		break;
	}
	return FWTS_OK;
}

/*
 *  fwts_framework_args()
 *	parse args and run tests
 */
int fwts_framework_args(const int argc, char **argv)
{
	int ret = FWTS_OK;
	int i;

	fwts_list tests_to_run;
	fwts_framework *fw;
	fwts_list_link *item;

	if ((fw = (fwts_framework *)calloc(1, sizeof(fwts_framework))) == NULL)
		return FWTS_ERROR;

	/* Set the power method to FWTS_PM_UNDEFINED before we parse arguments */
	fw->pm_method = FWTS_PM_UNDEFINED;

	/* Set host/target test architecture defaults */
	fw->host_arch = fwts_arch_get_host();
	fw->target_arch = fw->host_arch;

	ret = fwts_args_add_options(fwts_framework_options,
		fwts_framework_options_handler, NULL);
	if (ret == FWTS_ERROR)
		goto tidy_args;

	fw->firmware_type = fwts_firmware_detect();

	fw->magic = FWTS_FRAMEWORK_MAGIC;
	fw->flags = FWTS_FLAG_DEFAULT |
		    FWTS_FLAG_SHOW_PROGRESS;
	fw->log_type = LOG_TYPE_PLAINTEXT;
	fw->filter_level = LOG_LEVEL_ALL;

	fwts_list_init(&fw->errors_filter_keep);
	fwts_list_init(&fw->errors_filter_discard);

	fwts_summary_init();

	fwts_framework_strdup(&fw->lspci, FWTS_LSPCI_PATH);
	fwts_framework_strdup(&fw->results_logname, RESULTS_LOG);
	fwts_framework_strdup(&fw->json_data_path, FWTS_JSON_DATA_PATH);

	fwts_list_init(&tests_to_run);
	fwts_list_init(&tests_to_skip);

	switch (fwts_args_parse(fw, argc, argv)) {
	case FWTS_OK:
		break;
	case FWTS_COMPLETE:		/* All done, e.g. --help, --version */
		goto tidy_close;
	default:
		ret = FWTS_ERROR;	/* Parsing error, or out of memory etc */
		goto tidy_close;
	}

	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-")) {
			fwts_framework_strdup(&fw->results_logname, "stdout");
			fw->flags = (fw->flags &
					~(FWTS_FLAG_SHOW_PROGRESS |
					  FWTS_FLAG_SHOW_PROGRESS_DIALOG))
					| FWTS_FLAG_QUIET;
			break;
		}

	if (fw->flags & FWTS_FLAG_SHOW_TESTS) {
		fwts_framework_show_tests(fw, false);
		goto tidy_close;
	}
	if (fw->flags & FWTS_FLAG_SHOW_TESTS_FULL) {
		fwts_framework_show_tests(fw, true);
		goto tidy_close;
	}
	if (fw->flags & FWTS_FLAG_SHOW_TESTS_CATEGORIES) {
		fwts_framework_show_tests_categories();
		goto tidy_close;
	}
	if (fw->flags & FWTS_FLAG_DUMP) {
		fwts_dump_info(fw);
		goto tidy_close;
	}
	if ((fw->lspci == NULL) || (fw->results_logname == NULL)) {
		ret = FWTS_ERROR;
		fprintf(stderr, "%s: Memory allocation failure.", argv[0]);
		goto tidy_close;
	}

	/* These are mutually exclusive, both cannot have items */
	if ((fwts_list_len(&fw->errors_filter_keep) > 0) &&
	    (fwts_list_len(&fw->errors_filter_discard) > 0)) {
		fprintf(stderr,
			"The --filter-error-discard and --filter-error-keep options are\n"
			"mutually exclusive.  Specify one or the other, but not both.\n");
		ret = FWTS_ERROR;
		goto tidy_close;
	}

	/* Ensure we have just one log type specified for non-filename logging */
	if (fwts_log_type_count(fw->log_type) > 1 &&
	    fwts_log_get_filename_type(fw->results_logname) != LOG_FILENAME_TYPE_FILE) {
		fprintf(stderr,
			"Cannot specify more than one log type when "
			"logging to stderr or stdout\n");
		ret = FWTS_ERROR;
		goto tidy_close;
	}

	/* Results log */
	if ((fw->results = fwts_log_open("fwts",
			fw->results_logname,
			(fw->flags & FWTS_FLAG_FORCE_CLEAN) ? "w" : "a",
			fw->log_type)) == NULL) {
		char *filenames = fwts_log_get_filenames(fw->results_logname, fw->log_type);

		ret = FWTS_ERROR;
		fprintf(stderr, "%s: Cannot open results log '%s'"
			" (you may need to remove it to set proper"
			" permissions).\n",
			argv[0],
			filenames ? filenames : fw->results_logname);
		if (filenames)
			free(filenames);
		goto tidy_close;
	}

	/* Init firmware data required by tests */
	fwts_devicetree_read(fw);

	/* Collect up tests to run */
	for (i = optind; i < argc; i++) {
		fwts_framework_test *test;

		if (*argv[i] == '-')
			continue;

		if ((test = fwts_framework_test_find(argv[i])) == NULL) {
			fprintf(stderr, "No such test '%s', available tests:\n",argv[i]);
			fwts_framework_show_tests_brief();
			ret = FWTS_ERROR;
			goto tidy;
		}

		if (fwts_framework_skip_test(test) == NULL)
			fwts_list_append(&tests_to_run, test);
	}

	/* No options given and no tests, so default to run batch tests */
	if (!(FWTS_FLAG_RUN_ALL & fw->flags) &&
	    (fwts_list_len(&tests_to_run) == 0))
		fw->flags |= FWTS_FLAG_BATCH;

	/* Find tests that are eligible for running */
	fwts_list_foreach(item, &fwts_framework_test_list) {
		fwts_framework_test *test = fwts_list_data(fwts_framework_test*, item);
		if (fw->flags & test->flags & FWTS_FLAG_RUN_ALL)
			if (fwts_framework_skip_test(test) == NULL)
				fwts_list_append(&tests_to_run, test);
	}

	if (!(fw->flags & FWTS_FLAG_QUIET)) {
		char *filenames = fwts_log_get_filenames(fw->results_logname, fw->log_type);

		if (filenames) {
			printf("Running %d tests, results appended to %s\n",
				fwts_list_len(&tests_to_run),
				filenames);
			free(filenames);
		}
	}

	fwts_log_section_begin(fw->results, "heading");
	fwts_framework_heading_info(fw, &tests_to_run, argc, argv);
	fwts_log_section_end(fw->results);

	fwts_log_section_begin(fw->results, "tests");
	fwts_framework_tests_run(fw, &tests_to_run);
	fwts_log_section_end(fw->results);

	if (fw->print_summary) {
		fwts_log_section_begin(fw->results, "summary");
		fwts_log_set_owner(fw->results, "summary");
		fwts_log_nl(fw);
		fwts_framework_total_summary(fw);
		fwts_log_nl(fw);
		fwts_summary_report(fw, &fwts_framework_test_list);
		fwts_log_section_end(fw->results);
	}

tidy:
	fwts_list_free_items(&tests_to_skip, NULL);
	fwts_list_free_items(&tests_to_run, NULL);
	fwts_log_close(fw->results);

tidy_close:
#if defined(FWTS_HAS_ACPI)
	fwts_acpi_free_tables();
#endif
	fwts_summary_deinit();

	free(fw->lspci);
	free(fw->results_logname);
	free(fw->clog);
	free(fw->klog);
	free(fw->olog);
	free(fw->json_data_path);
	free(fw->json_data_file);
	free(fw->fdt);

	fwts_list_free_items(&fw->errors_filter_discard, NULL);
	fwts_list_free_items(&fw->errors_filter_keep, NULL);
	fwts_list_free_items(&fwts_framework_test_list, free);

	/* Failed tests flagged an error */
	if ((fw->total.failed > 0) || (fw->total.warning > 0))
		ret = FWTS_ERROR;

tidy_args:
	fwts_args_free();
	free(fw);

	return ret;
}
