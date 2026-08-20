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
#include <string.h>

#include "fwts.h"

typedef struct {
	char *test;	/* test that found the error */
	char *text;	/* text of failure message */
} fwts_summary_item;

enum {
	SUMMARY_CRITICAL = 0,
	SUMMARY_HIGH,
	SUMMARY_MEDIUM,
	SUMMARY_LOW,
	SUMMARY_UNKNOWN,

	SUMMARY_MAX = SUMMARY_UNKNOWN+1
};

/* list of summary items per error level */
static fwts_list *fwts_summaries[SUMMARY_MAX];

/*
 *  fwts_summary_init()
 *	initialise
 */
int fwts_summary_init(void)
{
	int i;

	/* initialise list of summary items for all error levels */
	for (i = 0; i < SUMMARY_MAX; i++)
		if ((fwts_summaries[i] = fwts_list_new()) == NULL) {
			fwts_summary_deinit();
			return FWTS_ERROR;
		}

	return FWTS_OK;
}

/*
 *  fwts_summary_item_free()
 *	free a summary item
 */
static void fwts_summary_item_free(void *data)
{
	fwts_summary_item *item = (fwts_summary_item *)data;

	free(item->test);
	free(item->text);
	free(item);
}

/*
 *  fwts_summary_init()
 *	free up summary lists
 */
void fwts_summary_deinit(void)
{
	int i;

	for (i = 0; i < SUMMARY_MAX; i++)
		if (fwts_summaries[i])
			fwts_list_free(fwts_summaries[i], fwts_summary_item_free);
}

static int fwts_summary_level_to_index(const fwts_log_level level)
{
	switch (level) {
	case LOG_LEVEL_CRITICAL:
		return SUMMARY_CRITICAL;
	case LOG_LEVEL_HIGH:
		return SUMMARY_HIGH;
	case LOG_LEVEL_MEDIUM:
		return SUMMARY_MEDIUM;
	case LOG_LEVEL_LOW:
		return SUMMARY_LOW;
	default:
		return SUMMARY_UNKNOWN;
	}
}

/*
 *  fwts_summary_add()
 *	add an error summary for a test with error message text at given
 *	error level to the list of summaries.
 */
int fwts_summary_add(
	fwts_framework *fw,
	const char *test,
	const fwts_log_level level,
	const char *text)
{
	fwts_list_link	*item;
	fwts_summary_item *summary_item = NULL;
	bool summary_item_found = false;
	int index = fwts_summary_level_to_index(level);

	if (FWTS_LEVEL_IGNORE(fw, level))
		return FWTS_OK;

	/* Does the text already exist? - search for it */
	fwts_list_foreach(item, fwts_summaries[index]) {
		summary_item = fwts_list_data(fwts_summary_item *,item);
		if (strcmp(text, summary_item->text) == 0) {
			summary_item_found = true;
			break;
		}
	}

	/* Not found, create a new one */
	if (!summary_item_found) {
		if ((summary_item = calloc(1, sizeof(fwts_summary_item))) == NULL)
			return FWTS_ERROR;

		if ((summary_item->test = strdup(test)) == NULL) {
			free(summary_item);
			return FWTS_ERROR;
		}

		if ((summary_item->text = strdup(text)) == NULL) {
			free(summary_item->test);
			free(summary_item);
			return FWTS_ERROR;
		}
		fwts_chop_newline(summary_item->text);

		/* And append new item if not done so already */
		fwts_list_append(fwts_summaries[index], summary_item);
	}

	return FWTS_OK;
}

static void fwts_summary_format_field(
	char *buffer,
	const int buflen,
	const uint32_t value)
{
	if (value)
		snprintf(buffer, buflen, "%5u", value);
	else
		*buffer = '\0';
}

/*
 *  fwts_summary_report()
 *  	report test failure summary, sorted by error levels
 */
int fwts_summary_report(fwts_framework *fw, fwts_list *test_list)
{
	static const char *summary_names[] = {
		"Critical",
		"High",
		"Medium",
		"Low",
		"Other"
	};

	static const int summary_levels[] = {
		LOG_LEVEL_CRITICAL,
		LOG_LEVEL_HIGH,
		LOG_LEVEL_MEDIUM,
		LOG_LEVEL_LOW,
		LOG_LEVEL_NONE
	};

	int i;

	fwts_log_summary(fw, "Test Failure Summary");
	fwts_log_underline(fw->results, '=');
	fwts_log_nl(fw);

	for (i = 0; i < SUMMARY_MAX; i++) {
		if (FWTS_LEVEL_IGNORE(fw, summary_levels[i]))
			continue;

		fwts_log_section_begin(fw->results, "failure");

		if (fwts_summaries[i]->len) {
			fwts_list_link *item;
			fwts_log_summary(fw, "%s failures: %d", summary_names[i], fwts_summaries[i]->len);

			fwts_log_section_begin(fw->results, "failures");
			fwts_list_foreach(item, fwts_summaries[i]) {
				fwts_summary_item *summary_item = fwts_list_data(fwts_summary_item *,item);

				/*
				 *  This is not pleasant, we really don't want very wide lines
				 *  logged in the HTML format, where we don't mind for other formats.
				 */
				if (fw->log_type == LOG_TYPE_HTML)
					fwts_log_summary(fw, " %s test: %s",
						summary_item->test,
						summary_item->text);
				else
					fwts_log_summary_verbatim(fw, " %s: %s",
						summary_item->test,
						summary_item->text);
			}
			fwts_log_section_end(fw->results);
		}
		else
			fwts_log_summary(fw, "%s failures: NONE", summary_names[i]);

		fwts_log_section_end(fw->results);
		fwts_log_nl(fw);
	}

	if ((fw->log_type & (LOG_TYPE_PLAINTEXT| LOG_TYPE_HTML)) &&
	     fw->total_run > 0) {
		fwts_list_link *item;
		fwts_list *sorted = fwts_list_new();

		if (!sorted) {
			fwts_log_error(fw, "Out of memory allocating test summary list");
			return FWTS_ERROR;
		}

		fwts_list_foreach(item, test_list)
			fwts_list_add_ordered(sorted, fwts_list_data(fwts_framework_test *,item), fwts_framework_compare_test_name);

		fwts_log_summary_verbatim(fw, "Test           |Pass |Fail |Abort|Warn |Skip |Info |");
		fwts_log_summary_verbatim(fw, "---------------+-----+-----+-----+-----+-----+-----+");
		fwts_list_foreach(item, sorted) {
			fwts_framework_test *test = fwts_list_data(fwts_framework_test*,item);
			if (test->was_run) {
				char passed[16];
				char failed[16];
				char aborted[16];
				char warning[16];
				char skipped[16];
				char infoonly[16];

				fwts_summary_format_field(passed, sizeof(passed), test->results.passed);
				fwts_summary_format_field(failed, sizeof(failed), test->results.failed);
				fwts_summary_format_field(aborted, sizeof(aborted), test->results.aborted);
				fwts_summary_format_field(warning, sizeof(warning), test->results.warning);
				fwts_summary_format_field(skipped, sizeof(skipped), test->results.skipped);
				fwts_summary_format_field(infoonly, sizeof(infoonly), test->results.infoonly);

				fwts_log_summary_verbatim(fw, "%-15.15s|%5.5s|%5.5s|%5.5s|%5.5s|%5.5s|%5.5s|",
					test->name, passed, failed, aborted, warning, skipped, infoonly);
			}
		}
		fwts_log_summary_verbatim(fw, "---------------+-----+-----+-----+-----+-----+-----+");
		fwts_log_summary_verbatim(fw, "Total:         |%5d|%5d|%5d|%5d|%5d|%5d|",
			fw->total.passed, fw->total.failed, fw->total.aborted,
			fw->total.warning, fw->total.skipped, fw->total.infoonly);
		fwts_log_summary_verbatim(fw, "---------------+-----+-----+-----+-----+-----+-----+");
		fwts_list_free(sorted, NULL);
	}
	return FWTS_OK;
}
