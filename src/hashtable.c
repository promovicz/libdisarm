/*
 * hashtable.c - Hashtable functions
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
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "hashtable.h"
#include "list.h"


static uint32_t
joaat_hash(const void *key, size_t len)
{
	uint8_t *u8key = (uint8_t *)key;
	uint32_t hash = 0;
	size_t i;

	for (i = 0; i < len; i++) {
		hash += u8key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

int
hashtable_init(hashtable_t *ht, size_t size)
{
	ht->size = size;

	ht->elms = (list_t *)malloc(sizeof(list_t)*size);
	if (ht->elms == NULL) {
		errno = ENOMEM;
		return -1;
	}

	for (size_t i = 0; i < size; i++) {
		list_init(&ht->elms[i]);
	}
}

hashtable_elm_t *
hashtable_lookup(hashtable_t *ht, const void *key, size_t len)
{
	uint32_t hash = joaat_hash(key, len);
	list_t *bucket = &ht->elms[hash % ht->size];

	list_elm_t *elm;
	list_foreach(*bucket, elm) {
		hashtable_elm_t *helm = (hashtable_elm_t *)elm;
		if (len == helm->len && !memcmp(key, helm->key, len)) {
			list_elm_remove(elm);
			list_prepend(bucket, elm);
			return helm;
		}
	}

	return NULL;
}

void
hashtable_store(hashtable_t *ht, hashtable_elm_t *helm,
		const void *key, size_t len)
{
	uint32_t hash = joaat_hash(key, len);
	list_t *bucket = &ht->elms[hash % ht->size];

	helm->key = key;
	helm->len = len;

	list_prepend(bucket, (list_elm_t *)helm);
}
