/* disarm.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include "arm.h"
#include "endian.h"
#include "list.h"


int
main(int argc, char *argv[])
{
	int r, i;

	if (argc < 2) return EXIT_FAILURE;

	FILE *f = fopen(argv[1], "rb");
	if (f == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	arm_instr_t instr;

	/* basic block analysis */
	r = fseek(f, 0, SEEK_SET);
	if (r < 0) {
		perror("fseeko");
		exit(EXIT_FAILURE);
	}

	list_t bb_list;
	list_init(&bb_list);

	typedef struct {
		list_elm_t elm;
		unsigned int target;
		unsigned int source;
	} bb_ref_elm_t;

	i = 0;
	while (1) {
		r = fread(&instr, sizeof(arm_instr_t), 1, f);
		if (r < 1) {
			if (feof(f)) break;
			perror("fread");
			exit(EXIT_FAILURE);
		}

		instr = htobe32(instr);
		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) {
			i += sizeof(arm_instr_t);
			continue;
		}

		if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
			bb_ref_elm_t *bbref = malloc(sizeof(bb_ref_elm_t));
			if (bbref == NULL) abort();

			bbref->source = i;
			int offset = arm_instr_get_param(instr, ip, 2);
			bbref->target = arm_instr_branch_target(offset, i);

			list_elm_t *elm;
			int inserted = 0;
			list_foreach(bb_list, elm) {
				bb_ref_elm_t *oldref = (bb_ref_elm_t *)elm;
				if (oldref->target > bbref->target) {
					list_insert_before(
						(list_elm_t *)oldref,
						(list_elm_t *)bbref);
					inserted = 1;
					break;
				}
			}
			if (!inserted) list_append(&bb_list,
						   (list_elm_t *)bbref);
		}

		i += sizeof(arm_instr_t);
	}

	/* print instructions */
	r = fseek(f, 0, SEEK_SET);
	if (r < 0) {
		perror("fseeko");
		exit(EXIT_FAILURE);
	}

	i = 0;
	while (1) {
		r = fread(&instr, sizeof(arm_instr_t), 1, f);
		if (r < 1) {
			if (feof(f)) break;
			perror("fread");
			exit(EXIT_FAILURE);
		}

		instr = htobe32(instr);

		int bb_begin = 0;
		while (1) {
			bb_ref_elm_t *bbref =
				(bb_ref_elm_t *)list_head(&bb_list);

			if (bbref->target <= i) {
				if (!bb_begin) {
					bb_begin = 1;
					printf("\n");
					printf("; basic blocks begins\n");
					printf("; reference from 0x%x",
					       bbref->source);
				} else {
					printf(", 0x%x", bbref->source);
				}

				list_elm_remove((list_elm_t *)bbref);
				free(bbref);
			} else break;
		}
		if (bb_begin) printf("\n");

		printf("%08x\t%08x\t", i, instr);
		arm_instr_fprint(stdout, instr, i);

		i += sizeof(arm_instr_t);
	}

	return EXIT_SUCCESS;
}
