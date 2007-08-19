/* basicblock.cpp */

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <iostream>
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
data_block_merge(basic_block_t *bb, basic_block_t *bb_next)
{
	/* move data refs */
	while (!bb_next->d.data_refs.empty()) {
		map<basic_block_t *, ref_data_t *>::iterator ref_iter;
		ref_iter = bb_next->d.data_refs.begin();

		basic_block_t *bb_source = ref_iter->first;
		ref_data_t *ref = ref_iter->second;
		bb_next->d.data_refs.erase(ref_iter);

		if (ref->remove) {
			delete ref;
			continue;
		}

		bb_source->c.data_refs.erase(bb_next);
		bb_source->c.data_refs.insert(make_pair(bb, ref));
		bb->d.data_refs.insert(make_pair(bb_source, ref));
	}

	/* update size */
	bb->size += bb_next->size;

	/* delete merged block */
	delete bb_next;
}

static void
mark_bb_data(basic_block_t *bb_source)
{
	stack<basic_block_t *> stack;

	stack.push(bb_source);

	while (!stack.empty()) {
		basic_block_t *bb = stack.top();
		stack.pop();

		bb->type = BASIC_BLOCK_TYPE_DATA;

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

			if (bb_next->type == BASIC_BLOCK_TYPE_UNKNOWN) {
				stack.push(bb_next);
			}
		}
	}
}

static void
mark_bb_code(basic_block_t *bb_source)
{
	stack<basic_block_t *> stack;

	stack.push(bb_source);

	while (!stack.empty()) {
		basic_block_t *bb = stack.top();
		stack.pop();

		bb->type = BASIC_BLOCK_TYPE_CODE;

		/* remove incoming data refs */
		while (!bb->d.data_refs.empty()) {
			map<basic_block_t *, ref_data_t *>::iterator ref_iter;
			ref_iter = bb->d.data_refs.begin();

			ref_data_t *ref = ref_iter->second;
			bb->d.data_refs.erase(ref_iter);

			if (ref->remove) delete ref;
			else ref->remove = true;
		}


		/* mark outgoing data refs as data blocks */
		map<basic_block_t *, ref_data_t *>::iterator data_iter;
		for (data_iter = bb->c.data_refs.begin();
		     data_iter != bb->c.data_refs.end(); data_iter++) {
			basic_block_t *bb_ref = data_iter->first;
			if (bb_ref->type == BASIC_BLOCK_TYPE_UNKNOWN) {
				mark_bb_data(bb_ref);
			}
		}

		/* add outgoing blocks to stack */
		map<basic_block_t *, ref_code_t *>::iterator code_iter;
		for (code_iter = bb->c.out_refs.begin();
		     code_iter != bb->c.out_refs.end(); code_iter++) {
			basic_block_t *bb_ref = code_iter->first;
			if (bb_ref->type == BASIC_BLOCK_TYPE_UNKNOWN) {
				stack.push(bb_ref);
			}
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
	if (bb_iter == bb_map->end()) return 0;

	bb_target = bb_iter->second;

	if (bb_target->type == BASIC_BLOCK_TYPE_DATA) {
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

static void
reference_data_add(map<arm_addr_t, basic_block_t *> *bb_map,
		   basic_block_t *bb_source,
		   arm_addr_t source, arm_addr_t target, uint_t size)
{
	map<arm_addr_t, basic_block_t *>::iterator bb_iter;
	basic_block_t *bb_target;
	bb_iter = bb_map->upper_bound(target);
	if (bb_iter == bb_map->end()) return;
	bb_iter--;

	bb_target = bb_iter->second;
	if (bb_target == bb_source) return;

	ref_data_t *ref = new ref_data_t;
	if (ref == NULL) abort();

	ref->remove = false;
	ref->source = source;
	ref->target = target;
	ref->size = size;

	bb_source->c.data_refs.insert(make_pair(bb_target, ref));
	bb_target->d.data_refs.insert(make_pair(bb_source, ref));

	return;
}

static void
basicblock_add(map<arm_addr_t, basic_block_t *> *bb_map, arm_addr_t addr,
	       image_t *image)
{
	if (!image_is_addr_mapped(image, addr)) return;

	basic_block_t *bb = new basic_block_t;
	if (bb == NULL) abort();

	bb->addr = addr;
	bb->type = BASIC_BLOCK_TYPE_UNKNOWN;
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
			int link = arm_instr_get_param(instr, ip, 1);
			int offset = arm_instr_get_param(instr, ip, 2);
			int target = arm_instr_branch_target(offset, addr);

			/* basic block for fall-through */
			if (!link) {
				basicblock_add(bb_map,
					       addr + sizeof(arm_instr_t),
					       image);
			}

			/* basic block for branch target */
			basicblock_add(bb_map, target, image);
		} else {	
			r = arm_instr_is_reg_changed(instr, 15);
			if (r < 0) return -1;
			else if (r) {
				basicblock_add(bb_map,
					       addr + sizeof(arm_instr_t),
					       image);
			}
		}

		addr += sizeof(arm_instr_t);
	}

	return 0;
}

static int
bb_pass_size(map<arm_addr_t, basic_block_t *> *bb_map, image_t *image)
{
	int r;

	map<arm_addr_t, basic_block_t *>::iterator bb_iter = bb_map->begin();
	if (bb_iter != bb_map->end()) {
		basic_block_t *bb_prev = (bb_iter++)->second;
		while (bb_iter != bb_map->end()) {
			basic_block_t *bb = (bb_iter++)->second;
			bb_prev->size = bb->addr - bb_prev->addr;
			bb_prev = bb;
		}

		/* find size of last block */
		arm_addr_t addr = bb_prev->addr;
		while (image_is_addr_mapped(image, addr)) addr += 4;
		bb_prev->size = addr - bb_prev->addr;
	}

	return 0;
}

/* Try to backtrack to last cmp rREG, #X instruction and return X. */
static int
block_backtrack_reg_bounds(basic_block_t *bb, arm_addr_t addr, uint_t reg,
			   image_t *image)
{
	assert(addr >= bb->addr && addr < bb->addr + bb->size);

	int r;

	if (addr == bb->addr) return -1;
	addr -= sizeof(arm_instr_t);

	for (; addr >= bb->addr; addr -= sizeof(arm_instr_t)) {
		arm_instr_t instr;
		r = image_read_word(image, addr, &instr);
		if (r <= 0) return -1;

		if (!arm_instr_is_flag_changed(instr, ARM_FLAG_C) &&
		    !arm_instr_is_flag_changed(instr, ARM_FLAG_Z)) {
			continue;
		}

		/* instr does change carry and/or zero flag */
		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) return -1;

		if (ip->type == ARM_INSTR_TYPE_DATA_IMM) {
			int cond, opcode, s, rn, rd, rot, imm;
			r = arm_instr_get_params(instr, ip, 7, &cond,
						   &opcode, &s, &rn, &rd,
						   &rot, &imm);
			if (r < 0) abort();

			uint_t rot_imm = (imm >> (rot << 1)) |
				(imm << (32 - (rot << 1)));

			if (cond == ARM_COND_AL &&
			    opcode == ARM_DATA_OPCODE_CMP && rn == reg) {
				return rot_imm;
			}
		}

		return -1;
	}

	return -1;
}

/* Try to backtrack to last add rREG, r15, rX instruction and
   return r15 + X + 8. */
static int
block_backtrack_reg_add_pc(basic_block_t *bb, arm_addr_t addr, uint_t reg,
			   image_t *image, arm_addr_t *data)
{
	assert(addr >= bb->addr && addr < bb->addr + bb->size);

	int r;

	if (addr == bb->addr) return -1;
	addr -= sizeof(arm_instr_t);

	for (; addr >= bb->addr; addr -= sizeof(arm_instr_t)) {
		arm_instr_t instr;
		r = image_read_word(image, addr, &instr);
		if (r <= 0) return -1;

		if (!arm_instr_is_reg_changed(instr, 14)) continue;

		/* instr does change r14 */
		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) return -1;

		if (ip->type == ARM_INSTR_TYPE_DATA_IMM) {
			int cond, opcode, s, rn, rd, rot, imm;
			r = arm_instr_get_params(instr, ip, 7, &cond,
						 &opcode, &s, &rn, &rd,
						 &rot, &imm);
			assert(!(r < 0));

			uint_t rot_imm = (imm >> (rot << 1)) |
				(imm << (32 - (rot << 1)));

			if (cond == ARM_COND_AL &&
			    opcode == ARM_DATA_OPCODE_ADD &&
			    rd == reg && rn == 15) {
				*data = addr + rot_imm + 8;
				return 0;
			}
		}

		return -1;
	}

	return -1;
}

static int
block_pass_third(basic_block_t *bb, map<arm_addr_t, basic_block_t *> *bb_map,
		 image_t *image)
{
	int r;

	for (arm_addr_t addr = bb->addr; addr < bb->addr + bb->size;
	     addr += sizeof(arm_instr_t)) {
		arm_instr_t instr;
		r = image_read_word(image, addr, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		/* code/data analysis */
		bool unpredictable;
		r = arm_instr_is_unpredictable(instr, &unpredictable);
		if (r < 0) return -1;

		if (unpredictable) {
			mark_bb_data(bb);
			break;
		}

		/* find references */
		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) continue;

		int cond;
		bool check_fall_through = false;

		if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
			cond = arm_instr_get_param(instr, ip, 0);
			int link = arm_instr_get_param(instr, ip, 1);
			int offset = arm_instr_get_param(instr, ip, 2);
			int target = arm_instr_branch_target(offset, addr);

			/* also add fall through ref on link */
			if (link) {
				check_fall_through = true;
			}

			/* target reference */
			r = reference_code_add(bb_map, bb, addr, target,
					       (cond != ARM_COND_AL), link);
			if (r < 0) return -1;
			else if (r == 1) break;
		} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT) {
			int opcode, s, rn, rd, sha, sh, rm;
			r = arm_instr_get_params(instr, ip, 8, &cond,
						   &opcode, &s, &rn, &rd,
						   &sha, &sh, &rm);
			assert(!(r < 0));

			if (cond == ARM_COND_LS &&
			    opcode == ARM_DATA_OPCODE_ADD &&
			    rd == 15 && rn == 15 &&
			    sh == ARM_DATA_SHIFT_LSL && sha == 2) {
				/* try to find upper bound for rm */
				int bound = block_backtrack_reg_bounds(
					bb, addr, rm, image);
				if (bound >= 0) {
					while (bound >= 0) {
						arm_addr_t target =
							(bound << 2) +
							addr + 8;
						r = reference_code_add(
							bb_map, bb, addr,
							target, true, false);
						if (r < 0) return -1;
						else if (r == 1) break;

						bound -= 1;
					}
					if (r == 1) break;

					check_fall_through = true;
				}
			} else if (cond == ARM_COND_AL &&
				   opcode == ARM_DATA_OPCODE_MOV &&
				   rd == 15 && sh == ARM_DATA_SHIFT_LSL &&
				   sha == 0) {
				/* could be a link */
				arm_addr_t r14_data;
				r = block_backtrack_reg_add_pc(bb, addr, 14,
							       image,
							       &r14_data);
				if (r >= 0 &&
				    r14_data == addr + sizeof(arm_instr_t)) {
					r = reference_code_add(bb_map, bb,
							       addr, r14_data,
							       false, true);
					if (r < 0) return -1;
					else if (r == 1) break;
				}
				check_fall_through = true;
			} else if ((opcode < ARM_DATA_OPCODE_TST &&
				    opcode > ARM_DATA_OPCODE_CMN) &&
				   rd == 15) {
				check_fall_through = true;
			}
		} else if (ip->type == ARM_INSTR_TYPE_LS_IMM_OFF) {
			int p, u, b, w, load, rn, rd, imm;
			r = arm_instr_get_params(instr, ip, 9, &cond, &p, &u,
						 &b, &w, &load, &rn, &rd,
						 &imm);
			assert(!(r < 0));

			/* data reference */
			if (rn == 15 && cond != ARM_COND_NV) {
				arm_addr_t target = addr + 8 +
					(imm * (u ? 1 : -1));
				reference_data_add(bb_map, bb, addr,
						   target, (b ? 1 : 4));
			}


			/* code reference */
			if (load && rd == 15) {
				if (p && !w && rn == 15) {
					int off = imm * (u ? 1 : -1);
					arm_addr_t target = addr + off + 8;

					uint32_t jump_target;
					r = image_read_word(image, target,
							    &jump_target);
					if (r >= 0) {
						r = reference_code_add(
							bb_map, bb, addr,
							jump_target,
							(cond != ARM_COND_AL),
							false);
						if (r < 0) return -1;
						else if (r == 1) break;
					}
				}

				check_fall_through = true;
			}
		} else {
			arm_cond_t c;
			r = arm_instr_get_cond(instr, &c);
			if (r < 0) c = ARM_COND_AL;
			cond = static_cast<int>(c);

			/* r15 is changed */
			r = arm_instr_is_reg_changed(instr, 15);
			if (r < 0) return -1;
			else if (r) {
				check_fall_through = true;;
			}
		}

		/* check fall-through reference */
		if (check_fall_through && (cond != ARM_COND_AL) &&
		    addr == (bb->addr + bb->size - sizeof(arm_instr_t))) {
			/* last instruction in block */
			r = reference_code_add(bb_map, bb, addr,
					       addr + sizeof(arm_instr_t),
					       true, false);
			if (r < 0) return -1;
			else if (r == 1) break;
		}
	}

	return 0;
}

static int
bb_pass_third(map<arm_addr_t, basic_block_t *> *bb_map, image_t *image)
{
	int r;

	map<arm_addr_t, basic_block_t *>::iterator bb_iter = bb_map->begin();
	while (bb_iter != bb_map->end()) {
		basic_block_t *bb = (bb_iter++)->second;

		if (bb->type != BASIC_BLOCK_TYPE_DATA) {
			r = block_pass_third(bb, bb_map, image);
			if (r < 0) return -1;
		}
	}

	return 0;
}

static int
bb_pass_merge(map<arm_addr_t, basic_block_t *> *bb_map, image_t *image)
{
	int r;

	map<arm_addr_t, basic_block_t *>::iterator bb_iter, bb_save;
	bb_iter = bb_map->begin();
	if (bb_iter != bb_map->end()) {
		basic_block_t *bb_prev = (bb_iter++)->second;
		while (bb_iter != bb_map->end()) {
			bb_save = bb_iter++;
			basic_block_t *bb = bb_save->second;

			if (bb_prev->type == BASIC_BLOCK_TYPE_DATA &&
			    bb->type == BASIC_BLOCK_TYPE_DATA) {
				/* merge data blocks */
				data_block_merge(bb_prev, bb);
				bb_map->erase(bb_save);
			} else {
				bb_prev = bb;
			}
		}
	}

	return 0;
}

static int
bb_pass_codetree(map<arm_addr_t, basic_block_t *> *bb_map,
		 list<arm_addr_t> *entrypoints)
{
	/* entrypoints */
	list<arm_addr_t>::iterator entrypoint_iter;
	for (entrypoint_iter = entrypoints->begin();
	     entrypoint_iter != entrypoints->end(); entrypoint_iter++) {
		mark_bb_code((*bb_map)[*entrypoint_iter]);
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
		basicblock_add(bb_map, (*entrypoint_iter), image);
	}

	/* first pass: */
	/* mark basic blocks */
	r = bb_pass_mark(bb_map, image);
	if (r < 0) return -1;

	/* second pass: */
	/* save basic block size */
	r = bb_pass_size(bb_map, image);
	if (r < 0) return -1;

	/* third pass: */
	/* do code/data analysis */
	/* collect references */
	r = bb_pass_third(bb_map, image);
	if (r < 0) return -1;

	/* fourth pass: */
	/* follow reference tree from entrypoints and
	   mark code blocks */
	r = bb_pass_codetree(bb_map, entrypoints);
	if (r < 0) return -1;

	/* fifth pass: */
	/* merge adjacent blocks */
	r = bb_pass_merge(bb_map, image);
	if (r < 0) return -1;

	return 0;
}
