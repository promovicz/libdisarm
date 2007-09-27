/* symbol.hh */

#ifndef _SYMBOL_HH
#define _SYMBOL_HH

#include <map>

#include "types.hh"


int symbol_add(std::map<arm_addr_t, char *> *sym_map,
	       arm_addr_t addr, const char *name, bool overwrite);
int symbol_add_from_file(std::map<arm_addr_t, char *> *sym_map, FILE *f);


#endif /* ! _SYMBOL_HH */
