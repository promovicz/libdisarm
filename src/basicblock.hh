/* basicblock.hh */

#ifndef _BASICBLOCK_HH
#define _BASICBLOCK_HH

#include <list>
#include <set>
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
	uint_t size;
} ref_data_t;


typedef struct basic_block basic_block_t;

typedef struct {
	map<basic_block_t *, ref_code_t *> in_refs;
	map<basic_block_t *, ref_code_t *> out_refs;
} bb_code_t;

typedef struct {
	map<basic_block_t *, ref_data_t *> data_refs;
} bb_data_t;

struct basic_block {
	arm_addr_t addr;
	uint_t size;
	bool code;
	bb_data_t c;
	bb_data_t d;
};

void basicblock_find(map<arm_addr_t, basic_block_t *> *bb_map, arm_addr_t addr,
		     arm_addr_t *bb_addr, basic_block_t **bb);
bool basicblock_is_addr_entry(map<arm_addr_t, basic_block_t *> *bb_map,
			      arm_addr_t addr);
int basicblock_analysis(map<arm_addr_t, basic_block_t *> *bb_map,
			list<arm_addr_t> *entrypoints,
			map<arm_addr_t, char *> *sym_map,
			map<arm_addr_t, list<ref_code_t *> *> *coderefs_map,
			map<arm_addr_t, list<ref_data_t *> *> *datarefs_map,
			image_t *image);


#endif /* ! _BASICBLOCK_HH */
