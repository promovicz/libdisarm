/* basicblock.hh */

#ifndef _BASICBLOCK_HH
#define _BASICBLOCK_HH

#include <list>
#include <map>

#include "image.hh"
#include "types.hh"

using namespace std;


typedef struct {
	arm_addr_t source;
	int cond;
	int link;
} ref_t;


int basicblock_analysis(map<arm_addr_t, bool> *bb_map,
			map<arm_addr_t, char *> *sym_map,
			map<arm_addr_t, list<ref_t *> *> *ref_list_map,
			image_t *image, uint8_t *codemap);


#endif /* ! _BASICBLOCK_HH */
