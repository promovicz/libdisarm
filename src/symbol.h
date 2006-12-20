/* symbol.h */

#ifndef _SYMBOL_H
#define _SYMBOL_H

#include <stdio.h>
#include <stdlib.h>

#include "hashtable.h"
#include "types.h"


typedef struct {
	hashtable_elm_t elm;
	arm_addr_t addr;
	char *name;
} sym_elm_t;


void symbol_add(hashtable_t *sym_ht, arm_addr_t addr, const char *name,
		int overwrite);
int symbol_add_from_file(hashtable_t *sym_ht, FILE *f);


#endif /* ! _SYMBOL_H */
