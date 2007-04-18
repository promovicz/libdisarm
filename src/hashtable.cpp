/*
 * hashtable.cpp - Hashtable functions
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

#include "hashtable.hh"
#include "slist.hh"

#define REHASH_MAX_THRES  0.75
#define REHASH_MIN_THRES  0.75
#define TABLE_MIN_SIZE   23

#define TABLE_SIZE_INC(size)  (((size) << 1) + 55)
#define TABLE_SIZE_DEC(size)  (((size) - 55) >> 1)


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

static hashtable_elm_t *
bucket_remove_elm(slist_t *bucket, const void *key, size_t len)
{
	slist_elm_t *elm, *preelm;
	slist_foreach(bucket, elm, preelm) {
		hashtable_elm_t *helm = (hashtable_elm_t *)elm;
		if (len == helm->len && !memcmp(key, helm->key, len)) {
			slist_elm_remove(elm, preelm);
			return helm;
		}
	}

	return NULL;
}

static void
bucket_insert_elm(slist_t *bucket, hashtable_elm_t *helm)
{
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
			slist_t *bucket =
				lookup_bucket(elms, size,
					      helm->key, helm->len);
			bucket_insert_elm(bucket, helm);
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
	ht->size = (size > TABLE_MIN_SIZE) ? size : TABLE_MIN_SIZE;
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
	hashtable_elm_t *helm = bucket_remove_elm(bucket, key, len);
	if (helm == NULL) return NULL;

	bucket_insert_elm(bucket, helm);
	return helm;
}

int
hashtable_insert(hashtable_t *ht, hashtable_elm_t *helm,
		 const void *key, size_t len, hashtable_elm_t **old_helm)
{
	slist_t *bucket = lookup_bucket(ht->elms, ht->size, key, len);
	hashtable_elm_t *old = bucket_remove_elm(bucket, key, len);
	if (old_helm != NULL) *old_helm = old;

	helm->key = key;
	helm->len = len;
	bucket_insert_elm(bucket, helm);

	ht->load += 1;
	if (((float)ht->load / (float)ht->size) >= REHASH_MAX_THRES) {
		return hashtable_rehash(ht, TABLE_SIZE_INC(ht->size));
	}

	return 0;
}

int
hashtable_remove(hashtable_t *ht, hashtable_elm_t *helm)
{
	slist_t *bucket = lookup_bucket(ht->elms, ht->size,
					helm->key, helm->len);
	helm = bucket_remove_elm(bucket, helm->key, helm->len);
	if (helm == NULL) return 0;

	ht->load -= 1;
	if (((float)ht->load / (float)ht->size) < REHASH_MIN_THRES &&
	    TABLE_SIZE_DEC(ht->size) > TABLE_MIN_SIZE) {
		return hashtable_rehash(ht, TABLE_SIZE_DEC(ht->size));
	}

	return 0;
}
