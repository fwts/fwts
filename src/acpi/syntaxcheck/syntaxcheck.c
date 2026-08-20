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
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define MAX_TABLES	(128)

#define ASL_EXCEPTIONS
#include "aslmessages.h"

typedef struct {
	const uint16_t	error_number;
	const char	*id_str;
	char		*advice;
} syntaxcheck_error_map_item;

static int syntaxcheck_load_advice(fwts_framework *fw);
static void syntaxcheck_free_advice(void);

/*
 *  syntaxcheck advice data file, data stored in json format
 */
#define SYNTAXCHECK_JSON_FILE		"syntaxcheck.json"

#define ASL_ID(error)	{ error, #error, NULL }

/*
 *  From aslmessages.h, current ASL errors
 * 	create a mapping from value to stringified name
 */
static syntaxcheck_error_map_item syntaxcheck_error_map[] = {
	ASL_ID(ASL_MSG_ALIGNMENT),
	ASL_ID(ASL_MSG_ALPHANUMERIC_STRING),
	ASL_ID(ASL_MSG_AML_NOT_IMPLEMENTED),
	ASL_ID(ASL_MSG_ARG_COUNT_HI),
	ASL_ID(ASL_MSG_ARG_COUNT_LO),
	ASL_ID(ASL_MSG_ARG_INIT),
	ASL_ID(ASL_MSG_BACKWARDS_OFFSET),
	ASL_ID(ASL_MSG_BUFFER_LENGTH),
	ASL_ID(ASL_MSG_CLOSE),
	ASL_ID(ASL_MSG_COMPILER_INTERNAL),
	ASL_ID(ASL_MSG_COMPILER_RESERVED),
	ASL_ID(ASL_MSG_CONNECTION_MISSING),
	ASL_ID(ASL_MSG_CONNECTION_INVALID),
	ASL_ID(ASL_MSG_CONSTANT_EVALUATION),
	ASL_ID(ASL_MSG_CONSTANT_FOLDED),
	ASL_ID(ASL_MSG_CORE_EXCEPTION),
	ASL_ID(ASL_MSG_DEBUG_FILE_OPEN),
	ASL_ID(ASL_MSG_DEBUG_FILENAME),
	ASL_ID(ASL_MSG_DEPENDENT_NESTING),
	ASL_ID(ASL_MSG_DMA_CHANNEL),
	ASL_ID(ASL_MSG_DMA_LIST),
	ASL_ID(ASL_MSG_DUPLICATE_CASE),
	ASL_ID(ASL_MSG_DUPLICATE_ITEM),
	ASL_ID(ASL_MSG_EARLY_EOF),
	ASL_ID(ASL_MSG_ENCODING_LENGTH),
	ASL_ID(ASL_MSG_EX_INTERRUPT_LIST),
	ASL_ID(ASL_MSG_EX_INTERRUPT_LIST_MIN),
	ASL_ID(ASL_MSG_EX_INTERRUPT_NUMBER),
	ASL_ID(ASL_MSG_FIELD_ACCESS_WIDTH),
	ASL_ID(ASL_MSG_FIELD_UNIT_ACCESS_WIDTH),
	ASL_ID(ASL_MSG_FIELD_UNIT_OFFSET),
	ASL_ID(ASL_MSG_GPE_NAME_CONFLICT),
	ASL_ID(ASL_MSG_HID_LENGTH),
	ASL_ID(ASL_MSG_HID_PREFIX),
	ASL_ID(ASL_MSG_HID_SUFFIX),
	ASL_ID(ASL_MSG_INCLUDE_FILE_OPEN),
	ASL_ID(ASL_MSG_INPUT_FILE_OPEN),
	ASL_ID(ASL_MSG_INTEGER_LENGTH),
	ASL_ID(ASL_MSG_INTEGER_OPTIMIZATION),
	ASL_ID(ASL_MSG_INTERRUPT_LIST),
	ASL_ID(ASL_MSG_INTERRUPT_NUMBER),
	ASL_ID(ASL_MSG_INVALID_ACCESS_SIZE),
	ASL_ID(ASL_MSG_INVALID_ADDR_FLAGS),
	ASL_ID(ASL_MSG_INVALID_CONSTANT_OP),
	ASL_ID(ASL_MSG_INVALID_EISAID),
	ASL_ID(ASL_MSG_INVALID_ESCAPE),
	ASL_ID(ASL_MSG_INVALID_GRAN_FIXED),
	ASL_ID(ASL_MSG_INVALID_GRANULARITY),
	ASL_ID(ASL_MSG_INVALID_LENGTH),
	ASL_ID(ASL_MSG_INVALID_LENGTH_FIXED),
	ASL_ID(ASL_MSG_INVALID_MIN_MAX),
	ASL_ID(ASL_MSG_INVALID_OPERAND),
	ASL_ID(ASL_MSG_INVALID_PERFORMANCE),
	ASL_ID(ASL_MSG_INVALID_PRIORITY),
	ASL_ID(ASL_MSG_INVALID_STRING),
	ASL_ID(ASL_MSG_INVALID_TARGET),
	ASL_ID(ASL_MSG_INVALID_TIME),
	ASL_ID(ASL_MSG_INVALID_TYPE),
	ASL_ID(ASL_MSG_INVALID_UUID),
	ASL_ID(ASL_MSG_ISA_ADDRESS),
	ASL_ID(ASL_MSG_LEADING_ASTERISK),
	ASL_ID(ASL_MSG_LIST_LENGTH_LONG),
	ASL_ID(ASL_MSG_LIST_LENGTH_SHORT),
	ASL_ID(ASL_MSG_LISTING_FILE_OPEN),
	ASL_ID(ASL_MSG_LISTING_FILENAME),
	ASL_ID(ASL_MSG_LOCAL_INIT),
	ASL_ID(ASL_MSG_LOCAL_OUTSIDE_METHOD),
	ASL_ID(ASL_MSG_LONG_LINE),
	ASL_ID(ASL_MSG_MEMORY_ALLOCATION),
	ASL_ID(ASL_MSG_MISSING_ENDDEPENDENT),
	ASL_ID(ASL_MSG_MISSING_STARTDEPENDENT),
	ASL_ID(ASL_MSG_MULTIPLE_DEFAULT),
	ASL_ID(ASL_MSG_MULTIPLE_TYPES),
	ASL_ID(ASL_MSG_NAME_EXISTS),
	ASL_ID(ASL_MSG_NAME_OPTIMIZATION),
	ASL_ID(ASL_MSG_NAMED_OBJECT_IN_WHILE),
	ASL_ID(ASL_MSG_NESTED_COMMENT),
	ASL_ID(ASL_MSG_NO_CASES),
	ASL_ID(ASL_MSG_NO_REGION),
	ASL_ID(ASL_MSG_NO_RETVAL),
	ASL_ID(ASL_MSG_NO_WHILE),
	ASL_ID(ASL_MSG_NON_ASCII),
	ASL_ID(ASL_MSG_BUFFER_FIELD_LENGTH),
	ASL_ID(ASL_MSG_NOT_EXIST),
	ASL_ID(ASL_MSG_NOT_FOUND),
	ASL_ID(ASL_MSG_NOT_METHOD),
	ASL_ID(ASL_MSG_NOT_PARAMETER),
	ASL_ID(ASL_MSG_NOT_REACHABLE),
	ASL_ID(ASL_MSG_NOT_REFERENCED),
	ASL_ID(ASL_MSG_NULL_DESCRIPTOR),
	ASL_ID(ASL_MSG_NULL_STRING),
	ASL_ID(ASL_MSG_OPEN),
	ASL_ID(ASL_MSG_OUTPUT_FILE_OPEN),
	ASL_ID(ASL_MSG_OUTPUT_FILENAME),
	ASL_ID(ASL_MSG_PACKAGE_LENGTH),
	ASL_ID(ASL_MSG_PREPROCESSOR_FILENAME),
	ASL_ID(ASL_MSG_READ),
	ASL_ID(ASL_MSG_RECURSION),
	ASL_ID(ASL_MSG_REGION_BUFFER_ACCESS),
	ASL_ID(ASL_MSG_REGION_BYTE_ACCESS),
	ASL_ID(ASL_MSG_RESERVED_ARG_COUNT_HI),
	ASL_ID(ASL_MSG_RESERVED_ARG_COUNT_LO),
	ASL_ID(ASL_MSG_RESERVED_METHOD),
	ASL_ID(ASL_MSG_RESERVED_NO_RETURN_VAL),
	ASL_ID(ASL_MSG_RESERVED_OPERAND_TYPE),
	ASL_ID(ASL_MSG_RESERVED_PACKAGE_LENGTH),
	ASL_ID(ASL_MSG_RESERVED_RETURN_VALUE),
	ASL_ID(ASL_MSG_RESERVED_USE),
	ASL_ID(ASL_MSG_RESERVED_WORD),
	ASL_ID(ASL_MSG_RESOURCE_FIELD),
	ASL_ID(ASL_MSG_RESOURCE_INDEX),
	ASL_ID(ASL_MSG_RESOURCE_LIST),
	ASL_ID(ASL_MSG_RESOURCE_SOURCE),
	ASL_ID(ASL_MSG_RESULT_NOT_USED),
	ASL_ID(ASL_MSG_RETURN_TYPES),
	ASL_ID(ASL_MSG_SCOPE_FWD_REF),
	ASL_ID(ASL_MSG_SCOPE_TYPE),
	ASL_ID(ASL_MSG_SEEK),
	ASL_ID(ASL_MSG_SERIALIZED),
	ASL_ID(ASL_MSG_SERIALIZED_REQUIRED),
	ASL_ID(ASL_MSG_SINGLE_NAME_OPTIMIZATION),
	ASL_ID(ASL_MSG_SOME_NO_RETVAL),
	ASL_ID(ASL_MSG_STRING_LENGTH),
	ASL_ID(ASL_MSG_SWITCH_TYPE),
	ASL_ID(ASL_MSG_SYNC_LEVEL),
	ASL_ID(ASL_MSG_SYNTAX),
	ASL_ID(ASL_MSG_TABLE_SIGNATURE),
	ASL_ID(ASL_MSG_TAG_LARGER),
	ASL_ID(ASL_MSG_TAG_SMALLER),
	ASL_ID(ASL_MSG_TIMEOUT),
	ASL_ID(ASL_MSG_TOO_MANY_TEMPS),
	ASL_ID(ASL_MSG_TRUNCATION),
	ASL_ID(ASL_MSG_UNKNOWN_RESERVED_NAME),
	ASL_ID(ASL_MSG_UNREACHABLE_CODE),
	ASL_ID(ASL_MSG_UNSUPPORTED),
	ASL_ID(ASL_MSG_UPPER_CASE),
	ASL_ID(ASL_MSG_VENDOR_LIST),
	ASL_ID(ASL_MSG_WRITE),
	ASL_ID(ASL_MSG_RANGE),
	ASL_ID(ASL_MSG_BUFFER_ALLOCATION),
	ASL_ID(ASL_MSG_MISSING_DEPENDENCY),
	ASL_ID(ASL_MSG_ILLEGAL_FORWARD_REF),
	ASL_ID(ASL_MSG_ILLEGAL_METHOD_REF),
	ASL_ID(ASL_MSG_LOCAL_NOT_USED),
	ASL_ID(ASL_MSG_ARG_AS_LOCAL_NOT_USED),
	ASL_ID(ASL_MSG_ARG_NOT_USED),
	ASL_ID(ASL_MSG_CONSTANT_REQUIRED),
	ASL_ID(ASL_MSG_CROSS_TABLE_SCOPE),
	ASL_ID(ASL_MSG_EXCEPTION_NOT_RECEIVED),
	ASL_ID(ASL_MSG_NULL_RESOURCE_TEMPLATE),
	ASL_ID(ASL_MSG_FOUND_HERE),
	ASL_ID(ASL_MSG_ILLEGAL_RECURSION),
	ASL_ID(ASL_MSG_DUPLICATE_INPUT_FILE),
	ASL_ID(ASL_MSG_WARNING_AS_ERROR),
	ASL_ID(ASL_MSG_OEM_TABLE_ID),
	ASL_ID(ASL_MSG_OEM_ID),
	ASL_ID(ASL_MSG_UNLOAD),
	ASL_ID(ASL_MSG_OFFSET),
	ASL_ID(ASL_MSG_LONG_SLEEP),
	ASL_ID(ASL_MSG_PREFIX_NOT_EXIST),
	ASL_ID(ASL_MSG_NAMEPATH_NOT_EXIST),
	ASL_ID(ASL_MSG_REGION_LENGTH),
	ASL_ID(ASL_MSG_TEMPORARY_OBJECT),
	ASL_ID(ASL_MSG_UNDEFINED_EXTERNAL),
	ASL_ID(ASL_MSG_BUFFER_FIELD_OVERFLOW),
	ASL_ID(ASL_MSG_BUFFER_ELEMENT),
	ASL_ID(ASL_MSG_DIVIDE_BY_ZERO),
	ASL_ID(ASL_MSG_FLAG_VALUE),
	ASL_ID(ASL_MSG_INTEGER_SIZE),
	ASL_ID(ASL_MSG_INVALID_EXPRESSION),
	ASL_ID(ASL_MSG_INVALID_FIELD_NAME),
	ASL_ID(ASL_MSG_INVALID_HEX_INTEGER),
	ASL_ID(ASL_MSG_OEM_TABLE),
	ASL_ID(ASL_MSG_RESERVED_VALUE),
	ASL_ID(ASL_MSG_UNKNOWN_LABEL),
	ASL_ID(ASL_MSG_UNKNOWN_SUBTABLE),
	ASL_ID(ASL_MSG_UNKNOWN_TABLE),
	ASL_ID(ASL_MSG_ZERO_VALUE),
	ASL_ID(ASL_MSG_DIRECTIVE_SYNTAX),
	ASL_ID(ASL_MSG_ENDIF_MISMATCH),
	ASL_ID(ASL_MSG_ERROR_DIRECTIVE),
	ASL_ID(ASL_MSG_EXISTING_NAME),
	ASL_ID(ASL_MSG_INVALID_INVOCATION),
	ASL_ID(ASL_MSG_MACRO_SYNTAX),
	ASL_ID(ASL_MSG_TOO_MANY_ARGUMENTS),
	ASL_ID(ASL_MSG_UNKNOWN_DIRECTIVE),
	ASL_ID(ASL_MSG_UNKNOWN_PRAGMA),
	ASL_ID(ASL_MSG_WARNING_DIRECTIVE),
	ASL_ID(ASL_MSG_INCLUDE_FILE),
	{ 0, NULL, NULL }
};

static int syntaxcheck_init(fwts_framework *fw)
{
	(void)syntaxcheck_load_advice(fw);

	if (fwts_iasl_init(fw) != FWTS_OK) {
		fwts_aborted(fw, "Failure to initialise iasl, aborting.");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static int syntaxcheck_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_iasl_deinit();
	syntaxcheck_free_advice();

	return FWTS_OK;
}

static inline uint16_t syntaxcheck_error_code_to_error_level(const uint32_t error_code)
{
	uint16_t error_level = (error_code / 1000) - 1;

	return error_level;
}

static inline uint16_t syntaxcheck_error_code_to_error_number(const uint32_t error_code)
{
	uint16_t error_number = (error_code % 1000);

	return error_number;
}

static const char *syntaxcheck_error_code_to_id(const uint32_t error_code)
{
	int i;
	uint16_t error_number = syntaxcheck_error_code_to_error_number(error_code);
	static const char *unknown = "Unknown";

	for (i = 0; syntaxcheck_error_map[i].id_str != NULL; i++) {
		if (syntaxcheck_error_map[i].error_number == error_number)
			return syntaxcheck_error_map[i].id_str;
	}

	return unknown;
}

static const char *syntaxcheck_error_level(uint32_t error_code)
{
	uint16_t error_level = syntaxcheck_error_code_to_error_level(error_code);
	static char buf[64];
	char *ptr;

	/* Out of range for some reason? */
	if (error_level >= ASL_NUM_REPORT_LEVELS)
		return "Unknown";

	/* AslErrorLevel strings are end-space padded, so strip off end spaces if any */
	strncpy(buf, fwts_iasl_exception_level((uint8_t)error_level), sizeof(buf));
	buf[sizeof(buf) -1] = '\0';
	ptr = strchr(buf, ' ');
	if (ptr)
		*ptr = '\0';

	return buf;
}

/*
 *  syntaxcheck_dump_code()
 *	output a block of source around where the error is reported
 */
static void syntaxcheck_dump_code(fwts_framework *fw,
	int error_code,
	int carat_offset,
	char *error_message,
	fwts_list* iasl_disassembly,
	int error_line,
	int howmany)
{
	int i = 0;
	fwts_list_link *item;

	fwts_log_info_verbatim(fw, "Line | AML source\n");
	fwts_log_underline(fw->results, '-');

	fwts_list_foreach(item, iasl_disassembly) {
		i++;
		if (i >= error_line + (howmany / 2))
			break;
		if (i > error_line - (howmany / 2)) {
			fwts_log_info_verbatim(fw, "%5.5d| %s\n", i,
				fwts_text_list_text(item));
			if (i == error_line) {
				fwts_log_info_verbatim(fw, "     | %*.*s", carat_offset, carat_offset, "^");
				fwts_log_info_verbatim(fw, "     | %s %d: %s\n",
					syntaxcheck_error_level(error_code), error_code, error_message);
			}
		}
	}
	fwts_log_underline(fw->results, '=');
}

/*
 *  syntaxcheck_load_advice()
 *	load error, advice string tuple from json formatted data file
 *	this populates the syntaxcheck_error_map advice field.
 */
static int syntaxcheck_load_advice(fwts_framework *fw)
{
	int ret = FWTS_ERROR;
	int n;
	int i;
	json_object *syntaxcheck_table;
	json_object *syntaxcheck_objs;
	char json_data_path[PATH_MAX];

	snprintf(json_data_path, sizeof(json_data_path), "%s/%s", fw->json_data_path, SYNTAXCHECK_JSON_FILE);

	syntaxcheck_objs = json_object_from_file(json_data_path);
	if (FWTS_JSON_ERROR(syntaxcheck_objs)) {
		fwts_log_error(fw, "Cannot load objects table from %s.", json_data_path);
		return FWTS_ERROR;
	}
#if JSON_HAS_GET_EX
        if (!json_object_object_get_ex(syntaxcheck_objs, "erroradvice", &syntaxcheck_table)) {
                fwts_log_error(fw, "Cannot fetch syntaxcheck table from %s.", json_data_path);
                goto fail_put;
        }
#else
	syntaxcheck_table = json_object_object_get(syntaxcheck_objs, "erroradvice");
	if (FWTS_JSON_ERROR(syntaxcheck_table)) {
                fwts_log_error(fw, "Cannot fetch syntaxcheck table from %s.", json_data_path);
                goto fail_put;
        }
#endif

	n = json_object_array_length(syntaxcheck_table);

	/* Now fetch json objects */
	for (i = 0; i < n; i++) {
		const char *advice, *id_str;
		json_object *obj;
#if JSON_HAS_GET_EX
		json_object *obj_str;
#endif
		int j;

		obj = json_object_array_get_idx(syntaxcheck_table, i);
		if (FWTS_JSON_ERROR(obj)) {
			fwts_log_error(fw, "Cannot fetch %d item from syntaxcheck table.", i);
			break;
		}

#if JSON_HAS_GET_EX
		if (!json_object_object_get_ex(obj, "advice", &obj_str)) {
			fwts_log_error(fw, "Cannot fetch advice object from item %d.", i);
			break;
		}
		advice = json_object_get_string(obj_str);
#else
		advice = json_object_get_string(json_object_object_get(obj, "advice"));
#endif
		if (FWTS_JSON_ERROR(advice)) {
			fwts_log_error(fw, "Cannot fetch advice string from item %d.", i);
			break;
		}
#if JSON_HAS_GET_EX
		if (!json_object_object_get_ex(obj, "id", &obj_str)) {
			fwts_log_error(fw, "Cannot fetch ID object from item %d.", i);
			break;
		}
		id_str = json_object_get_string(obj_str);
#else
		id_str = json_object_get_string(json_object_object_get(obj, "id"));
#endif
		if (FWTS_JSON_ERROR(id_str)) {
			fwts_log_error(fw, "Cannot fetch ID string from item %d.", i);
			break;
		}

		for (j = 0; syntaxcheck_error_map[j].id_str != NULL; j++) {
			if (strcmp(id_str, syntaxcheck_error_map[j].id_str) == 0) {
				/* Matching id, so update the advice field */
				syntaxcheck_error_map[j].advice = strdup(advice);
				break;
			}
		}
	}

	ret = FWTS_OK;

fail_put:
	json_object_put(syntaxcheck_objs);

	return ret;
}

/*
 *  syntaxcheck_free_advice()
 * 	free up the adviceinfo data
 */
static void syntaxcheck_free_advice(void)
{
	int i;

	for (i = 0; syntaxcheck_error_map[i].id_str != NULL; i++)
		if (syntaxcheck_error_map[i].advice)
			free(syntaxcheck_error_map[i].advice);

}

/*
 *  syntaxcheck_give_advice()
 *	look up advice on a given error number and give advice if any exists
 */
static void syntaxcheck_give_advice(fwts_framework *fw, uint32_t error_code)
{
	int i;
	/* iasl encodes error_codes as follows: */
	uint16_t error_number = syntaxcheck_error_code_to_error_number(error_code);

	for (i = 0; syntaxcheck_error_map[i].id_str != NULL; i++) {
		if ((syntaxcheck_error_map[i].error_number == error_number) &&
		    (syntaxcheck_error_map[i].advice != NULL)) {
			fwts_advice(fw, "(for %s #%d, %s): %s",
				syntaxcheck_error_level(error_code),
				error_code,
				syntaxcheck_error_map[i].id_str,
				syntaxcheck_error_map[i].advice);
			break;
		}
	}
}

/*
 *  syntaxcheck_single_table()
 *	disassemble and reassemble a table, check for errors. which indicates the Nth
 *	table
 */
static int syntaxcheck_single_table(
	fwts_framework *fw,
	const fwts_acpi_table_info *info,
	const int n)
{
	fwts_list_link *item;
	int errors = 0;
	int warnings = 0;
	int remarks = 0;
	fwts_list *iasl_stdout = NULL,
		  *iasl_stderr = NULL,
		  *iasl_disassembly = NULL;

	if (fwts_iasl_reassemble(fw, info,
		&iasl_disassembly, &iasl_stdout, &iasl_stderr) != FWTS_OK) {
		fwts_text_list_free(iasl_disassembly);
		fwts_text_list_free(iasl_stderr);
		fwts_text_list_free(iasl_stdout);
		fwts_aborted(fw, "Cannot re-assasemble with iasl.");
		return FWTS_ERROR;
	}

	fwts_log_nl(fw);
	fwts_log_info(fw, "Checking ACPI table %s (#%d)", info->name, n);
	fwts_log_nl(fw);

	if (iasl_stderr) {
		/* Scan error text from assembly */
		fwts_list_foreach(item, iasl_stderr) {
			int num;
			char ch;
			char *line = fwts_text_list_text(item);

			if ((sscanf(line, "%*s %d%c", &num, &ch) == 2) && ch == ':') {
				if (item->next != NULL) {
					char *error_text = fwts_text_list_text(item->next);
					int iasl_error = (strstr(error_text, "Error") != NULL);
					int iasl_warning = (strstr(error_text, "Warning") != NULL);
					int iasl_remark = (strstr(error_text, "Remark") != NULL);
					int error_code;

					sscanf(error_text, "%*s %d", &error_code);

					/* Valid error or warning, go and report */
					if (iasl_error || iasl_warning || iasl_remark) {
						char label[64];
						char *colon = strstr(line, ":");
						char *carat = strstr(error_text, "^");
						char *ptr;
						int colon_offset = (colon == NULL) ? 0 : colon + 1 - line;
						int carat_offset = (carat == NULL) ? 0 : carat - error_text;
						uint16_t error_level =
							syntaxcheck_error_code_to_error_level((uint32_t)error_code);
						bool skip = false;

						/* trim */
						fwts_chop_newline(error_text);

						/* Strip out the ^ from the error message */
						for (ptr = error_text; *ptr; ptr++)
							if (*ptr == '^')
								*ptr = ' ';

						/* Look for error message after: "Error:  4042 - " prefix */
						ptr = strstr(error_text, "-");
						if (ptr)
							ptr += 2;
						else
							ptr = error_text;	/* Urgh, none found, default */

						/* Skip over leading white space */
						while (*ptr == ' ')
							ptr++;

						snprintf(label, sizeof(label), "AMLAsm%s",
							syntaxcheck_error_code_to_id(error_code));

						switch (error_level) {
						case ASL_WARNING:
						case ASL_WARNING2:
						case ASL_WARNING3:
							fwts_failed(fw, LOG_LEVEL_MEDIUM, label, "Assembler warning in line %d", num);
							break;
						case ASL_ERROR:
							fwts_failed(fw, LOG_LEVEL_HIGH, label, "Assembler error in line %d", num);
							break;
						case ASL_REMARK:
							if (syntaxcheck_error_code_to_error_number(error_code) == ASL_MSG_COMPILER_RESERVED)
								fwts_log_info(fw, "Assembler remark in line %d", num);
							else
								fwts_failed(fw, LOG_LEVEL_LOW, label, "Assembler remark in line %d", num);
							break;
						case ASL_OPTIMIZATION:
							skip = true;
							break;
						default:
							fwts_log_info(fw, "Assembler message in line %d", num);
							break;
						}

						if (!skip) {
							syntaxcheck_dump_code(fw, error_code,
								carat_offset - colon_offset, ptr,
								iasl_disassembly, num, 8);
							syntaxcheck_give_advice(fw, error_code);
						}
					}
					errors += iasl_error;
					warnings += iasl_warning;
					remarks += iasl_remark;
					item = item->next;
				}
				else {
					fwts_log_info(fw, "%s", line);
					fwts_log_error(fw,
						"Could not find parser error message "
						"(this can happen if iasl segfaults!)");
				}
			}
		}
	}

	/*
	 *  Did iasl emit any errors that we need to check?
	 */
	if (iasl_stderr) {
		fwts_list_foreach(item, iasl_stderr) {
			char *line = fwts_text_list_text(item);
			if (strstr(line, "Compiler aborting due to parser-detected syntax error")) {
				fwts_failed(fw, LOG_LEVEL_HIGH, "SyntaxCheckIASLCompilerAborted",
					"Compilation aborted early due to a parser detected syntax error.");
				fwts_advice(fw,
					"Some subsequent errors may not be detected because the "
					"compiler had to terminate prematurely.  If the compiler did not "
					"abort early then potentially correct code may parse incorrectly "
					"producing some or many false positive errors.");
			}
		}
	}

	fwts_text_list_free(iasl_disassembly);
	fwts_text_list_free(iasl_stdout);
	fwts_text_list_free(iasl_stderr);

	if (errors + warnings + remarks > 0)
		fwts_log_info(fw, "Table %s (%d) reassembly: Found %d errors, %d warnings, %d remarks.",
			info->name, n, errors, warnings, remarks);
	else
		fwts_passed(fw, "%s (%d) reassembly, Found 0 errors, 0 warnings, 0 remarks.", info->name, n);

	fwts_log_nl(fw);

	return FWTS_OK;
}

static int syntaxcheck_tables(fwts_framework *fw)
{
	int i, n;

	for (i = 0, n = 0; i < ACPI_MAX_TABLES; i++) {
		fwts_acpi_table_info *info;

		if (fwts_acpi_get_table(fw, i, &info) != FWTS_OK)
			break;
		if (info && info->has_aml)
			syntaxcheck_single_table(fw, info, n++);
	}

	return FWTS_OK;
}

static fwts_framework_minor_test syntaxcheck_tests[] = {
	{ syntaxcheck_tables, "Disassemble and reassemble DSDT and SSDTs." },
	{ NULL, NULL }
};

static fwts_framework_ops syntaxcheck_ops = {
	.description = "Re-assemble DSDT and SSDTs to find syntax errors and warnings.",
	.init        = syntaxcheck_init,
	.deinit      = syntaxcheck_deinit,
	.minor_tests = syntaxcheck_tests
};

FWTS_REGISTER("syntaxcheck", &syntaxcheck_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH_EXPERIMENTAL)

#endif
