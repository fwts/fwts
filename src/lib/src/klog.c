/*
 * Copyright (C) 2010 Canonical
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

#include <sys/klog.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "klog.h"
#include "text_list.h"

void klog_free(text_list *klog)
{
	text_list_free(klog);
}

int klog_clear(void)
{
	if (klogctl(5, NULL, 0) < 0)
		return 1;
	return 0;
}

text_list *klog_read(void)
{
	int len;
	char *buffer;
	text_list *list;

	if ((len = klogctl(10, NULL, 0)) < 0)
		return NULL;

	if ((buffer = malloc(len)) < 0)
		return NULL;
	
	if (klogctl(3, buffer, len) < 0)
		return NULL;

	list = text_list_from_text(buffer);
	free(buffer);
	
	return list;
}

typedef void (*scan_callback_t)(log *log, char *line, char *prevline, void *private, int *warnings, int *errors);

#if 0
typedef void (*klog_iterator)(log *log, const char *text, void *private);

void klog_iterate(log *log, text_list *list, klog_iterator iterator, void *private)
{
	while (list) {
		iterator(log, list->text, private);
		list = list->next;
	}
}
#endif

int klog_scan(log *log, text_list *klog, scan_callback_t callback, void *private, int *warnings, int *errors)
{
	*warnings = 0;
	*errors = 0;
	char *prev;
	text_list_element *item;

	if (!klog)
		return 1;


	prev = "";

	for (item = klog->head; item != NULL; item=item->next) {
		char *ptr = item->text;

		if ((ptr[0] == '<') && (ptr[2] == '>'))
			ptr += 3;

		callback(log, ptr, prev, private, warnings, errors);
		prev = ptr;
	}
	return 0;
}

/* List of errors and warnings */
static klog_pattern firmware_error_warning_patterns[] = {
	{ "ACPI Warning ",	KERN_WARNING },
	{ "[Firmware Bug]:",	KERN_ERROR },
	{ "PCI: BIOS Bug:",	KERN_ERROR },
	{ "ACPI Error ",	KERN_ERROR },
	{ NULL,			0 }
};

static klog_pattern pm_error_warning_patterns[] = {
        { "PM: Failed to prepare device", 		KERN_ERROR },
        { "PM: Some devices failed to power down", 	KERN_ERROR },
        { "PM: Some system devices failed to power down", KERN_ERROR },
        { "PM: Error", 					KERN_ERROR },
        { "PM: Some devices failed to power down", 	KERN_ERROR },
        { "PM: Restore failed, recovering", 		KERN_ERROR },
        { "PM: Resume from disk failed", 		KERN_ERROR },
        { "PM: Not enough free memory", 		KERN_ERROR },
        { "PM: Memory allocation failed", 		KERN_ERROR },
        { "PM: Image mismatch", 			KERN_ERROR },
        { "PM: Some devices failed to power down", 	KERN_ERROR },
        { "PM: Some devices failed to suspend", 	KERN_ERROR },
        { "PM: can't read", 				KERN_ERROR },
        { "PM: can't set", 				KERN_ERROR },
        { "PM: suspend test failed, error", 		KERN_ERROR },
        { "PM: can't test ", 				KERN_WARNING },
        { "PM: no wakealarm-capable RTC driver is ready", KERN_WARNING },
        { "PM: Adding page to bio failed at", 		KERN_ERROR },
        { "PM: Swap header not found", 			KERN_ERROR },
        { "PM: Cannot find swap device", 		KERN_ERROR },
        { "PM: Not enough free swap", 			KERN_ERROR },
        { "PM: Image device not initialised", 		KERN_ERROR },
        { "PM: Please power down manually", 		KERN_ERROR },
        { NULL,                   0, }
};

void klog_scan_patterns(log *log, char *line, char *prevline, void *private, int *warnings, int *errors)
{
	int i;

	klog_pattern *patterns = (klog_pattern *)private;

	for (i=0;patterns[i].pattern != NULL;i++) {
		if (strstr(line, patterns[i].pattern)) {
			log_info(log, "%s\n", line);
			if (patterns[i].type & KERN_WARNING)
				(*warnings)++;
			if (patterns[i].type & KERN_ERROR)
				(*errors)++;
		}
	}
}

int klog_firmware_check(log *log, text_list *klog, int *warnings, int *errors)
{	
	return klog_scan(log, klog, klog_scan_patterns, firmware_error_warning_patterns, warnings, errors);
}

int klog_pm_check(log *log, text_list *klog, int *warnings, int *errors)
{
	return klog_scan(log, klog, klog_scan_patterns, pm_error_warning_patterns, warnings, errors);
}
