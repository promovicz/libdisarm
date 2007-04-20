/* basicblock.cpp */

#include <cstdio>
#include <cstdlib>

#include <list>
#include <map>

#include "basicblock.hh"
#include "arm.hh"
#include "symbol.hh"
#include "types.hh"

using namespace std;


static void
reference_add(map<arm_addr_t, list<ref_t *> *> *ref_list_map,
	      map<arm_addr_t, char *> *sym_map,
	      arm_addr_t source, arm_addr_t target, bool cond, bool link)
{
	int r;

	ref_t *ref = new ref_t;
	if (ref == NULL) abort();

	ref->source = source;
	ref->cond = cond;
	ref->link = link;

	map<arm_addr_t, list<ref_t *> *>::iterator ref_list_pos =
		ref_list_map->find(target);
	list<ref_t *> *ref_list;
	if (ref_list_pos == ref_list_map->end()) {
		ref_list = new list<ref_t *>;
		if (ref_list == NULL) abort();

		(*ref_list_map)[target] = ref_list;
	} else {
		ref_list = (*ref_list_pos).second;
	}

	ref_list->push_back(ref);

	if (link) {
		char *funname = NULL;
		static const char *format = "fun_%x";

		r = snprintf(NULL, 0, format, target);
		if (r > 0) {
			funname = new char[r+1];
			if (funname == NULL) abort();
			r = snprintf(funname, r+1, format, target);
		}

		if (r <= 0) {
			if (funname != NULL) delete funname;
			return;
		}

		symbol_add(sym_map, target, funname, false);
		delete funname;
	}
}

static void
basicblock_add(map<arm_addr_t, bool> *bb_map, arm_addr_t addr)
{
	(*bb_map)[addr] = true;
}

int
basicblock_initial_analysis(map<arm_addr_t, bool> *bb_map,
			    list<arm_addr_t> *entrypoints,
			    map<arm_addr_t, char *> *sym_map,
			    map<arm_addr_t, list<ref_t *> *> *reflist_map,
			    image_t *image)
{
	int r;

	/* add entrypoints */
	list<arm_addr_t>::iterator entrypoint_iter;
	for (entrypoint_iter = entrypoints->begin();
	     entrypoint_iter != entrypoints->end(); entrypoint_iter++) {
		basicblock_add(bb_map, (*entrypoint_iter));
	}

	uint_t i = 0;
	while (1) {
		arm_instr_t instr;
		r = image_read_instr(image, i, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) {
			i += sizeof(arm_instr_t);
			continue;
		}

		if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int link = arm_instr_get_param(instr, ip, 1);
			int offset = arm_instr_get_param(instr, ip, 2);
			int target = arm_instr_branch_target(offset, i);


			/* basic block for fall-through */
			basicblock_add(bb_map, i + sizeof(arm_instr_t));

			if (link || cond != ARM_COND_AL) {
				/* fall-through reference */
				reference_add(reflist_map, sym_map, i,
					      i + sizeof(arm_instr_t),
					      false, false);
			}

			/* basic block for branch target */
			basicblock_add(bb_map, target);

			/* target reference */
			reference_add(reflist_map, sym_map, i, target,
				      (cond != ARM_COND_AL), link);
		} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_IMM) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int rd = arm_instr_get_param(instr, ip, 4);
			if (rd == 15) {
				basicblock_add(bb_map,
					       i + sizeof(arm_instr_t));

				if (cond != ARM_COND_AL) {
					/* fall-through reference */
					reference_add(reflist_map, sym_map, i,
						      i + sizeof(arm_instr_t),
						      true, false);
				}
			}
		} else if (ip->type == ARM_INSTR_TYPE_LS_REG_OFF ||
			   ip->type == ARM_INSTR_TYPE_LS_IMM_OFF) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int rd = arm_instr_get_param(instr, ip, 7);
			if (rd == 15) {
				basicblock_add(bb_map,
					       i + sizeof(arm_instr_t));

				if (cond != ARM_COND_AL) {
					/* fall-through reference */
					reference_add(reflist_map, sym_map, i,
						      i + sizeof(arm_instr_t),
						      true, false);
				}
			}
		} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int load = arm_instr_get_param(instr, ip, 5);
			int reglist = arm_instr_get_param(instr, ip, 7);
			if (load && (reglist & (1 << 15))) {
				basicblock_add(bb_map,
					       i + sizeof(arm_instr_t));

				if (cond != ARM_COND_AL) {
					/* fall-through reference */
					reference_add(reflist_map, sym_map, i,
						      i + sizeof(arm_instr_t),
						      true, false);
				}
			}
		}

		i += sizeof(arm_instr_t);
	}

	return 0;
}
