/*
 * slist.c - Singly-linked list functions
 *
 * Copyright (C) 2006  Jon Lund Steffensen <jonls@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "slist.h"


void
slist_init(slist_t *list)
{
	list->head = NULL;
}

slist_elm_t *
slist_head(slist_t *list)
{
	return list->head;
}

void
slist_prepend(slist_t *list, slist_elm_t *elm)
{
	elm->next = list->head;
	list->head = elm;
}

void
slist_elm_remove(slist_elm_t *elm, slist_elm_t *preelm)
{
	preelm->next = elm->next;
	elm->next = NULL;
}

slist_elm_t *
slist_remove_head(slist_t *list)
{
	if (slist_is_empty(list)) return NULL;
	slist_elm_t *elm = slist_head(list);
	slist_elm_remove(elm, (slist_elm_t *)list);
	return elm;
}

int
slist_is_empty(slist_t *list)
{
	return (list->head == NULL);
}
