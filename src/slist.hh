/*
 * slist.hh - Singly-linked list header (C++)
 *
 * Copyright (C) 2007  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef _SLIST_HH
#define _SLIST_HH


#define SLIST_INIT  \
  { NULL }

#define slist_foreach(list,elm,preelm)  \
  for ((preelm) = (slist_elm_t *)list, (elm) = (preelm)->next; \
       (elm) != NULL; \
       (preelm) = (elm), (elm) = (elm)->next)

#define slist_foreach_safe(list,elm,preelm,tmpelm)  \
  for ((preelm) = (slist_elm_t *)list, (elm) = (preelm)->next, \
       (tmpelm) = (((elm) != NULL) ? (elm)->next : NULL); \
       (elm) != NULL; \
       (preelm) = (elm), (elm) = (tmpelm), \
       (tmpelm) = (((elm) != NULL) ? (elm)->next : NULL))


typedef struct slist slist_t;
typedef struct slist_elm slist_elm_t;

/* SList */
struct slist {
	slist_elm_t *head;
};

struct slist_elm {
	slist_elm_t *next;
};

/* SList functions */
void slist_init(slist_t *list);
slist_elm_t *slist_head(slist_t *list);
void slist_prepend(slist_t *list, slist_elm_t *elm);
void slist_elm_remove(slist_elm_t *elm, slist_elm_t *preelm);
slist_elm_t *slist_remove_head(slist_t *list);
int slist_is_empty(slist_t *list);


#endif /* ! _SLIST_HH */
