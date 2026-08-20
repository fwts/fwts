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
#ifndef __FWTS_JSON_H__
#define __FWTS_JSON_H__

/*
 *  Minimal subset of json for fwts
 */

#define FWTS_JSON_ERROR(ptr) 	(!ptr)

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE  1
#endif

/*
 *  json types supported
 */
typedef enum {
	type_null,
	type_int,
	type_string,
	type_object,
	type_array,
} json_type;

/*
 *  json object information
 */
typedef struct json_object {
	char *key;		/* Null if undefined */
	int length;		/* Length of a collection of objects */
	json_type type;		/* Object type */
        union {
                void *ptr;	/* string or object array pointer */
                int  intval;	/* integer value */
        } u;
} json_object;

/*
 *  minimal json c library functions as required by fwts
 */
json_object *json_object_from_file(const char *filename);
json_object *json_object_object_get(json_object *obj, const char *key);
int json_object_array_length(json_object *obj);
json_object *json_object_array_get_idx(json_object *obj, int index);
const char *json_object_get_string(json_object *obj);
json_object *json_object_new_int(int);
void json_object_object_add(json_object *obj, const char *key, json_object *value);

json_object *json_object_new_object(void);
json_object *json_object_new_array(void);
char *json_object_to_json_string(json_object *obj);
void json_object_put(json_object *obj);
json_object *json_object_new_string(const char *str);
int json_object_array_add(json_object *obj, json_object *item);

#endif
