/* basicblock.cpp */

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <list>
#include <map>
#include <stack>

#include "basicblock.hh"
#include "arm.hh"
#include "symbol.hh"
#include "types.hh"

using namespace std;


bool
bb_cmp_t::operator()(const basic_block_t *bb1,
		     const basic_block_t *bb2) const {
	return (bb1->addr < bb2->addr);
}

static void
mark_bb_data(basic_block_t *bb_source)
{
	stack<basic_block_t *> stack;

	stack.push(bb_source);

	while (!stack.empty()) {
		basic_block_t *bb = stack.top();
		stack.pop();

		bb->code = false;

		/* remove outgoing refs */
		while (!bb->c.out_refs.empty()) {
			map<basic_block_t *, ref_code_t *>::iterator ref_iter;
			ref_iter = bb->c.out_refs.begin();

			ref_code_t *ref = ref_iter->second;
			bb->c.out_refs.erase(ref_iter);

			if (ref->remove) delete ref;
			else ref->remove = true;
		}

		/* remove data refs */
		while (!bb->c.data_refs.empty()) {
			map<basic_block_t *, ref_data_t *>::iterator ref_iter;
			ref_iter = bb->c.data_refs.begin();

			ref_data_t *ref = ref_iter->second;
			bb->c.data_refs.erase(ref_iter);

			if (ref->remove) delete ref;
			else ref->remove = true;
		}

		/* remove incoming refs and add source to stack */
		while (!bb->c.in_refs.empty()) {
			map<basic_block_t *, ref_code_t *>::iterator ref_iter;
			ref_iter = bb->c.in_refs.begin();

			basic_block_t *bb_next = ref_iter->first;
			ref_code_t *ref = ref_iter->second;
			bb->c.in_refs.erase(ref_iter);

			if (ref->remove) delete ref;
			else ref->remove = true;

			stack.push(bb_next);
		}
	}
}

/* return 0 on success, return 1 if source block was changed */
static int
reference_code_add(map<arm_addr_t, basic_block_t *> *bb_map,
		   basic_block_t *bb_source,
		   arm_addr_t source, arm_addr_t target,
		   bool cond, bool link)
{
	int r;

	map<arm_addr_t, basic_block_t *>::iterator bb_iter;
	basic_block_t *bb_target;
	bb_iter = bb_map->find(target);
	bb_target = bb_iter->second;

	if (!bb_target->code) {
		mark_bb_data(bb_source);
		return 1;
	}

	ref_code_t *ref = new ref_code_t;
	if (ref == NULL) abort();

	ref->remove = false;
	ref->source = source;
	ref->target = target;
	ref->cond = cond;
	ref->link = link;

	bb_source->c.out_refs[bb_target] = ref;
	bb_target->c.in_refs[bb_source] = ref;

	return 0;
}

/* return 0 on success, return 1 if source block was changed */
static int
reference_data_add(map<arm_addr_t, basic_block_t *> *bb_map,
		   basic_block_t *bb_source,
		   arm_addr_t source, arm_addr_t target, uint_t size)
{
	map<arm_addr_t, basic_block_t *>::iterator bb_iter;
	basic_block_t *bb_target;
	bb_iter = bb_map->upper_bound(target);
	if (bb_iter == bb_map->end()) return 0;
	bb_iter--;

	bb_target = bb_iter->second;

	if (bb_target == bb_source) return 0;

	ref_data_t *ref = new ref_data_t;
	if (ref == NULL) abort();

	ref->remove = false;
	ref->source = source;
	ref->target = target;
	ref->size = size;

	bb_source->c.data_refs.insert(make_pair(bb_target, ref));
	bb_target->d.data_refs.insert(make_pair(bb_source, ref));

	return 0;
}

void
basicblock_find(map<arm_addr_t, basic_block_t *> *bb_map, arm_addr_t addr,
		arm_addr_t *bb_addr, basic_block_t **bb)
{
	map<arm_addr_t, basic_block_t *>::iterator iter =
		bb_map->upper_bound(addr);
	arm_addr_t bb_end = iter->first;

	iter--;

	if (bb_addr != NULL) *bb_addr = iter->first;
	if (bb != NULL) {
		*bb = iter->second;
		if ((*bb)->size == 0) (*bb)->size = bb_end - iter->first;
	}
}

bool
basicblock_is_addr_entry(map<arm_addr_t, basic_block_t *> *bb_map,
			 arm_addr_t addr)
{
	map<arm_addr_t, basic_block_t *>::iterator iter = bb_map->find(addr);
	return (iter != bb_map->end());
}

static void
basicblock_add(map<arm_addr_t, basic_block_t *> *bb_map, arm_addr_t addr)
{
	basic_block_t *bb = new basic_block_t;
	if (bb == NULL) abort();

	bb->addr = addr;
	bb->code = true;
	bb->size = 0;

	(*bb_map)[addr] = bb;
}

static int
bb_pass_mark(map<arm_addr_t, basic_block_t *> *bb_map, image_t *image)
{
	int r;

	arm_addr_t addr = 0;
	while (1) {
		arm_instr_t instr;
		r = image_read_word(image, addr, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) {
			addr += sizeof(arm_instr_t);
			continue;
		}

		if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
			int offset = arm_instr_get_param(instr, ip, 2);
			int target = arm_instr_branch_target(offset, addr);

			/* basic block for fall-through */
			basicblock_add(bb_map, addr + sizeof(arm_instr_t));

			/* basic block for branch target */
			basicblock_add(bb_map, target);
		} else {	
			r = arm_instr_is_reg_changed(instr, 15);
			if (r < 0) return -1;
			else if (r) {
				basicblock_add(bb_map,
					       addr + sizeof(arm_instr_t));
			}
		}

		addr += sizeof(arm_instr_t);
	}

	return 0;
}

static int
bb_pass_second(map<arm_addr_t, basic_block_t *> *bb_map, image_t *image)
{
	int r;

	map<arm_addr_t, basic_block_t *>::iterator bb_iter = bb_map->begin();
	basic_block_t *bb = bb_iter->second;
	bb_iter++;
	arm_addr_t bb_next = bb_iter->first;

	arm_addr_t addr = 0;
	while (1) {
		arm_instr_t instr;
		r = image_read_word(image, addr, &instr);
		if (r == 0) {
			bb->size = addr - bb->addr;
			break;
		} else if (r < 0) return -1;

		/* basic block size */
		if (addr == bb_next) {
			bb->size = addr - bb->addr;
			bb = bb_iter->second;
			bb_iter++;
			bb_next = bb_iter->first;
		}

		/* code/data analysis */
		bool unpredictable;
		r = arm_instr_is_unpredictable(instr, &unpredictable);
		if (r < 0) return -1;

		if (unpredictable) mark_bb_data(bb);;

		addr += sizeof(arm_instr_t);
	}

	return 0;
}

static int
block_find_references(basic_block_t *bb,
		      map<arm_addr_t, basic_block_t *> *bb_map, image_t *image)
{
	int r;

	arm_addr_t addr = bb->addr;
	while (addr < bb->addr + bb->size) {
		arm_instr_t instr;
		r = image_read_word(image, addr, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) {
			addr += sizeof(arm_instr_t);
			continue;
		}

		if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int link = arm_instr_get_param(instr, ip, 1);
			int offset = arm_instr_get_param(instr, ip, 2);
			int target = arm_instr_branch_target(offset, addr);

			if (link || cond != ARM_COND_AL) {
				/* fall-through reference */
				r = reference_code_add(bb_map, bb, addr,
						       addr +
						       sizeof(arm_instr_t),
						       (cond != ARM_COND_AL),
						       false);
				if (r < 0) return -1;
				else if (r == 1) break;
			}

			/* target reference */
			r = reference_code_add(bb_map, bb, addr, target,
					       (cond != ARM_COND_AL), link);
			if (r < 0) return -1;
			else if (r == 1) break;
		} else {
			arm_cond_t cond;
			r = arm_instr_get_cond(instr, &cond);
			if (r < 0) cond = ARM_COND_AL;

			r = arm_instr_is_reg_changed(instr, 15);
			if (r < 0) return -1;
			else if (r && cond != ARM_COND_AL) {
				/* fall-through reference */
				r = reference_code_add(bb_map, bb, addr,
						       addr +
						       sizeof(arm_instr_t),
						       true, false);
				if (r < 0) return -1;
				else if (r == 1) break;
			}

			if (ip->type == ARM_INSTR_TYPE_LS_IMM_OFF) {
				int u = arm_instr_get_param(instr, ip, 2);
				int rn = arm_instr_get_param(instr, ip, 6);
				int imm = arm_instr_get_param(instr, ip, 8);

				if (rn == 15 && cond != ARM_COND_NV) {
					arm_addr_t target = addr + 8 +
						(imm * (u ? 1 : -1));
					r = reference_data_add(bb_map, bb,
							       addr, target,
							       4);
					if (r < 0) return -1;
					else if (r == 1) break;
				}
			}
		}

		addr += sizeof(arm_instr_t);
	}

	return 0;
}

static int
bb_pass_reference(map<arm_addr_t, basic_block_t *> *bb_map, image_t *image)
{
	int r;

	map<arm_addr_t, basic_block_t *>::iterator bb_iter = bb_map->begin();
	for (bb_iter = bb_map->begin(); bb_iter != bb_map->end(); bb_iter++) {
		basic_block_t *bb = bb_iter->second;

		if (!bb->code) continue;

		r = block_find_references(bb, bb_map, image);
		if (r < 0) return -1;
	}

	return 0;
}

int
basicblock_analysis(map<arm_addr_t, basic_block_t *> *bb_map,
		    list<arm_addr_t> *entrypoints, image_t *image)
{
	int r;

	/* add entrypoints */
	list<arm_addr_t>::iterator entrypoint_iter;
	for (entrypoint_iter = entrypoints->begin();
	     entrypoint_iter != entrypoints->end(); entrypoint_iter++) {
		basicblock_add(bb_map, (*entrypoint_iter));
	}

	/* first pass: */
	/* mark basic blocks */
	r = bb_pass_mark(bb_map, image);
	if (r < 0) return -1;

	/* second pass: */
	/* save basic block size */
	/* do code/data analysis */
	r = bb_pass_second(bb_map, image);
	if (r < 0) return -1;

	/* third pass: */
	/* collect references */
	r = bb_pass_reference(bb_map, image);
	if (r < 0) return -1;

	return 0;
}
