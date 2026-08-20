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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fwts.h"

/*
 *  fwts_list_iterate()
 *	iterate over items in list, call callback function to operate on each
 *	item, and pass private data over to callback. private may be NULL if not used
 */
void fwts_list_iterate(fwts_list *list, fwts_list_foreach_callback callback, void *private)
{
	fwts_list_link *item;

	if (!list)
		return;

	fwts_list_foreach(item, list)
		callback(item->data, private);
}

/*
 *  fwts_list_free_items()
 *	free items on list but not list head. provide free() func pointer data_free() to
 *	free individual items in list. If func is null, don't
 * 	free items.
 */
void fwts_list_free_items(fwts_list *list, fwts_list_link_free data_free)
{
	fwts_list_link *item;
	fwts_list_link *next;

	if (!list)
		return;

	for (item = list->head; item; item = next) {
		next = item->next;
		if (item->data && data_free)
			data_free(item->data);
		free(item);
	}
}
/*
 *  fwts_list_free()
 *	free list. provide free() func pointer data_free() to
 *	free individual items in list. If func is null, don't
 * 	free items.
 */
void fwts_list_free(fwts_list *list, fwts_list_link_free data_free)
{
	if (list) {
		fwts_list_free_items(list, data_free);
		free(list);
	}
}

/*
 *  fwts_list_append()
 *	add new data to end of list
 */
fwts_list_link *fwts_list_append(fwts_list *list, void *data)
{
	fwts_list_link *link;

	if (!list)
		return NULL;

	if ((link = calloc(1, sizeof(fwts_list_link))) == NULL)
		return NULL;

	link->data = data;

	if (list->head)
		list->tail->next = link;
	else
		list->head = link;

	list->tail = link;
	list->len++;

	return link;
}

/*
 *  fwts_list_add_ordered()
 *	add new data into list, based on order from callback func compare().
 */
fwts_list_link *fwts_list_add_ordered(fwts_list *list, void *new_data, fwts_list_compare compare)
{
	fwts_list_link   *new_list_item;
	fwts_list_link   **list_item;

	if ((new_list_item = calloc(1, sizeof(fwts_list_link))) == NULL)
		return NULL;

	new_list_item->data = new_data;

	for (list_item = &list->head; *list_item; list_item = &(*list_item)->next) {
		void *data = (void *)(*list_item)->data;

		if (compare(data, new_data) >= 0) {
			new_list_item->next = (*list_item);
			break;
		}
	}
	if (!new_list_item->next)
		list->tail = new_list_item;

	*list_item = new_list_item;
	list->len++;

	return new_list_item;
}
