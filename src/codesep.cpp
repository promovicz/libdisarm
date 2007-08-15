/* codesep.cpp */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cctype>
#include <cerrno>

#include <iostream>
#include <iomanip>
#include <list>
#include <stack>

#include "codesep.hh"
#include "arm.hh"
#include "image.hh"
#include "types.hh"

#define CODESEP_CODE  0xff;
#define CODESEP_DATA  0x7f;

using namespace std;


static void
report_jump_fail(arm_instr_t instr, arm_addr_t addr)
{
	cout << "Unable to handle jump at "
	     << hex << setw(8) << setfill('0') << addr << endl;
	cout << " Instruction: ";
	arm_instr_fprint(stdout, instr, addr, NULL);
}

static int
backtrack_reg_change(image_t *image, uint_t reg, arm_addr_t addr,
		     arm_addr_t limit, arm_addr_t *change_addr)
{
	int r;

	while (addr >= limit) {
		arm_instr_t instr;
		r = image_read_instr(image, addr, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		r = arm_instr_is_reg_changed(instr, reg);
		if (r < 0) return -1;
		else if (r) {
			*change_addr = addr;
			return 1;
		}

		addr -= sizeof(arm_instr_t);
	}

	return 0;
}

static arm_addr_t
separate_bb_data_imm_shift(image_t *image,
			   arm_instr_t instr, const arm_instr_pattern_t *ip,
			   stack<arm_instr_t> *bb_stack,
			   arm_addr_t addr, arm_addr_t bb_begin)
{
	int r;
	int cond, opcode, s, rn, rd, sha, sh, rm;
	r = arm_instr_get_params(instr, ip, 8, &cond,
				 &opcode, &s, &rn, &rd, &sha,
				 &sh, &rm);
	if (r < 0) abort();

	if ((opcode < ARM_DATA_OPCODE_TST || opcode > ARM_DATA_OPCODE_CMN) &&
	    rd == 15 && cond != ARM_COND_NV) {
		if (opcode == ARM_DATA_OPCODE_MOV &&
		    sh == ARM_DATA_SHIFT_LSL && sha == 0 && rm == 14) {
			arm_addr_t btaddr;
			r = backtrack_reg_change(image, 14, addr, bb_begin,
						 &btaddr);
			if (r < 0) cerr << "Backtrack error." << endl;
			else if (r) report_jump_fail(instr, addr);

			if (cond != ARM_COND_AL) addr += sizeof(arm_instr_t);
		} else {
			report_jump_fail(instr, addr);
		}
	} else {
		addr += sizeof(arm_instr_t);
	}

	return addr;
}

static int
separate_basic_block(image_t *image, stack<arm_instr_t> *bb_stack,
		     map<arm_addr_t, list<arm_addr_t> *> *dest_map,
		     arm_addr_t addr, uint8_t *code_bitmap)
{
	int r;
	arm_addr_t bb_begin = addr;

	while (1) {
		if (code_bitmap[addr >> 2]) break;
		code_bitmap[addr >> 2] = CODESEP_CODE;

		map<arm_addr_t, list<arm_addr_t> *>::iterator dest_list_pos =
			dest_map->find(addr);
		if (dest_list_pos != dest_map->end()) {
			list<arm_addr_t> *dest_list =
				(*dest_list_pos).second;
			while (!dest_list->empty()) {
				arm_addr_t addr = dest_list->front();
				dest_list->pop_front();
				bb_stack->push(addr);
			}

			dest_map->erase(dest_list_pos);
			delete dest_list;
			break;
		}

		arm_instr_t instr;
		r = image_read_instr(image, addr, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) {
			cerr << "Undefined instruction encountered in"
			     << " code/data separation at 0x"
			     << hex << addr << "." << endl;
			break;
		}

		if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int link = arm_instr_get_param(instr, ip, 1);
			int offset = arm_instr_get_param(instr, ip, 2);
			int target = arm_instr_branch_target(offset, addr);

			if (!link && cond == ARM_COND_AL) {
				addr = target;
			} else if (cond != ARM_COND_NV) {
				/* fall through */
				addr += sizeof(arm_instr_t);

				/* branch target */
				bb_stack->push(target);
			} else {
				addr += sizeof(arm_instr_t);
			}
		} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT) {
			addr = separate_bb_data_imm_shift(image, instr, ip,
							  bb_stack, addr,
							  bb_begin);
		} else if (ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_IMM) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int opcode = arm_instr_get_param(instr, ip, 1);
			int rd = arm_instr_get_param(instr, ip, 4);

			if ((opcode < ARM_DATA_OPCODE_TST ||
			     opcode > ARM_DATA_OPCODE_CMN) && rd == 15 &&
			    cond != ARM_COND_NV) {
				report_jump_fail(instr, addr);
				if (cond == ARM_COND_AL) break;
			}
			addr += sizeof(arm_instr_t);
		} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int load = arm_instr_get_param(instr, ip, 5);
			int reglist = arm_instr_get_param(instr, ip, 7);

			if (load && (reglist & (1 << 15)) &&
			    cond != ARM_COND_NV) {
				report_jump_fail(instr, addr);
				if (cond == ARM_COND_AL) break;
			}
			addr += sizeof(arm_instr_t);
		} else {
			r = arm_instr_is_reg_changed(instr, 15);
			if (r < 0) return -1;
			else if (r) {
				cout << "Type not explicitly handled: "
				     << ip->type << endl;
				report_jump_fail(instr, addr);
				break;
			}
			addr += sizeof(arm_instr_t);
		}
	}
}

static int
jump_dest_add(map<arm_addr_t, list<arm_addr_t> *> *dest_map,
	      arm_addr_t addr, list<arm_addr_t> *dests)
{
	int r;

	map<arm_addr_t, list<arm_addr_t> *>::iterator dest_list_pos =
		dest_map->find(addr);
	if (dest_list_pos != dest_map->end()) {
		list<arm_addr_t> *old_dests = (*dest_list_pos).second;
		dest_map->erase(dest_list_pos);
		dests->merge(*old_dests);
		delete old_dests;
	}

	(*dest_map)[addr] = dests;

	return 0;
}

static int
read_jump_dest_from_file(map<arm_addr_t, list<arm_addr_t> *> *dest_map,
			 FILE *f)
{
	int r;
	char *text = NULL;
	size_t textlen = 0;
	ssize_t read;

	while ((read = getline(&text, &textlen, f)) != -1) {
		if (read == 0 || !strcmp(text, "\n") ||
		    !strncmp(text, "#", 1)) {
			continue;
		} else {
			errno = 0;
			char *endptr;
			arm_addr_t addr = strtol(text, &endptr, 16);

			if ((errno == ERANGE &&
			     (addr == LONG_MAX || addr == LONG_MIN))
			    || (errno != 0 && addr == 0)
			    || (endptr == text)) {
				delete text;
				cerr << "Unable to parse address." << endl;
				return -1;
			}

			list<arm_addr_t> *dests = new list<arm_addr_t>();
			if (dests == NULL) {
				errno = ENOMEM;
				return -1;
			}

			while (1) {
				while (isblank(*endptr)) endptr += 1;
				if (*endptr == '\0' || *endptr == '\n') break;

				char *next = endptr;
				arm_addr_t dest = strtol(next, &endptr, 16);

				if ((errno == ERANGE &&
				     (dest == LONG_MAX || dest == LONG_MIN))
				    || (errno != 0 && dest == 0)
				    || (endptr == text)) {
					delete text;
					cerr << "Unable to parse address."
					     << endl;
					return -1;
				}

				dests->push_back(dest);
			}

			r = jump_dest_add(dest_map, addr, dests);
			if (r < 0) {
				int errsv = errno;
				delete dests;
				errno = errsv;
				return -1;
			}
		}
	}

	if (text) delete text;

	return 0;
}

int
codesep_analysis(list<arm_addr_t> *ep_list, image_t *image,
		 uint8_t **code_bitmap, FILE *f)
{
	int r;

	map<arm_addr_t, list<arm_addr_t> *> dest_map;

	if (f != NULL) {
		r = read_jump_dest_from_file(&dest_map, f);
		if (r < 0) return -1;
	}

	*code_bitmap = static_cast<uint8_t *>
		(calloc(image->size >> 2, sizeof(uint8_t)));
	if (*code_bitmap == NULL) abort();

	stack<arm_addr_t> bb_stack;

	list<arm_addr_t>::iterator ep_iter;
	for (ep_iter = ep_list->begin(); ep_iter != ep_list->end();
	     ep_iter++) {
		bb_stack.push(*ep_iter);
	}

	while (!bb_stack.empty()) {
		arm_addr_t addr = bb_stack.top();
		bb_stack.pop();

		separate_basic_block(image, &bb_stack, &dest_map,
				     addr, *code_bitmap);
	}

	return 0;
}
