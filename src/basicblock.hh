/* basicblock.hh */

#ifndef _BASICBLOCK_HH
#define _BASICBLOCK_HH

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "hashtable.hh"
#include "image.hh"
#include "list.hh"
#include "types.hh"


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


#endif /* ! _BASICBLOCK_HH */
