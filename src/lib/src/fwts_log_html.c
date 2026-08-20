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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include "fwts.h"

#define MAX_HTML_STACK	(64)
#define HTML_INDENT	(2)

typedef struct {
	const char *name;
} fwts_log_html_stack_t;

typedef struct {
	unsigned char	ch;	/* ASCII */
	char *		html;	/* HTML equivalent */
} fwts_log_html_ascii_t;

static fwts_log_html_stack_t html_stack[MAX_HTML_STACK];
static int html_stack_index = 0;

static void fwts_log_html(fwts_log_file *log_file, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	fprintf(log_file->fp, "%*s", html_stack_index * HTML_INDENT, "");
	vfprintf(log_file->fp, fmt, args);

	va_end(args);
}

/*
 *  fwts_log_html_convert_ascii()
 *	return a static string containing any conversion from ASCII to a
 *	HTML representation of char ch.  If no conversion needed, just
 *	return a string containing the ch.
 */
static char *fwts_log_html_convert_ascii(const char ch)
{
	/*
	 * ASCII to HTML conversion table:
	 * ISO 10646, ISO 8879, ISO 8859-1 Latin alphabet No. 1
	 * Browser support: All browsers
	 */
	static const fwts_log_html_ascii_t fwts_log_html_ascii_table[] = {
		{ '"',	"&quot;" },
		{ '&', 	"&amp;" },
		{ '<', 	"&lt;" },
		{ '>',	"&gt;" },
		{ '{',	"&#123;" },
		{ '|',	"&#124;" },
		{ '}',	"&#125;" },
		{ '~',	"&#126;" },

		{ 160,	"&nbsp;" },
		{ 161,	"&iexl;" },
		{ 162,	"&cent;" },
		{ 163,	"&pound;" },
		{ 164,	"&curren;" },
		{ 165,	"&yen;" },
		{ 166,	"&brvbar;" },
		{ 167,	"&sect;" },
		{ 168,	"&uml;" },
		{ 169,	"&copy;" },
		{ 170,	"&ordf;" },
		{ 171,	"&laquo;" },
		{ 172,	"&not;" },
		{ 173,	"&shy;" },
		{ 174,	"&reg;" },
		{ 175,	"&macr;" },

		{ 176,	"&deg;" },
		{ 177,	"&plusmn;" },
		{ 178,	"&sup2;" },
		{ 179,	"&sup3;" },
		{ 180,	"&acute;" },
		{ 181,	"&micro;" },
		{ 182,	"&para;" },
		{ 183,	"&middot;" },
		{ 184,	"&cedil;" },
		{ 185,	"&sup1;" },
		{ 186,	"&ordm;" },
		{ 187,	"&raquo;" },
		{ 187,	"&frac14;" },
		{ 189,	"&frac12;" },
		{ 190,	"&frac34;" },
		{ 191,	"&iquest;" },

		/* Probably never used, but here in case */

		{ 192,	"&Agrave;" },
		{ 193,	"&Aacute;" },
		{ 194,	"&Acirc;" },
		{ 195,	"&Atilde;" },
		{ 196,	"&Auml;" },
		{ 197,	"&Aring;" },
		{ 198,	"&AElig;" },
		{ 199,	"&Ccedil;" },
		{ 200,	"&Egrave;" },
		{ 201,	"&Eacute;" },
		{ 202,	"&Ecirc;" },
		{ 203,	"&Euml;" },
		{ 204,	"&lgrave;" },
		{ 205,	"&lacute;" },
		{ 206,	"&lcirc;" },
		{ 207,	"&luml;" },

		{ 208,	"&ETH;" },
		{ 209,	"&Ntilde;" },
		{ 210,	"&Ograve;" },
		{ 211,	"&Oacute;" },
		{ 212,	"&Ocirc;" },
		{ 213,	"&Otilde;" },
		{ 214,	"&Ouml;" },
		{ 215,	"&times;" },
		{ 216,	"&Oslash;" },
		{ 217,	"&Ugrave;" },
		{ 218,	"&Uacute;" },
		{ 219,	"&Ucirc;" },
		{ 220,	"&Uuml;" },
		{ 221,	"&Yacute;" },
		{ 222,	"&THORN;" },
		{ 223,	"&szlig;" },

		{ 224,	"&agrave;" },
		{ 225,	"&aacute;" },
		{ 226,	"&acirc;" },
		{ 227,	"&atilde;" },
		{ 228,	"&auml;" },
		{ 229,	"&aring;" },
		{ 230,	"&aelig;" },
		{ 231,	"&ccedil;" },
		{ 232,	"&egrave;" },
		{ 233,	"&eacute;" },
		{ 234,	"&ecirc;" },
		{ 235,	"&emuml;" },
		{ 236,	"&igrave;" },
		{ 237,	"&iacute;" },
		{ 238,	"&icirc;" },
		{ 239,	"&iuml;" },

		{ 240,	"&eth;" },
		{ 241,	"&ntilde;" },
		{ 242,	"&ograve;" },
		{ 243,	"&oacute;" },
		{ 244,	"&ocirc;" },
		{ 245,	"&otilde;" },
		{ 246,	"&ouml;" },
		{ 247,	"&divide;" },
		{ 248,	"&oslash;" },
		{ 249,	"&ugrave;" },
		{ 250,	"&uacute;" },
		{ 251,	"&ucirc;" },
		{ 252,	"&uuml;" },
		{ 253,	"&yacute;" },
		{ 254,	"&thorn;" },
		{ 255,	"&yuml;" },

		{ 0, 	NULL },
	};

	int i;
	static char buf[2];

	for (i = 0; fwts_log_html_ascii_table[i].html != NULL; i++) {
		if (fwts_log_html_ascii_table[i].ch == ch)
			return fwts_log_html_ascii_table[i].html;
	}

	/* No mapping, just return string containing ch */
	buf[0] = ch;
	buf[1] = '\0';

	return buf;
}

/*
 *  fwts_log_html_convert_ascii_str()
 *	convert an ASCII string into a HTML encoded string. The returned
 *	string needs free'ing once finished with.
 */
static char *fwts_log_html_convert_ascii_str(const char *buffer)
{
	const char *str1;
	char *converted;
	size_t len = 0;

	/* Step 1, figure out how much space we need */
	for (str1 = buffer; *str1; str1++) {
		char *str = fwts_log_html_convert_ascii(*str1);
		len += strlen(str);
	}

	if ((converted = calloc(len + 1, 1)) == NULL)
		return NULL;

	/* Step 2, convert */
	for (str1 = buffer; *str1; str1++) {
		char *str = fwts_log_html_convert_ascii(*str1);
		strcat(converted, str);
	}

	return converted;
}


/*
 *  fwts_log_print_html()
 *	print to a log
 */
static int fwts_log_print_html(
	fwts_log_file *log_file,
	const fwts_log_field field,
	const fwts_log_level level,
	const char *status,
	const char *label,
	const char *prefix,
	const char *buffer)
{
	char *str;
	char *style;
	char *code_start;
	char *code_end;
	char *html_converted;

	FWTS_UNUSED(label);
	FWTS_UNUSED(prefix);

	if (!((field & LOG_FIELD_MASK) & fwts_log_filter))
		return 0;

	if (field & (LOG_NEWLINE | LOG_SEPARATOR | LOG_DEBUG))
		return 0;

	if ((html_converted = fwts_log_html_convert_ascii_str(buffer)) == NULL) {
		/* We can't report an error via the logging mechanism in case we loop */
		fprintf(stderr, "Out of memory converting html.\n");
		exit(EXIT_FAILURE);
	}

	fwts_log_html(log_file, "<TR>\n");

	if (field & LOG_VERBATUM) {
		code_start = "<PRE class=style_code>";
		code_end   = "</PRE>";
	} else {
		code_start = "";
		code_end   = "";
	}

	switch (field & LOG_FIELD_MASK) {
	case LOG_ERROR:
		fwts_log_html(log_file, "  <TD class=style_error>Error</TD>"
			"<TD COLSPAN=2>%s</TD>\n", html_converted);
		break;
	case LOG_WARNING:
		fwts_log_html(log_file, "  <TD class=style_error>Warning</TD>"
			"<TD COLSPAN=2 class=style_advice_info>%s%s%s</TD>\n",
			code_start, html_converted, code_end);
		break;
	case LOG_HEADING:
		fwts_log_html(log_file, "<TD COLSPAN=2 class=style_heading>%s%s%s</TD>\n",
			code_start, html_converted, code_end);
		break;
	case LOG_INFO:
		fwts_log_html(log_file, "  <TD></TD><TD COLSPAN=2 class=style_infos>%s%s%s</TD>\n",
			code_start, html_converted, code_end);
		break;
	case LOG_PASSED:
		fwts_log_html(log_file, "<TD class=style_passed>PASSED</TD><TD>%s</TD>\n",
			html_converted);
		break;
	case LOG_FAILED:
		switch (level) {
		case LOG_LEVEL_CRITICAL:
			style = " class=style_critical";
			break;
		case LOG_LEVEL_HIGH:
			style = " class=style_high";
			break;
		case LOG_LEVEL_MEDIUM:
			style = " class=style_medium";
			break;
		case LOG_LEVEL_LOW:
			style = " class=style_low";
			break;
		case LOG_LEVEL_INFO:
		case LOG_LEVEL_NONE:
		default:
			style = "";
			break;
		}
		str = fwts_log_level_to_str(level);

		fwts_log_html(log_file, "  <TD%s>%s [%s]</TD>\n", style, *status ? status : "", str);
		fwts_log_html(log_file, "  <TD>%s</TD>\n", html_converted);
		break;

	case LOG_SKIPPED:
		fwts_log_html(log_file, "<TD class=style_skipped>Skipped</TD>"
			"<TD>%s%s%s</TD>\n", code_start, html_converted, code_end);
		break;

	case LOG_SUMMARY:
		fwts_log_html(log_file, "  <TD></TD>"
			"<TD COLSPAN=2 class=style_summary>%s%s%s</TD>\n",
			code_start, html_converted, code_end);
		break;

	case LOG_ADVICE:
		fwts_log_html(log_file, "  <TD class=style_advice>Advice</TD>"
			"<TD COLSPAN=2 class=style_advice_info>%s%s%s</TD>\n",
			code_start, html_converted, code_end);
		break;

	default:
		break;
	}

	free(html_converted);

	fwts_log_html(log_file, "</TR>\n");
	fflush(log_file->fp);
	log_file->line_number++;	/* not used, but bump it anyway */

	return 0;
}

/*
 *  fwts_log_underline_html()
 *	write an underline across log, using character ch as the underline
 */
static void fwts_log_underline_html(fwts_log_file *log_file, const int ch)
{
	FWTS_UNUSED(log_file);
	FWTS_UNUSED(ch);

	/* No-op for html */
}

/*
 *  fwts_log_newline()
 *	write newline to log
 */
static void fwts_log_newline_html(fwts_log_file *log_file)
{
	FWTS_UNUSED(log_file);

	/* No-op for html */
}

static void fwts_log_section_begin_html(fwts_log_file *log_file, const char *name)
{
	if (!strcmp(name, "summary")) {
		fwts_log_html(log_file, "<TR><TD class=style_heading COLSPAN=2>Summary</TD></TR>\n");
	} else if (!strcmp(name, "heading")) {
		fwts_log_html(log_file, "<TR><TD class=style_heading COLSPAN=2>Firmware Test Suite</TD></TR>\n");
	} else if (!strcmp(name, "subtest_info")) {
		fwts_log_html(log_file, "<TR><TD class=style_subtest COLSPAN=2></TD></TR>\n");
	} else if (!strcmp(name, "failure")) {
		fwts_log_html(log_file, "<TR><TD class=style_heading COLSPAN=2></TD></TR>\n");
	}

	fflush(log_file->fp);

	if (html_stack_index < MAX_HTML_STACK) {
		html_stack[html_stack_index].name = name;
		html_stack_index++;
	} else  {
		fprintf(stderr, "html log stack overflow pushing section %s.\n", name);
		exit(EXIT_FAILURE);
	}
}

static void fwts_log_section_end_html(fwts_log_file *log_file)
{
	if (html_stack_index > 0) {
		html_stack_index--;
		fflush(log_file->fp);
	} else {
		fprintf(stderr, "html log stack underflow.\n");
		exit(EXIT_FAILURE);
	}

}

static void fwts_log_open_html(fwts_log_file *log_file)
{
	fwts_log_html(log_file, "<HTML>\n");
	fwts_log_html(log_file, "<HEAD>\n");
	fwts_log_html(log_file, "  <TITLE>fwts log</TITLE>\n");
	fwts_log_html(log_file, "</HEAD>\n");
	fwts_log_html(log_file, "<BODY>\n");
	fwts_log_html(log_file, "<STYLE>\n");
	fwts_log_html(log_file,
		".style_critical { background-color: red; font-weight: bold; "
		"text-align: center; vertical-align: center  }\n"
		".style_high { background-color: orange; font-weight: bold; "
		"text-align: center; vertical-align: center  }\n"
		".style_medium { background-color: yellow; font-weight: bold; "
		"text-align: center; vertical-align: center  }\n"
		".style_low { background-color: #9acd32; font-weight: bold; "
		"text-align: center; vertical-align: center  }\n"
		".style_passed { background-color: green; font-weight: bold; "
		"text-align: center; vertical-align: center  }\n"
		".style_advice { text-align: center; vertical-align: center; "
		"font-weight: bold }\n"
		".style_advice_info { font-style: italic; font-weight: bold }"
		".style_skipped { background-color: wheat; text-align: center; "
		"vertical-align: center }\n"
		".style_heading { background-color: wheat; font-weight: bold; "
		"text-align: center }\n"
		".style_summary { font-weight: bold }\n"
		".style_error { background-color: orange; font-weight: bold; "
		"text-align: center; vertical-align: center }\n"
		".style_subtest { background-color: lightgray; }\n"
		".style_infos { max-width:90em }\n"
		".style_code { font-family: \"courier\",\"mono\"; font-size:0.75em; overflow:auto; "
		"width:90%%; line-height:1.08em; font-stretch:extra-condensed; word-wrap:normal }\n");
	fwts_log_html(log_file, "</STYLE>\n");
	fflush(log_file->fp);

	fwts_log_html(log_file, "<TABLE WIDTH=1024>\n");
	fwts_log_html(log_file, "</TR>\n");

	fwts_log_section_begin_html(log_file, "fwts");
}

static void fwts_log_close_html(fwts_log_file *log_file)
{
	fwts_log_section_end_html(log_file);

	fwts_log_html(log_file, "</TABLE>\n");
	fwts_log_html(log_file, "</BODY>\n");
	fwts_log_html(log_file, "</HTML>\n");
	fflush(log_file->fp);
}

fwts_log_ops fwts_log_html_ops = {
	.print = 	 fwts_log_print_html,
	.underline =	 fwts_log_underline_html,
	.newline =	 fwts_log_newline_html,
	.section_begin = fwts_log_section_begin_html,
	.section_end   = fwts_log_section_end_html,
	.open          = fwts_log_open_html,
	.close	       = fwts_log_close_html
};
