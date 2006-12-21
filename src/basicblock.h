/* basicblock.h */

#ifndef _BASICBLOCK_H
#define _BASICBLOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "hashtable.h"
#include "image.h"
#include "list.h"
#include "types.h"


typedef struct {
	hashtable_elm_t elm;
	arm_addr_t target;
	list_t refs;
} reflist_elm_t;

typedef struct {
	list_elm_t elm;
	arm_addr_t source;
	int cond;
	int link;
} ref_elm_t;

typedef struct {
	hashtable_elm_t elm;
	arm_addr_t addr;
} bb_elm_t;


int basicblock_analysis(hashtable_t *bb_ht, hashtable_t *sym_ht,
			hashtable_t *reflist_ht, image_t *image,
			uint8_t *codemap);


#endif /* ! _BASICBLOCK_H */
