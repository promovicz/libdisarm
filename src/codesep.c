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


int
code_data_separate(list_t *ep_list, image_t *image, FILE *f)
{
	int r;

	uint8_t *code_bitmap = calloc(image->size >> 2, sizeof(uint8_t));
	if (code_bitmap == NULL) abort();

	typedef struct {
		list_elm_t elm;
		arm_addr_t addr;
	} bb_stack_elm_t;

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
				fprintf(stderr, "Undefined instruction"
					" encountered in code/data"
					" separation at 0x%x.\n", addr);
				break;
			}

			if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
				int cond = arm_instr_get_param(instr, ip, 0);
				int link = arm_instr_get_param(instr, ip, 1);
				int offset = arm_instr_get_param(instr, ip, 2);
				int target = arm_instr_branch_target(offset,
								     addr);

				if (!link && cond == ARM_COND_AL) {
					addr = target;
				} else {
					/* fall through */
					addr += sizeof(arm_instr_t);

					/* branch target */
					bb_stack_elm_t *bbs =
						malloc(sizeof(bb_stack_elm_t));
					if (bbs == NULL) abort();

					bbs->addr = target;
					list_prepend(&stack,
						     (list_elm_t *)bbs);
				}
			} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT ||
				   ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
				   ip->type == ARM_INSTR_TYPE_DATA_IMM) {
				int rd = arm_instr_get_param(instr, ip, 4);
				if (rd == 15) {
					fprintf(stderr, "Unable to handle jump"
						" at 0x%x.\n", addr);
					break;
				}
				addr += sizeof(arm_instr_t);
			} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
				int load = arm_instr_get_param(instr, ip, 5);
				int reglist = arm_instr_get_param(instr,
								  ip, 7);
				if (load && (reglist & (1 << 15))) {
					fprintf(stderr, "Unable to handle jump"
						" at 0x%x.\n", addr);
					break;
				}
				addr += sizeof(arm_instr_t);
			} else {
				addr += sizeof(arm_instr_t);
			}
		}
	}

	FILE *out = fopen("code_bitmap", "wb");
	if (out == NULL) return -1;
	size_t write = fwrite(code_bitmap, sizeof(uint8_t),
			      image->size >> 2, out);
	if (write < (image->size >> 2)) {
		errno = EIO;
		return -1;
	}

	return 0;
}
