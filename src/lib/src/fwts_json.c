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
 * This is a simplified json parser and json object store
 * based on json but does not support keywords and is designed
 * just for the fwts json data files. It is not a full json
 * implementation and should not be used as such.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "fwts.h"

/*
 *  json file information
 */
typedef struct {
	FILE *fp;		/* File pointer */
	const char *filename;	/* Name of file */
	int linenum;		/* Parser line number */
	int charnum;		/* Parser char position */
	int error_reported;	/* Error count */
} json_file;

/*
 *  json tokens, subset of full json token set as used
 *  for fwts requirements
 */
typedef enum {
	token_lbrace,
	token_rbrace,
	token_lbracket,
	token_rbracket,
	token_colon,
	token_comma,
	token_int,
	token_string,
	token_true,
	token_false,
	token_null,
	token_error,
	token_eof,
} json_token_type;

/*
 *  json parser token
 */
typedef struct {
	json_token_type type;	/* token type */
	long offset;		/* offset in file for re-winding */
	union {
		char *str;	/* token string value */
		int  intval;	/* token integer value */
	} u;
} json_token;

/*
 *  json_token_string()
 *	convert json token to a human readable string
 */
char *json_token_string(json_token *jtoken)
{
	static char tmp[64];

	switch (jtoken->type) {
	case token_lbrace:
		return "{";
	case token_rbrace:
		return "}";
	case token_lbracket:
		return "[";
	case token_rbracket:
		return "]";
	case token_colon:
		return ":";
	case token_comma:
		return ",";
	case token_int:
		(void)snprintf(tmp, sizeof(tmp), "%d", jtoken->u.intval);
		return tmp;
	case token_string:
		return jtoken->u.str;
	case token_error:
		return "<error>";
	case token_eof:
		return "end of file";
	default:
		break;
	}
	return "<illegal token>";
}

/*
 *  json_get_string()
 *	parse a literal string
 */
json_token_type json_get_string(json_file *jfile, json_token *token)
{
	char buffer[4096];
	size_t i = 0;

	for (;;) {
		int ch;

		ch = fgetc(jfile->fp);
		jfile->charnum++;
		if (ch == EOF) {
			fprintf(stderr, "json_parser: unexpected EOF in literal string\n");
			token->u.str = NULL;
			return token_error;
		}

		if (ch == '\\') {
			ch = fgetc(jfile->fp);
			switch (ch) {
			case '\\':
			case '"':
			case '/':
				break;
			case 'b':
				ch = '\b';
				break;
			case 'f':
				ch = '\f';
				break;
			case 'n':
				ch = '\n';
				break;
			case 'r':
				ch = '\r';
				break;
			case 't':
				ch = '\t';
				break;
			case 'u':
				fprintf(stderr, "json parser: escaped hex values not supported\n");
				ch = '?';
				break;
			}
			jfile->charnum++;
		} else if (ch == '"') {
			buffer[i] = '\0';
			token->u.str = strdup(buffer);
			if (!token->u.str) {
				fprintf(stderr, "json parser: out of memory allocating %zd byte string\n", i);
				break;
			}
			return token_string;
		}
		buffer[i] = ch;
		i++;
		if (i >= sizeof(buffer)) {
			fprintf(stderr, "json parser: string too long, maximum size %zd bytes\n", sizeof(buffer) - 1);
			break;
		}
	}

	token->u.str = NULL;
	return token_error;
}

/*
 *  json_get_int()
 *	parse a simple integer
 */
json_token_type json_get_int(json_file *jfile, json_token *token)
{
	char buffer[64];
	size_t i = 0;

	for (;;) {
		int ch;

		ch = fgetc(jfile->fp);
		if (!isdigit(ch)) {
			(void)ungetc(ch, jfile->fp);
			buffer[i] = '\0';
			token->u.intval = atoi(buffer);
			return token_int;
		}
		jfile->charnum++;
		buffer[i] = ch;
		i++;
		if (i >= sizeof(buffer)) {
			fprintf(stderr, "json parser: integer too long, maximum size %zd bytes\n", sizeof(buffer) - 1);
			break;
		}
	}

	token->u.str = NULL;
	return token_error;
}

/*
 *  json_unget_token()
 *  	push file pointer back to point before the token
 *	to unpush the token
 */
int json_unget_token(json_file *jfile, json_token *token)
{
	return fseek(jfile->fp, token->offset, SEEK_SET);
}

/*
 *  json_get_keyword()
 *	get a keyword
 */
int json_get_keyword(json_file *jfile, json_token *token)
{
	char buffer[32];
	size_t i = 0;

	token->u.str = NULL;
	*buffer = '\0';

	for (;;) {
		int ch;

		ch = fgetc(jfile->fp);
		jfile->charnum++;
		if (ch == EOF) {
			fprintf(stderr, "json_parser: unexpected EOF in keyword string\n");
			return token_error;
		}
		if (ch < 'a' || ch > 'z')
			break;

		buffer[i] = ch;
		i++;
		if (i >= sizeof(buffer)) {
			fprintf(stderr, "json parser: keyword too long, maximum size %zd bytes\n", sizeof(buffer) - 1);
			return token_error;
		}
	}
	if (!strcmp(buffer, "true"))
		return token_true;
	if (!strcmp(buffer, "false"))
		return token_false;
	if (!strcmp(buffer, "null"))
		return token_null;
	return token_error;
}

/*
 *  json_free_token()
 *	free any heap allocated members in token when required
 */
void json_free_token(json_token *token)
{
	if (token->type == token_string) {
		free(token->u.str);
		token->u.str = NULL;
	}
}

/*
 *  json_get_token()
 *	read next input character(s) and return a matching token
 */
json_token_type json_get_token(json_file *jfile, json_token *token)
{
	(void)memset(token, 0, sizeof(*token));

	token->offset = ftell(jfile->fp);
	for (;;) {
		int ch;

		ch = fgetc(jfile->fp);
		jfile->charnum++;

		switch (ch) {
		case '\n':
			jfile->linenum++;
			continue;
		case ' ':
		case '\r':
		case '\t':
			continue;
		case EOF:
			token->type = token_eof;
			return token->type;
		case '{':
			token->type = token_lbrace;
			return token->type;
		case '}':
			token->type = token_rbrace;
			return token->type;
		case '[':
			token->type = token_lbracket;
			return token->type;
		case ']':
			token->type = token_rbracket;
			return token->type;
		case ':':
			token->type = token_colon;
			return token->type;
		case ',':
			token->type = token_comma;
			return token->type;
		case '"':
			token->type = json_get_string(jfile, token);
			return token->type;
		case '0'...'9':
			token->type = json_get_int(jfile, token);
			return token->type;
		case 'a'...'z':
			fprintf(stderr, "json_parser: keywords not supported\n");
			token->type = token_error;
			return token->type;
		default:
			token->type = token_error;
			return token->type;
		}
	}

	/* Should never reach here */
	token->type = token_error;
	return token->type;
}

/*
 *  json_parse_error_where()
 *	very simple parser error message, report where in the file
 *	the parsing error occurred.
 */
void json_parse_error_where(json_file *jfile)
{
	if (jfile->error_reported == 0)
		fprintf(stderr, "json_parser: aborted at line %d, char %d of file %s\n",
			jfile->linenum, jfile->charnum, jfile->filename);
	jfile->error_reported++;
}

json_object *json_parse_object(json_file *jfile);

/*
 *  json_parse_array()
 *	parse a json array
 */
json_object *json_parse_array(json_file *jfile)
{
	json_object *array_obj;

	array_obj = json_object_new_array();
	if (!array_obj) {
		fprintf(stderr, "json_parser: out of memory allocating a json array object\n");
		json_parse_error_where(jfile);
		return NULL;
	}

	for (;;) {
		json_object *obj;
		json_token token;

		obj = json_parse_object(jfile);
		if (!obj) {
			json_parse_error_where(jfile);
			json_object_put(array_obj);
			return NULL;
		}
		if (json_object_array_add(array_obj, obj) < 0) {
			json_object_put(array_obj);
			json_object_put(obj);
			return NULL;
		}

		switch (json_get_token(jfile, &token)) {
		case token_rbracket:
			return array_obj;
		case token_comma:
			continue;
		default:
			if (json_unget_token(jfile, &token) != 0) {
				fprintf(stderr, "json_parser: failed to unget a token\n");
				json_object_put(array_obj);
				json_free_token(&token);
				return NULL;
			}
			json_free_token(&token);
			break;
		}
	}
	return array_obj;
}

/*
 *  json_parse_object()
 *	parse a json object (simplified fwts json format only)
 */
json_object *json_parse_object(json_file *jfile)
{
	json_token token;
	json_object *obj, *val_obj;

	if (json_get_token(jfile, &token) != token_lbrace) {
		fprintf(stderr, "json_parser: expecting '{', got %s instead\n", json_token_string(&token));
		json_free_token(&token);
		return NULL;
	}

	obj = json_object_new_object();
	if (!obj)
		goto err_nomem;

	for (;;) {
		char *key = NULL;

		switch (json_get_token(jfile, &token)) {
		case token_rbrace:
			json_free_token(&token);
			return obj;
		case token_string:
			key = strdup(token.u.str);
			if (!key)
				goto err_nomem;
			json_free_token(&token);
			break;
		default:
			fprintf(stderr, "json_parser: expecting } or key literal string, got %s instead\n", json_token_string(&token));
			goto err_free;
		}

		json_free_token(&token);
		if (json_get_token(jfile, &token) != token_colon) {
			fprintf(stderr, "json_parser: expecting ':', got %s instead\n", json_token_string(&token));
			free(key);
			goto err_free;
		}
		switch (json_get_token(jfile, &token)) {
		case token_string:
			val_obj = json_object_new_string(token.u.str);
			if (!val_obj) {
				free(key);
				goto err_nomem;
			}
			json_object_object_add(obj, key, val_obj);
			break;
		case token_int:
			val_obj = json_object_new_int(token.u.intval);
			if (!val_obj) {
				free(key);
				goto err_nomem;
			}
			json_object_object_add(obj, key, val_obj);
			break;
		case token_lbracket:
			val_obj = json_parse_array(jfile);
			if (!val_obj) {
				free(key);
				goto err_nomem;
			}
			json_object_object_add(obj, key, val_obj);
			break;
		case token_lbrace:
			fprintf(stderr, "json_parser: nested objects not supported\n");
			free(key);
			goto err_free;
		case token_true:
		case token_false:
		case token_null:
			fprintf(stderr, "json_parser: tokens %s not supported\n", json_token_string(&token));
			free(key);
			goto err_free;
		default:
			fprintf(stderr, "json_parser: unexpected token %s\n", json_token_string(&token));
		}
		free(key);

		json_free_token(&token);
		switch (json_get_token(jfile, &token)) {
		case token_comma:
			json_free_token(&token);
			continue;
		case token_rbrace:
			json_free_token(&token);
			return obj;
		default:
			fprintf(stderr, "json_parser: expected , or }, got %s instead\n", json_token_string(&token));
			goto err_free;
		}
	}

err_nomem:
	fprintf(stderr, "json_parser: out of memory allocating a json object\n");
	json_parse_error_where(jfile);
err_free:
	free(obj);
	json_free_token(&token);
	return NULL;
}

/*
 *  json_object_from_file()
 *	parse a simplified fwts json file and convert it into
 *	a json object, return NULL if parsing failed or ran
 *	out of memory
 */
json_object *json_object_from_file(const char *filename)
{
	json_object *obj;
	json_file jfile;

	jfile.filename = filename;
	jfile.linenum = 1;
	jfile.charnum = 0;
	jfile.error_reported = 0;

	jfile.fp = fopen(filename, "r");
	if (!jfile.fp)
		return NULL;

	obj = json_parse_object(&jfile);

	fclose(jfile.fp);
	return obj;
}

/*
 *  json_object_new_object()
 *	return a new json object, NULL if failed
 */
json_object *json_object_new_object(void)
{
	json_object *obj;

	obj = calloc(1, sizeof(*obj));
	if (!obj)
		return NULL;
	obj->type = type_object;
	obj->u.ptr = NULL;

	return obj;
}

/*
 *  json_object_new_object()
 *	return a new json object, NULL if failed
 */
int json_object_array_length(json_object *obj)
{
	if (!obj)
		return 0;
	if (obj->type != type_array)
		return 0;
	return obj->length;
}

/*
 *  json_object_new_int()
 *	return a new json integer object, NULL if failed
 */
json_object *json_object_new_int(int val)
{
	json_object *obj;

	obj = calloc(1, sizeof(*obj));
	if (!obj)
		return NULL;
	obj->type = type_int;
	obj->u.intval = val;

	return obj;
}

/*
 *  json_object_new_string()
 *	return a new json string object, NULL if failed
 */
json_object *json_object_new_string(const char *str)
{
	json_object *obj;

	obj = calloc(1, sizeof(*obj));
	if (!obj)
		return NULL;
	obj->type = type_string;
	obj->u.ptr = strdup(str);
	if (!obj->u.ptr) {
		free(obj);
		return NULL;
	}
	return obj;
}

/*
 *  json_object_new_array()
 *	return a new json array object, NULL if failed
 */
json_object *json_object_new_array(void)
{
	json_object *obj;

	obj = calloc(1, sizeof(*obj));
	if (!obj)
		return NULL;
	obj->type = type_array;
	obj->length = 0;
	obj->u.ptr = NULL;

	return obj;
}

/*
 *  json_object_array_add_item()
 *	add an object to another object, return 0 if succeeded,
 *	non-zero if failed
 */
static int json_object_array_add_item(json_object *obj, json_object *item)
{
	json_object **obj_ptr;

	if (obj->length < 0)
		return -1;
	obj_ptr = realloc(obj->u.ptr, sizeof(json_object *) * (obj->length + 1));
	if (!obj_ptr)
		return -1;
	obj->u.ptr = (void *)obj_ptr;
	obj_ptr[obj->length] = item;
	obj->length++;
	return 0;
}

/*
 *  json_object_array_add()
 *	add a object to a json array object, return NULL if failed
 */
int json_object_array_add(json_object *obj, json_object *item)
{
	if (!obj || !item)
		return -1;
	if (obj->type != type_array)
		return -1;
	return json_object_array_add_item(obj, item);
}


/*
 *  json_object_object_add()
 *	add a key/valyue object to a json object, return NULL if failed
 */
void json_object_object_add(json_object *obj, const char *key, json_object *value)
{
	if (!obj || !key || !value)
		return;
	if (obj->type != type_object)
		return;
	value->key = strdup(key);
	if (!value->key)
		return;
	json_object_array_add_item(obj, value);
}

/*
 *  json_object_array_get_idx()
 *	get an object at position index from a json array object,
 *	return NULL if failed
 */
json_object *json_object_array_get_idx(json_object *obj, int index)
{
	json_object **obj_array;

	if (!obj)
		return NULL;
	if (obj->type != type_array)
		return NULL;
	if (index >= obj->length)
		return NULL;
	obj_array = (json_object **)obj->u.ptr;
	if (obj_array == NULL)
		return NULL;
	return obj_array[index];
}

/*
 *  json_object_get_string()
 *	return a C string from a json string object
 */
const char *json_object_get_string(json_object *obj)
{
	if (!obj)
		return NULL;
	if (obj->type != type_string)
		return NULL;
	return (const char *)obj->u.ptr;
}

/*
 *  json_object_put()
 *	free a json object and all sub-objects
 */
void json_object_put(json_object *obj)
{
	int i;
	json_object **obj_ptr;

	if (!obj)
		return;

	if (obj->key)
		free(obj->key);

	switch (obj->type) {
	case type_array:
	case type_object:
		obj_ptr = (json_object **)obj->u.ptr;

		for (i = 0; i < obj->length; i++) {
			json_object_put(obj_ptr[i]);
		}
		free(obj->u.ptr);
		break;
	case type_string:
		free(obj->u.ptr);
		break;
	case type_null:
	case type_int:
	default:
		break;
	}
	free(obj);
}

/*
 *  str_append()
 *	append a string to a string, return NULL if failed
 */
static char *str_append(char *str, char *append)
{
	char *new_str;
	size_t len;

	if (!append)
		return NULL;

	if (str) {
		len = strlen(append) + strlen(str) + 1;
		new_str = realloc(str, len);
		if (!new_str) {
			free(str);
			return NULL;
		}
		strcat(new_str, append);
	} else {
		len = strlen(append) + 1;
		new_str = malloc(len);
		if (!new_str)
			return NULL;
		strcpy(new_str, append);
	}
	return new_str;
}

/*
 *  str_indent()
 *	add 2 spaces per indent level to a string, returns
 *	NULL if failed
 */
static char *str_indent(char *str, int indent)
{
	char buf[81];
	int i;

	indent = indent + indent;
	if (indent > 80)
		indent = 80;

	for (i = 0; i < indent; i++)
		buf[i] = ' ';
	buf[i] = '\0';

	return str_append(str, buf);
}

/*
 *  char_escape()
 *	convert escape char to the unescaped char to
 *	be prefixed by \\.  If not an escape char, return 0
 */
static inline int char_escape(const int ch)
{
	switch (ch) {
	case '"':
		return '"';
	case '\b':
		return 'b';
	case '\f':
		return 'f';
	case '\n':
		return 'n';
	case '\r':
		return 'r';
	case '\t':
		return 't';
	default:
		break;
	}
	return 0;
}

static char *str_escape(char *oldstr)
{
	char *oldptr, *newstr, *newptr;
	ssize_t n;

	for (n = 0, oldptr = oldstr; *oldptr; oldptr++, n++)
		if (char_escape(*oldptr))
			n++;

	newstr = malloc(n + 1);
	if (!newstr)
		return NULL;

	for (oldptr = oldstr, newptr = newstr; *oldptr; oldptr++) {
		const int esc = char_escape(*oldptr);

		if (esc) {
			*(newptr++) = '\\';
			*(newptr++) = esc;
		} else {
			*(newptr++) = *oldptr;
		}
	}
	*newptr = '\0';
	return newstr;
}

/*
 *  json_object_to_json_string_indent()
 *	turn a simplified fwts json object into a C string, returns
 *	the stringified object or NULL if failed. Will traverse object
 *	tree and add indentation based on recursion depth.
 */
static char *json_object_to_json_string_indent(json_object *obj, int indent)
{
	int i;
	json_object **obj_ptr;
	char *str = NULL;
	char *tmp;
	char buf[64];

	if (!obj)
		return NULL;

	if (obj->type == type_object) {
		str = str_indent(str, indent);
		if (!str)
			return NULL;
	}

        if (obj->key) {
		str = str_indent(str, indent);
		if (!str)
			return NULL;
		str = str_append(str, "\"");
		if (!str)
			return NULL;
		str = str_append(str, obj->key);
		if (!str)
			return NULL;
		str = str_append(str, "\":");
		if (!str)
			return NULL;
        }

	switch (obj->type) {
	case type_array:
		str = str_append(str, "\n");
		if (!str)
			return NULL;
		str = str_indent(str, indent);
		if (!str)
			return NULL;
		str = str_append(str, "[");
		if (!str)
			return NULL;

		obj_ptr = (json_object **)obj->u.ptr;
		if (obj_ptr) {
			for (i = 0; i < obj->length; i++) {
				char *obj_str;

				if (i) {
					str = str_append(str, "\n");
					if (!str)
						return NULL;
					str = str_indent(str, indent + 1);
					if (!str)
						return NULL;
					str = str_append(str, ",");
					if (!str)
						return NULL;
				}
				obj_str = json_object_to_json_string_indent(obj_ptr[i], indent + 1);
				if (!obj_str) {
					free(str);
					return NULL;
				}
				str = str_append(str, obj_str);
				free(obj_str);
				if (!str)
					return NULL;
			}
		}
		str = str_append(str, "\n");
		if (!str)
			return NULL;
		str = str_indent(str, indent);
		if (!str)
			return NULL;
		str = str_append(str, "]");
		if (!str)
			return NULL;
		break;

	case type_object:
		str = str_append(str, "\n");
		if (!str)
			return NULL;
		str = str_indent(str, indent);
		if (!str)
			return NULL;
		str = str_append(str, "{");
		if (!str)
			return NULL;
		obj_ptr = (json_object **)obj->u.ptr;
		if (obj_ptr) {
			for (i = 0; i < obj->length; i++) {
				char *obj_str;

				str = str_append(str, (i == 0) ? "\n" : ",\n");
				if (!str)
					return NULL;
				obj_str = json_object_to_json_string_indent(obj_ptr[i], indent + 1);
				if (!obj_str) {
					free(str);
					return NULL;
				}
				str = str_append(str, obj_str);
				free(obj_str);
				if (!str)
					return NULL;
			}
		}
		str = str_append(str, "\n");
		if (!str)
			return NULL;
		str = str_indent(str, indent);
		if (!str)
			return NULL;
		str = str_append(str, "}");
		if (!str)
			return NULL;
		break;

	case type_string:
		str = str_append(str, "\"");
		if (!str)
			return NULL;
		tmp = str_escape((char *)obj->u.ptr);
		if (!tmp) {
			free(str);
			return NULL;
		}
		str = str_append(str, tmp);
		free(tmp);
		if (!str)
			return NULL;
		str = str_append(str, "\"");
		if (!str)
			return NULL;
		break;

	case type_null:
		str = str_append(str, "(null)");
		if (!str)
			return NULL;
		break;

	case type_int:
		snprintf(buf, sizeof(buf), "%d", obj->u.intval);
		str = str_append(str, buf);
		if (!str)
			return NULL;
		break;
	default:
		return NULL;
	}

	if (obj->type == type_object) {
		str = str_indent(str, indent);
		if (!str)
			return NULL;
	}
	return str;
}

/*
 *  json_object_to_json_string
 *	convert simplified fwts object to a C string, returns
 *	NULL if failed
 */
char *json_object_to_json_string(json_object *obj)
{
	return json_object_to_json_string_indent(obj, 0);
}

/*
 *  json_object_object_get()
 *	return value from key/value pair from an object, returns
 *	NULL if it can't be found
 */
json_object *json_object_object_get(json_object *obj, const char *key)
{
	int i;
	json_object **obj_ptr;

	if (!obj || !key)
		return NULL;
	if (obj->type != type_object)
		return NULL;

	obj_ptr = (json_object **)obj->u.ptr;
	for (i = 0; i < obj->length; i++) {
		if (obj_ptr[i]->key && !strcmp(obj_ptr[i]->key, key))
			return obj_ptr[i];
	}
	return NULL;
}

