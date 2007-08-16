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


template <typename ref_t> static void
reference_add(map<arm_addr_t, list<ref_t *> *> *refs_map,
	      arm_addr_t key, ref_t *ref)
{
	typename map<arm_addr_t, list<ref_t *> *>::iterator refs_pos =
		refs_map->find(key);
	list<ref_t *> *ref_list;
	if (refs_pos == refs_map->end()) {
		ref_list = new list<ref_t *>;
		if (ref_list == NULL) abort();

		(*refs_map)[key] = ref_list;
	} else {
		ref_list = (*refs_pos).second;
	}

	ref_list->push_back(ref);
}

static void
reference_code_add(map<arm_addr_t, list<ref_code_t *> *> *coderefs_map,
		   map<arm_addr_t, char *> *sym_map,
		   arm_addr_t source, arm_addr_t target,
		   bool cond, bool link)
{
	int r;

	ref_code_t *ref = new ref_code_t;
	if (ref == NULL) abort();

	ref->source = source;
	ref->target = target;
	ref->cond = cond;
	ref->link = link;

	reference_add<ref_code_t>(coderefs_map, target, ref);

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
reference_data_add(map<arm_addr_t, list<ref_data_t *> *> *datarefs_map,
		   arm_addr_t source, arm_addr_t target)
{
	ref_data_t *ref = new ref_data_t;
	if (ref == NULL) abort();

	ref->source = source;
	ref->target = target;

	reference_add<ref_data_t>(datarefs_map, target, ref);
}

int
basicblock_find(map<arm_addr_t, bool> *bb_map, arm_addr_t addr,
		arm_addr_t *bb_addr, uint_t *size)
{
	map<arm_addr_t, bool>::iterator iter = bb_map->upper_bound(addr);
	arm_addr_t bb_end = iter->first;

	iter--;

	if (bb_addr != NULL) *bb_addr = iter->first;
	if (size != NULL) *size = bb_end - iter->first;

	return 0;
}

bool
basicblock_is_addr_entry(map<arm_addr_t, bool> *bb_map, arm_addr_t addr)
{
	map<arm_addr_t, bool>::iterator iter = bb_map->find(addr);
	return (iter != bb_map->end());
}

static void
basicblock_add(map<arm_addr_t, bool> *bb_map, arm_addr_t addr)
{
	(*bb_map)[addr] = true;
}

int
bb_instr_analysis(map<arm_addr_t, bool> *bb_map,
		  list<arm_addr_t> *entrypoints,
		  map<arm_addr_t, char *> *sym_map,
		  map<arm_addr_t, list<ref_code_t *> *> *coderefs_map,
		  map<arm_addr_t, list<ref_data_t *> *> *datarefs_map,
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
		r = image_read_word(image, i, &instr);
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
				reference_code_add(coderefs_map, sym_map, i,
						   i + sizeof(arm_instr_t),
						   (cond != ARM_COND_AL),
						   false);
			}

			/* basic block for branch target */
			basicblock_add(bb_map, target);

			/* target reference */
			reference_code_add(coderefs_map, sym_map, i, target,
					   (cond != ARM_COND_AL), link);
		} else {	
			arm_cond_t cond;
			r = arm_instr_get_cond(instr, &cond);
			if (r < 0) cond = ARM_COND_AL;

			r = arm_instr_is_reg_changed(instr, 15);
			if (r < 0) return -1;
			else if (r) {
				basicblock_add(bb_map,
					       i + sizeof(arm_instr_t));


				if (cond != ARM_COND_AL) {
					/* fall-through reference */
					reference_code_add(coderefs_map,
							   sym_map, i,
							   i +
							   sizeof(arm_instr_t),
							   true, false);
				}
			}

			if (ip->type == ARM_INSTR_TYPE_LS_IMM_OFF) {
				int u = arm_instr_get_param(instr, ip, 2);
				int rn = arm_instr_get_param(instr, ip, 6);
				int imm = arm_instr_get_param(instr, ip, 8);

				if (rn == 15 && cond != ARM_COND_NV) {
					arm_addr_t target = i + 8 +
						(imm * (u ? 1 : -1));
					reference_data_add(datarefs_map, i,
							   target);
				}
			}
		}

		i += sizeof(arm_instr_t);
	}

	return 0;
}
