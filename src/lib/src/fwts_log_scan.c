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

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <fcntl.h>

#include "fwts.h"

/*
 *  fwts_log_free()
 *  free log list
 */
void fwts_log_free(fwts_list *log)
{
	fwts_text_list_free(log);
}

/*
 *  fwts_log_find_changes()
 *      find new lines added to log, clone them from new list
 *      must be freed with fwts_list_free(log_diff, NULL);
 */
fwts_list *fwts_log_find_changes(fwts_list *log_old, fwts_list *log_new)
{
        fwts_list_link *l_old, *l_new = NULL;
        fwts_list *log_diff;

        if (log_new == NULL) {
                /* Nothing new to compare, return nothing */
                return NULL;
        }
        if ((log_diff = fwts_list_new()) == NULL)
                return NULL;

        if (log_old == NULL) {
                /* Nothing in old log, so clone all of new list */
                l_new = log_new->head;
        } else {
                fwts_list_link *l_old_last = NULL;

                /* Clone just the new differences */

                /* Find last item in old log */
                fwts_list_foreach(l_old, log_old)
                        l_old_last = l_old;

                if (l_old_last) {
                        /* And now look for that last line in the new log */
                        char *old = fwts_list_data(char *, l_old_last);
                        fwts_list_foreach(l_new, log_new) {
                                const char *new = fwts_list_data(char *, l_new);

                                if (!strcmp(new, old)) {
                                        /* Found last line that matches, bump to next */
                                        l_new = l_new->next;
                                        break;
                                }
                        }
                }
        }

        /* Clone the new unique lines to the log_diff list */
        for (; l_new; l_new = l_new->next) {
                if (fwts_list_append(log_diff, l_new->data) == NULL) {
                        fwts_list_free(log_diff, NULL);
                        return NULL;
                }
        }
        return log_diff;
}

char *fwts_log_remove_timestamp(char *text)
{
        char *ptr = text;

        if ((ptr[0] == '<') && (ptr[2] == '>'))
                ptr += 3;

        if (*ptr == '[')
                while (*ptr && *ptr != ']')
                        ptr++;
        if (*ptr == ']')
                ptr += 2;       /* Skip ] and trailing space */

        return ptr;
}

int fwts_log_scan(fwts_framework *fw,
        fwts_list *log,
        fwts_log_scan_func scan_func,
        fwts_log_progress_func progress_func,
        void *private,
        int *match,
        bool remove_timestamp)
{
        typedef struct {
                char *line;
                int repeated;
        } log_reduced_item;

        char *prev;
        fwts_list_link *item;
        fwts_list *log_reduced;
        int i;
        char *newline = NULL;

        *match = 0;

        if (!log)
                return FWTS_ERROR;

        if ((log_reduced = fwts_list_new()) == NULL)
                return FWTS_ERROR;

        /*
         *  Form a reduced log by stripping out repeated warnings
         */
        i = 0;
        fwts_list_foreach(item, log) {
                if (remove_timestamp) {
                        newline = fwts_log_remove_timestamp(fwts_list_data(char *, item));
                } else {
                        newline = fwts_list_data(char *, item);
                }

                if (progress_func  && ((i % 25) == 0))
                        progress_func(fw, 50 * i / fwts_list_len(log));
                if (*newline) {
                        bool matched = false;
                        fwts_list_link *l;
                        fwts_list_foreach(l, log_reduced) {
                                char *line;
                                log_reduced_item *reduced = fwts_list_data(log_reduced_item *, l);

                                if (remove_timestamp)
                                        line = fwts_log_remove_timestamp(reduced->line);
                                else
                                        line = reduced->line;

                                if (strcmp(newline, line) == 0) {
                                        reduced->repeated++;
                                        matched = true;
                                        break;
                                }
                        }
                        if (!matched) {
                                log_reduced_item *new;

                                if ((new = calloc(1, sizeof(log_reduced_item))) == NULL) {
                                        fwts_list_free(log_reduced, free);
                                        return FWTS_ERROR;
                                }
                                new->line = fwts_list_data(char *, item);
                                new->repeated = 0;

                                fwts_list_append(log_reduced, new);
                        }
                }
                i++;
        }

        prev = "";

        i = 0;
        fwts_list_foreach(item, log_reduced) {
                log_reduced_item *reduced = fwts_list_data(log_reduced_item *, item);
                char *line = reduced->line;

                if ((line[0] == '<') && (line[2] == '>'))
                        line += 3;

                scan_func(fw, line, reduced->repeated, prev, private, match);
                if (progress_func  && ((i % 25) == 0))
                        progress_func(fw, (50+(50 * i)) / fwts_list_len(log_reduced));
                prev = line;
                i++;
        }
        if (progress_func)
                progress_func(fw, 100);

        fwts_list_free(log_reduced, free);

        return FWTS_OK;
}

char *fwts_log_unique_label(const char *str, const char *label)
{
        static char buffer[1024];
        const char *src = str;
        char *dst;
        int count = 0;
        bool forceupper = true;

        strncpy(buffer, label, sizeof(buffer)-1);
        buffer[sizeof(buffer)-1] = '\0';
        dst = buffer + strlen(label);

        while ((dst < (buffer + sizeof(buffer) - 1)) &&
               (count < 4) && (*src)) {
                if ((*src == '|') ||
                    (*src == '/') ||
                    (*src == ' ')) {
                        src++;
                        count++;
                        forceupper = true;
                        continue;
                }
                if (!isalnum(*src)) {
                        src++;
                        continue;
                }
                *dst++ = forceupper ? toupper(*src) : *src;
                src++;

                forceupper = false;
        }
        *dst = '\0';
        return buffer;
}

void fwts_log_scan_patterns(fwts_framework *fw,
        char *line,
        int  repeated,
        char *prevline,
        void *private,
        int *errors,
        const char *name,
        const char *advice)
{
        fwts_log_pattern *pattern = (fwts_log_pattern *)private;

        FWTS_UNUSED(prevline);

        while (pattern->pattern != NULL) {
                bool matched = false;
                switch (pattern->compare_mode) {
                case FWTS_COMPARE_REGEX:
                        if (pattern->compiled_ok) {
                                int ret = regexec(&pattern->compiled, line, 0, NULL, 0);
                                if (!ret) {
                                        /* A successful regular expression match! */
                                        matched = true;
                                } else if (ret != REG_NOMATCH) {
                                        char msg[1024];

                                        regerror(ret, &pattern->compiled, msg, sizeof(msg));
                                        fwts_log_info(fw, "regular expression engine error: %s.", msg);
                                }
                        }
                        break;
                case FWTS_COMPARE_STRING:
                default:
                        matched = (strstr(line, pattern->pattern) != NULL) ;
                        break;
                }

                if (matched) {
                        if (pattern->level == LOG_LEVEL_INFO)
                                fwts_log_info(fw, "%s message: %s", name, line);
                        else {
                                fwts_failed(fw, pattern->level, pattern->label,
                                        "%s %s message: %s", fwts_log_level_to_str(pattern->level), name, line);
                                fwts_error_inc(fw, pattern->label, errors);
                        }
                        if (repeated)
                                fwts_log_info(fw, "Message repeated %d times.", repeated);

                        if ((pattern->advice) != NULL && (*pattern->advice))
                                fwts_advice(fw, "%s", pattern->advice);
                        else
                                fwts_advice(fw, "%s", advice);
                        return;
                }
                pattern++;
        }
}

/*
 *  fwts_log_compare_mode_str_to_val()
 *      convert compare mode strings (from json database) to compare_mode values
 */
fwts_compare_mode fwts_log_compare_mode_str_to_val(const char *str)
{
        if (strcmp(str, "regex") == 0)
                return FWTS_COMPARE_REGEX;
        else if (strcmp(str, "string") == 0)
                return FWTS_COMPARE_STRING;
        else
                return FWTS_COMPARE_UNKNOWN;
}

/*
 *  fwts_json_str()
 *	given a key, fetch the string value associated with this object
 *	and report an error if it cannot be found.
 */
const char *fwts_json_str(
	fwts_framework *fw,
	const char *table,
	int index,
	json_object *obj,
	const char *key,
	bool log_error)
{
	const char *str;
#if JSON_HAS_GET_EX
	json_object *str_obj;

	if (!json_object_object_get_ex(obj, key, &str_obj))
		goto nullobj;
	str = json_object_get_string(str_obj);
	if (FWTS_JSON_ERROR(str))
		goto nullobj;
#else
	str = json_object_get_string(json_object_object_get(obj, key));
	if (FWTS_JSON_ERROR(str))
		goto nullobj;
#endif
	return str;
nullobj:
	if (log_error)
		fwts_log_error(fw, "Cannot fetch %s val from item %d, table %s.",
			key, index, table);
	return NULL;
}

int fwts_log_check(fwts_framework *fw,
        const char *table,
        fwts_log_scan_func fwts_log_scan_patterns_func,
        fwts_log_progress_func progress,
        fwts_list *log,
        int *errors,
        const char *json_data_path,
        const char *label,
        bool remove_timestamp)
{
        int ret = FWTS_ERROR;
        int n;
        int i;
        int fd;
        json_object *log_objs;
        json_object *log_table;
        fwts_log_pattern *patterns;

	*errors = 0;

        /*
         * json_object_from_file() can fail when files aren't readable
         * so check if we can open for read before calling json_object_from_file()
         */
        if ((fd = open(json_data_path, O_RDONLY)) < 0) {
                fwts_log_error(fw, "Cannot read file %s, check the path and check that the file exists, you may need to specify -j or -J.", json_data_path);
                return FWTS_ERROR;
        }
        (void)close(fd);

        log_objs = json_object_from_file(json_data_path);
        if (FWTS_JSON_ERROR(log_objs)) {
                fwts_log_error(fw, "Cannot load log data from %s.", json_data_path);
                return FWTS_ERROR;
        }

#if JSON_HAS_GET_EX
        if (!json_object_object_get_ex(log_objs, table, &log_table)) {
                fwts_log_error(fw, "Cannot fetch log table object '%s' from %s.", table, json_data_path);
                goto fail_put;
        }
#else
        log_table = json_object_object_get(log_objs, table);
        if (FWTS_JSON_ERROR(log_table)) {
                fwts_log_error(fw, "Cannot fetch log table object '%s' from %s.", table, json_data_path);
                goto fail_put;
        }
#endif

        n = json_object_array_length(log_table);

        /* Last entry is null to indicate end, so alloc n+1 items */
        if ((patterns = calloc(n+1, sizeof(fwts_log_pattern))) == NULL) {
                fwts_log_error(fw, "Cannot allocate pattern table.");
                goto fail_put;
        }

        /* Now fetch json objects and compile regex */
        for (i = 0; i < n; i++) {
                const char *str;
                json_object *obj;

                obj = json_object_array_get_idx(log_table, i);
                if (FWTS_JSON_ERROR(obj)) {
                        fwts_log_error(fw, "Cannot fetch %d item from table %s.", i, table);
                        goto fail;
                }
                if ((str = fwts_json_str(fw, table, i, obj, "compare_mode", true)) == NULL)
                        goto fail;
                patterns[i].compare_mode = fwts_log_compare_mode_str_to_val(str);

                if ((str = fwts_json_str(fw, table, i, obj, "log_level", true)) == NULL)
                        goto fail;
                patterns[i].level   = fwts_log_str_to_level(str);

                if ((patterns[i].pattern = fwts_json_str(fw, table, i, obj, "pattern", true)) == NULL)
                        goto fail;

                if ((patterns[i].advice = fwts_json_str(fw, table, i, obj, "advice", true)) == NULL)
                        goto fail;

                /* Labels appear in fwts 0.26.0, so are optional with older versions */
                str = fwts_json_str(fw, table, i, obj, "label", false);
                if (str) {
                        patterns[i].label = strdup(str);
                } else {
                        /* if not specified, auto-magically generate */
                        patterns[i].label = strdup(fwts_log_unique_label(patterns[i].pattern, label));
                }
                if (patterns[i].label == NULL)
                        goto fail;

                if (patterns[i].compare_mode == FWTS_COMPARE_REGEX) {
                        int rc;

                        rc = regcomp(&patterns[i].compiled, patterns[i].pattern, REG_EXTENDED);
                        if (rc) {
                                fwts_log_error(fw, "Regex %s failed to compile: %d.", patterns[i].pattern, rc);
                                patterns[i].compiled_ok = false;
                        } else {
                                patterns[i].compiled_ok = true;
                        }
                }
        }
        /* We've now collected up the scan patterns, lets scan the log for errors */
        ret = fwts_log_scan(fw, log, fwts_log_scan_patterns_func, progress, patterns, errors, remove_timestamp);

fail:
        for (i = 0; i < n; i++) {
                if (patterns[i].compiled_ok)
                        regfree(&patterns[i].compiled);
                if (patterns[i].label)
                        free(patterns[i].label);
        }
        free(patterns);
fail_put:
        json_object_put(log_objs);

        return ret;
}

static void fwts_log_regex_find_callback(fwts_framework *fw, char *line, int repeated,
        char *prev, void *pattern, int *match)
{
        int rc;
        regex_t compiled;

        FWTS_UNUSED(fw);
        FWTS_UNUSED(repeated);
        FWTS_UNUSED(prev);

        rc = regcomp(&compiled, (char *)pattern, REG_EXTENDED);
        if (rc) {
                fwts_log_error(fw, "Regex %s failed to compile: %d.", (char *)pattern, rc);
        } else {
                rc = regexec(&compiled, line, 0, NULL, 0);
                regfree(&compiled);
                if (!rc)
                        (*match)++;
        }
}

/*
 * fwts_log_regex_find()
 *      scan a log list of lines for a given regex pattern
 *      uses fwts_log_regex_find_callback() callback
 */
int fwts_log_regex_find(fwts_framework *fw, fwts_list *log, char *pattern, bool remove_timestamp)
{
        int found = 0;

        fwts_log_scan(fw, log, fwts_log_regex_find_callback, NULL, pattern, &found, remove_timestamp);

        return found;
}
