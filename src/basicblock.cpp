/* basicblock.cpp */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "basicblock.hh"
#include "arm.hh"
#include "symbol.hh"
#include "types.hh"


static void
reference_add(hashtable_t *reflist_ht, hashtable_t *sym_ht,
	      arm_addr_t source, arm_addr_t target, int cond, int link)
{
	int r;

	ref_elm_t *ref = static_cast<ref_elm_t *>(malloc(sizeof(ref_elm_t)));
	if (ref == NULL) abort();

	ref->source = source;
	ref->cond = cond;
	ref->link = link;

	reflist_elm_t *reflist = (reflist_elm_t *)
		hashtable_lookup(reflist_ht, &target, sizeof(arm_addr_t));
	if (reflist == NULL) {
		reflist = static_cast<reflist_elm_t *>(
			malloc(sizeof(reflist_elm_t)));
		if (reflist == NULL) abort();
		reflist->target = target;
		list_init(&reflist->refs);

		hashtable_elm_t *old;
		r = hashtable_insert(reflist_ht, (hashtable_elm_t *)reflist,
				     &reflist->target, sizeof(uint_t), &old);
		if (r < 0) {
			perror("hashtable_insert");
			exit(EXIT_FAILURE);
		}
		if (old != NULL) free(old);
	}

	list_append(&reflist->refs, (list_elm_t *)ref);

	if (link) {
		char *funname = NULL;
		static const char *format = "fun_%x";

		r = snprintf(NULL, 0, format, target);
		if (r > 0) {
			funname = (char *)malloc((r+1)*sizeof(char));
			if (funname == NULL) abort();
			r = snprintf(funname, r+1, format, target);
		}

		if (r <= 0) {
			if (funname != NULL) free(funname);
			return;
		}

		symbol_add(sym_ht, target, funname, 0);
		free(funname);
	}
}

static void
basicblock_add(hashtable_t *bb_ht, uint_t addr)
{
	int r;
	hashtable_elm_t *helm = hashtable_lookup(bb_ht, &addr, sizeof(uint_t));
	if (helm == NULL) {
		bb_elm_t *bb = static_cast<bb_elm_t *>(
			malloc(sizeof(bb_elm_t)));
		if (bb == NULL) abort();
		bb->addr = addr;

		hashtable_elm_t *old;
		r = hashtable_insert(bb_ht, (hashtable_elm_t *)bb,
				     &bb->addr, sizeof(uint_t), &old);
		if (r < 0) {
			perror("hashtable_insert");
			exit(EXIT_FAILURE);
		}
		if (old != NULL) free(old);
	}
}

int
basicblock_analysis(hashtable_t *bb_ht, hashtable_t *sym_ht,
		    hashtable_t *reflist_ht, image_t *image,
		    uint8_t *codemap)
{
	int r;

	bb_elm_t *entry_point = static_cast<bb_elm_t *>(
		malloc(sizeof(bb_elm_t)));
	if (entry_point == NULL) abort();
	entry_point->addr = 0x0;

	hashtable_elm_t *old;
	r = hashtable_insert(bb_ht, (hashtable_elm_t *)entry_point,
			     &entry_point->addr, sizeof(uint_t), &old);
	if (r < 0) {
		perror("hashtable_insert");
		exit(EXIT_FAILURE);
	}
	if (old != NULL) free(old);

	uint_t i = 0;
	while (1) {
		arm_instr_t instr;
		r = image_read_instr(image, i, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

#if 0
		if (!codemap[i >> 2]) {
			i += sizeof(arm_instr_t);
			continue;
		}
#endif

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
			basicblock_add(bb_ht, i + sizeof(arm_instr_t));

			if (link || cond != ARM_COND_AL) {
				/* fall-through reference */
				reference_add(reflist_ht, sym_ht, i,
					      i + sizeof(arm_instr_t), 0, 0);
			}

			/* basic block for branch target */
			basicblock_add(bb_ht, target);

			/* target reference */
			reference_add(reflist_ht, sym_ht, i, target,
				      (cond != ARM_COND_AL), link);
		} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_IMM) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int rd = arm_instr_get_param(instr, ip, 4);
			if (rd == 15) {
				basicblock_add(bb_ht, i + sizeof(arm_instr_t));

				if (cond != ARM_COND_AL) {
					/* fall-through reference */
					reference_add(reflist_ht, sym_ht, i,
						      i + sizeof(arm_instr_t),
						      1, 0);
				}
			}
		} else if (ip->type == ARM_INSTR_TYPE_LS_REG_OFF ||
			   ip->type == ARM_INSTR_TYPE_LS_IMM_OFF) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int rd = arm_instr_get_param(instr, ip, 7);
			if (rd == 15) {
				basicblock_add(bb_ht, i + sizeof(arm_instr_t));

				if (cond != ARM_COND_AL) {
					/* fall-through reference */
					reference_add(reflist_ht, sym_ht, i,
						      i + sizeof(arm_instr_t),
						      1, 0);
				}
			}
		} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
			int cond = arm_instr_get_param(instr, ip, 0);
			int load = arm_instr_get_param(instr, ip, 5);
			int reglist = arm_instr_get_param(instr, ip, 7);
			if (load && (reglist & (1 << 15))) {
				basicblock_add(bb_ht, i + sizeof(arm_instr_t));

				if (cond != ARM_COND_AL) {
					/* fall-through reference */
					reference_add(reflist_ht, sym_ht, i,
						      i + sizeof(arm_instr_t),
						      1, 0);
				}
			}
		}

		i += sizeof(arm_instr_t);
	}

	return 0;
}
