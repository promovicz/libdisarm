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
	arm_addr_t target;
	bool cond;
	bool link;
} ref_code_t;	

typedef struct {
	arm_addr_t source;
	arm_addr_t target;
} ref_data_t;


int bb_instr_analysis(map<arm_addr_t, bool> *bb_map,
		      list<arm_addr_t> *entrypoints,
		      map<arm_addr_t, char *> *sym_map,
		      map<arm_addr_t, list<ref_code_t *> *> *coderefs_map,
		      map<arm_addr_t, list<ref_data_t *> *> *datarefs_map,
		      image_t *image);


#endif /* ! _BASICBLOCK_HH */
