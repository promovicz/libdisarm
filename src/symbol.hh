/* symbol.hh */

#ifndef _SYMBOL_HH
#define _SYMBOL_HH

#include <stdio.h>
#include <stdlib.h>

#include "hashtable.hh"
#include "types.hh"


typedef struct {
	hashtable_elm_t elm;
	arm_addr_t addr;
	char *name;
} sym_elm_t;


int symbol_add(hashtable_t *sym_ht, arm_addr_t addr, const char *name,
	       int overwrite);
int symbol_add_from_file(hashtable_t *sym_ht, FILE *f);


#endif /* ! _SYMBOL_HH */
