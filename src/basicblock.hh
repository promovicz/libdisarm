/* basicblock.hh */

#ifndef _BASICBLOCK_HH
#define _BASICBLOCK_HH

#include <set>
#include <map>

#include "image.hh"
#include "types.hh"

using namespace std;


typedef struct {
	bool remove;
	arm_addr_t source;
	arm_addr_t target;
	bool cond;
	bool link;
} ref_code_t;	

typedef struct {
	bool remove;
	arm_addr_t source;
	arm_addr_t target;
	uint_t size;
} ref_data_t;


typedef struct basic_block basic_block_t;

typedef struct {
	bool operator()(const basic_block_t *bb1,
			const basic_block_t *bb2) const;
} bb_cmp_t;

typedef struct {
	map<basic_block_t *, ref_code_t *, bb_cmp_t> in_refs;
	map<basic_block_t *, ref_code_t *, bb_cmp_t> out_refs;
	multimap<basic_block_t *, ref_data_t *, bb_cmp_t> data_refs;
} bb_code_t;

typedef struct {
	multimap<basic_block_t *, ref_data_t *, bb_cmp_t> data_refs;
} bb_data_t;

typedef enum {
	BASIC_BLOCK_TYPE_CODE,
	BASIC_BLOCK_TYPE_DATA,
	BASIC_BLOCK_TYPE_UNKNOWN,
	BASIC_BLOCK_TYPE_MAX
} basic_block_type_t;

struct basic_block {
	arm_addr_t addr;
	uint_t size;
	basic_block_type_t type;
	bb_code_t c;
	bb_data_t d;
};


int basicblock_analysis(map<arm_addr_t, basic_block_t *> *bb_map,
			set<arm_addr_t> *entrypoints, image_t *image);


#endif /* ! _BASICBLOCK_HH */
