/* codesep.c */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "codesep.h"
#include "arm.h"
#include "entrypoint.h"
#include "image.h"
#include "list.h"
#include "types.h"


typedef struct {
	list_elm_t elm;
	arm_addr_t addr;
} bb_stack_elm_t;


static void
report_jump_fail(arm_instr_t instr, arm_addr_t addr)
{
	FILE *f = stdout;

	fprintf(f, "Unable to handle jump at 0x%x.\n", addr);
	fprintf(f, " Instruction: ");
	arm_instr_fprint(f, instr, addr, NULL, NULL);
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

		r = arm_is_reg_changed(instr, reg);
		if (r < 0) return -1;
		else if (r) {
			*change_addr = addr;
			return 1;
		}

		addr -= sizeof(arm_instr_t);
	}

	return 0;
}

static int
separate_basic_block(image_t *image, list_t *stack, arm_addr_t addr,
		     uint8_t *code_bitmap)
{
	int r;
	arm_addr_t bb_begin = addr;

	while (1) {
		if (code_bitmap[addr >> 2]) break;
		code_bitmap[addr >> 2] = 0xff;

		arm_instr_t instr;
		r = image_read_instr(image, addr, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) {
			fprintf(stderr, "Undefined instruction encountered in"
				" code/data separation at 0x%x.\n", addr);
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
				bb_stack_elm_t *bbs =
					malloc(sizeof(bb_stack_elm_t));
				if (bbs == NULL) abort();

				bbs->addr = target;
				list_prepend(stack, (list_elm_t *)bbs);
			} else {
				addr += sizeof(arm_instr_t);
			}
		} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT) {
			int cond, opcode, s, rn, rd, sha, sh, rm;
			r = arm_instr_get_params(instr, ip, 8, &cond,
						 &opcode, &s, &rn, &rd, &sha,
						 &sh, &rm);
			if (r < 0) abort();

			if ((opcode < ARM_DATA_OPCODE_TST ||
			     opcode > ARM_DATA_OPCODE_CMN) && rd == 15 &&
			    cond != ARM_COND_NV) {
				if (opcode == ARM_DATA_OPCODE_MOV &&
				    sh == ARM_DATA_SHIFT_LSL && sha == 0 &&
				    rm == 14) {
					arm_addr_t btaddr;
					r = backtrack_reg_change(image,
								 14, addr,
								 bb_begin,
								 &btaddr);
					if (r < 0) {
						fprintf(stderr,
							"Backtrack error.\n");
						break;
					} else if (r) {
						report_jump_fail(instr, addr);
						break;
					} else {
						printf("pc <- lr backtrack"
						       " success at 0x%x.\n",
						       addr);
						if (cond == ARM_COND_AL) {
							break;
						} else {
							addr +=	sizeof(
								arm_instr_t);
						}
					}
				} else {
					report_jump_fail(instr, addr);
					break;
				}
			} else {
				addr += sizeof(arm_instr_t);
			}
		} else if (ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_IMM) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int opcode = arm_instr_get_param(instr, ip, 1);
			int rd = arm_instr_get_param(instr, ip, 4);

			if ((opcode < ARM_DATA_OPCODE_TST ||
			     opcode > ARM_DATA_OPCODE_CMN) && rd == 15 &&
			    cond != ARM_COND_NV) {
				report_jump_fail(instr, addr);
				break;
			}
			addr += sizeof(arm_instr_t);
		} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int load = arm_instr_get_param(instr, ip, 5);
			int reglist = arm_instr_get_param(instr, ip, 7);

			if (load && (reglist & (1 << 15)) &&
			    cond != ARM_COND_NV) {
				report_jump_fail(instr, addr);
				break;
			}
			addr += sizeof(arm_instr_t);
		} else {
			addr += sizeof(arm_instr_t);
		}
	}
}

int
codesep_analysis(list_t *ep_list, image_t *image,
		 uint8_t **code_bitmap, FILE *f)
{
	int r;

	*code_bitmap = calloc(image->size >> 2, sizeof(uint8_t));
	if (*code_bitmap == NULL) abort();

	list_t stack;
	list_init(&stack);

	list_elm_t *elm;
	list_foreach(ep_list, elm) {
		ep_elm_t *ep = (ep_elm_t *)elm;
		bb_stack_elm_t *bbs = malloc(sizeof(bb_stack_elm_t));
		if (bbs == NULL) abort();

		bbs->addr = ep->addr;
		list_prepend(&stack, (list_elm_t *)bbs);
	}

	while (!list_is_empty(&stack)) {
		bb_stack_elm_t *bbs =
			(bb_stack_elm_t *)list_remove_head(&stack);
		arm_addr_t addr = bbs->addr;
		free(bbs);

		separate_basic_block(image, &stack, addr, *code_bitmap);
	}

	return 0;
}
