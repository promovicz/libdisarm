/*
 * hashtable.hh - Hashtable header (C++)
 *
 * Copyright (C) 2007  Jon Lund Steffensen <jonls@users.sourceforge.net>
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

#ifndef _HASHTABLE_HH
#define _HASHTABLE_HH

#include "slist.hh"


typedef struct {
	slist_elm_t elm;
	const void *key;
	size_t len;
} hashtable_elm_t;

typedef struct {
	slist_t *elms;
	size_t size;
	size_t load;
} hashtable_t;


int hashtable_init(hashtable_t *ht, size_t size);
void hashtable_deinit(hashtable_t *ht);
int hashtable_rehash(hashtable_t *ht, size_t size);
hashtable_elm_t *hashtable_lookup(hashtable_t *ht,
				  const void *key, size_t len);
int hashtable_insert(hashtable_t *ht, hashtable_elm_t *helm,
		     const void *key, size_t len, hashtable_elm_t **old_elm);
int hashtable_remove(hashtable_t *ht, hashtable_elm_t *helm);


#endif /* ! _HASHTABLE_HH */
