/* codesep.cpp */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include "codesep.hh"
#include "arm.hh"
#include "entrypoint.hh"
#include "image.hh"
#include "list.hh"
#include "types.hh"

#define CODESEP_CODE  0xff;
#define CODESEP_DATA  0x7f;


typedef struct {
	list_elm_t elm;
	arm_addr_t addr;
} bb_stack_elm_t;

typedef struct {
	hashtable_elm_t elm;
	arm_addr_t addr;
	list_t *dests;
} destlist_elm_t;

typedef struct {
	list_elm_t elm;
	arm_addr_t addr;
} dest_elm_t;


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

static int
separate_basic_block(image_t *image, list_t *stack, hashtable_t *dest_ht,
		     arm_addr_t addr, uint8_t *code_bitmap)
{
	int r;
	arm_addr_t bb_begin = addr;

	while (1) {
		if (code_bitmap[addr >> 2]) break;
		code_bitmap[addr >> 2] = CODESEP_CODE;

		destlist_elm_t *destlist = (destlist_elm_t *)
			hashtable_lookup(dest_ht, &addr, sizeof(arm_addr_t));
		if (destlist != NULL) {
			while (!list_is_empty(destlist->dests)) {
				dest_elm_t *dest = (dest_elm_t *)
					list_remove_head(destlist->dests);

				bb_stack_elm_t *bbs =
					static_cast<bb_stack_elm_t *>
					(malloc(sizeof(bb_stack_elm_t)));
				if (bbs == NULL) abort();

				bbs->addr = dest->addr;
				list_prepend(stack, (list_elm_t *)bbs);

				free(dest);
			}

			r = hashtable_remove(dest_ht,
					     (hashtable_elm_t *)destlist);
			if (r < 0) return -1;

			free(destlist->dests);
			free(destlist);
		}

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
					static_cast<bb_stack_elm_t *>
					(malloc(sizeof(bb_stack_elm_t)));
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
					} else if (r) {
						report_jump_fail(instr, addr);
					}
					if (cond == ARM_COND_AL) break;
					else addr += sizeof(arm_instr_t);
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
				report_jump_fail(instr, addr);
				break;
			}
			addr += sizeof(arm_instr_t);
		}
	}
}

static int
jump_dest_add(hashtable_t *dest_ht, arm_addr_t addr, list_t *dests)
{
	int r;

	destlist_elm_t *destlist = static_cast<destlist_elm_t *>
		(malloc(sizeof(destlist_elm_t)));
	if (destlist == NULL) {
		errno = ENOMEM;
		return -1;
	}

	destlist->addr = addr;
	destlist->dests = dests;

	destlist_elm_t *old;
	r = hashtable_insert(dest_ht, (hashtable_elm_t *)destlist,
			     &destlist->addr, sizeof(arm_addr_t),
			     (hashtable_elm_t **)&old);
	if (r < 0) {
		int errsv = errno;
		free(destlist);
		errno = errsv;
		return -1;
	}
	if (old != NULL) {
		while (!list_is_empty(old->dests)) {
			dest_elm_t *dest =
				(dest_elm_t *)list_remove_head(old->dests);
			list_prepend(destlist->dests, (list_elm_t *)dest);
		}
		free(old->dests);
		free(old);
	}

	return 0;
}

static int
read_jump_dest_from_file(hashtable_t *dest_ht, FILE *f)
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
				free(text);
				fprintf(stderr, "Unable to parse address.\n");
				return -1;
			}

			list_t *dests = static_cast<list_t *>
				(malloc(sizeof(list_t)));
			if (dests == NULL) {
				errno = ENOMEM;
				return -1;
			}
			list_init(dests);

			while (1) {
				while (isblank(*endptr)) endptr += 1;
				if (*endptr == '\0' || *endptr == '\n') break;

				char *next = endptr;
				arm_addr_t dest = strtol(next, &endptr, 16);

				if ((errno == ERANGE &&
				     (dest == LONG_MAX || dest == LONG_MIN))
				    || (errno != 0 && dest == 0)
				    || (endptr == text)) {
					free(text);
					fprintf(stderr,
						"Unable to parse address.\n");
					return -1;
				}

				dest_elm_t *destelm = static_cast<dest_elm_t *>
					(malloc(sizeof(dest_elm_t)));
				if (destelm == NULL) {
					errno = ENOMEM;
					return -1;
				}

				destelm->addr = dest;

				list_append(dests, (list_elm_t *)destelm);
			}

			r = jump_dest_add(dest_ht, addr, dests);
			if (r < 0) {
				int errsv = errno;
				free(dests);
				errno = errsv;
				return -1;
			}
		}
	}

	if (text) free(text);

	return 0;
}

int
codesep_analysis(list_t *ep_list, image_t *image,
		 uint8_t **code_bitmap, FILE *f)
{
	int r;

	hashtable_t dest_ht;
	r = hashtable_init(&dest_ht, 0);
	if (r < 0) return -1;

	if (f != NULL) {
		r = read_jump_dest_from_file(&dest_ht, f);
		if (r < 0) {
			hashtable_deinit(&dest_ht);
			return -1;
		}
	}

	*code_bitmap = static_cast<uint8_t *>
		(calloc(image->size >> 2, sizeof(uint8_t)));
	if (*code_bitmap == NULL) abort();

	list_t stack;
	list_init(&stack);

	list_elm_t *elm;
	list_foreach(ep_list, elm) {
		ep_elm_t *ep = (ep_elm_t *)elm;
		bb_stack_elm_t *bbs = static_cast<bb_stack_elm_t *>
			(malloc(sizeof(bb_stack_elm_t)));
		if (bbs == NULL) abort();

		bbs->addr = ep->addr;
		list_prepend(&stack, (list_elm_t *)bbs);
	}

	while (!list_is_empty(&stack)) {
		bb_stack_elm_t *bbs =
			(bb_stack_elm_t *)list_remove_head(&stack);
		arm_addr_t addr = bbs->addr;
		free(bbs);

		separate_basic_block(image, &stack, &dest_ht,
				     addr, *code_bitmap);
	}

	hashtable_deinit(&dest_ht);

	return 0;
}
