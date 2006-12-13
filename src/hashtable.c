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
#include "slist.h"

#define REHASH_THRESHOLD  0.75


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

static slist_t *
lookup_bucket(slist_t *elms, size_t size, const void *key, size_t len)
{
	uint32_t hash = joaat_hash(key, len);
	return &elms[hash % size];
}

static void
insert_elm(slist_t *elms, size_t size, hashtable_elm_t *helm)
{
	slist_t *bucket = lookup_bucket(elms, size, helm->key, helm->len);
	slist_prepend(bucket, (slist_elm_t *)helm);	
}

int
hashtable_rehash(hashtable_t *ht, size_t size)
{
	slist_t *elms = (slist_t *)calloc(size, sizeof(slist_t));
	if (elms == NULL) {
		errno = ENOMEM;
		return -1;
	}

	for (int i = 0; i < ht->size; i++) {
		slist_t *bucket = &ht->elms[i];
		while (!slist_is_empty(bucket)) {
			hashtable_elm_t *helm =
				(hashtable_elm_t *)slist_remove_head(bucket);
			insert_elm(elms, size, helm);
		}
	}

	free(ht->elms);

	ht->size = size;
	ht->elms = elms;

	return 0;
}

int
hashtable_init(hashtable_t *ht, size_t size)
{
	ht->size = (size > 0) ? size : 23;
	ht->load = 0;

	ht->elms = (slist_t *)calloc(ht->size, sizeof(slist_t));
	if (ht->elms == NULL) {
		errno = ENOMEM;
		return -1;
	}

	return 0;
}

void
hashtable_deinit(hashtable_t *ht)
{
	free(ht->elms);
}

hashtable_elm_t *
hashtable_lookup(hashtable_t *ht, const void *key, size_t len)
{
	slist_t *bucket = lookup_bucket(ht->elms, ht->size, key, len);

	slist_elm_t *elm, *preelm;
	slist_foreach(bucket, elm, preelm) {
		hashtable_elm_t *helm = (hashtable_elm_t *)elm;
		if (len == helm->len && !memcmp(key, helm->key, len)) {
			slist_elm_remove(elm, preelm);
			slist_prepend(bucket, elm);
			return helm;
		}
	}

	return NULL;
}

int
hashtable_insert(hashtable_t *ht, hashtable_elm_t *helm,
		 const void *key, size_t len)
{
	helm->key = key;
	helm->len = len;
	insert_elm(ht->elms, ht->size, helm);

	ht->load += 1;
	if (((float)ht->load / (float)ht->size) >= REHASH_THRESHOLD) {
		return hashtable_rehash(ht, 2 * ht->size + 55);
	}

	return 0;
}

int
hashtable_remove(hashtable_t *ht, hashtable_elm_t *helm)
{
	slist_t *bucket = lookup_bucket(ht->elms, ht->size,
					helm->key, helm->len);

	slist_elm_t *elm, *preelm;
	slist_foreach(bucket, elm, preelm) {
		hashtable_elm_t *loop_helm = (hashtable_elm_t *)elm;
		if (helm->len == loop_helm->len &&
		    !memcmp(helm->key, loop_helm->key, helm->len)) {
			slist_elm_remove(elm, preelm);
			ht->load -= 1;
			return 0;
		}
	}

	return 0;
}
