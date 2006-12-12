/*
 * hashtable.h - Hashtable header
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

#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>

#include "list.h"


typedef struct {
	list_elm_t elm;
	const void *key;
	size_t len;
} hashtable_elm_t;

typedef struct {
	size_t size;
	list_t *elms;
} hashtable_t;


int hashtable_init(hashtable_t *ht, size_t size);
hashtable_elm_t *hashtable_lookup(hashtable_t *ht,
				  const void *key, size_t len);
void hashtable_store(hashtable_t *ht, hashtable_elm_t *helm,
		     const void *key, size_t len);


#endif /* ! _HASHTABLE_H */
