/* disarm.c */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>

#include "arm.h"
#include "endian.h"
#include "hashtable.h"
#include "inter.h"
#include "list.h"


typedef struct {
	hashtable_elm_t elm;
	unsigned int addr;
	int pre;
	char *line;
} annot_elm_t;

typedef struct {
	hashtable_elm_t elm;
	unsigned int target;
	unsigned int source;
	int cond;
	int link;
} ref_elm_t;

typedef struct {
	hashtable_elm_t elm;
	unsigned int addr;
} bb_elm_t;

typedef enum {
	SECT_TYPE_UNKNOWN = 0,
	SECT_TYPE_DATA,
	SECT_TYPE_LIT_POOL,
	SECT_TYPE_CODE,
	SECT_TYPE_FUNC,
	SECT_TYPE_MAX
} section_type_t;

typedef struct {
	list_elm_t elm;
	unsigned int addr;
	unsigned int size;
	list_t subsections;
	char *name;
	char *desc;
	section_type_t type;
} image_section_t;


static hashtable_t annot_ht;
static hashtable_t ref_ht;
static hashtable_t bb_ht;
static image_section_t root_sect;


static char *
addr_string(unsigned int addr)
{
	char *addrstr = NULL;
	int r;

	static const char *format = "0x%x";

	r = snprintf(NULL, 0, format, addr);
	if (r > 0) {
		addrstr = (char *)malloc((r+1)*sizeof(char));
		if (addrstr == NULL) abort();
		r = snprintf(addrstr, r+1, format, addr);
	}

	if (r <= 0) {
		if (addrstr != NULL) free(addrstr);
		return NULL;
	}

	return addrstr;
}

static int
annot_read(FILE *f, hashtable_t *annot_ht)
{
	char *line = NULL;
	size_t linelen = 0;
	ssize_t read;

	unsigned int addr = 0;
	int reading_addr = 1;
	int pre = 1;

	while ((read = getline(&line, &linelen, f)) != -1) {
		if (read == 0 || !strcmp(line, "\n")) {
			reading_addr = 1;
		} else if (reading_addr) {
			errno = 0;
			char *endptr;
			addr = strtol(line, &endptr, 16);

			if ((errno == ERANGE &&
			     (addr == LONG_MAX || addr == LONG_MIN))
			    || (errno != 0 && addr == 0)
			    || (endptr == line)) {
				free(line);
				fprintf(stderr, "Unable to parse address.\n");
				return -1;
			}

			reading_addr = 0;
			pre = 1;
		} else if (!strcmp(line, "--\n")) {
			pre = 0;
		} else {
			annot_elm_t *annot = malloc(sizeof(annot_elm_t));
			if (annot == NULL) abort();

			annot->addr = addr;
			annot->pre = pre;
			annot->line = line;

			hashtable_store(annot_ht, (hashtable_elm_t *)annot,
					&annot->addr, sizeof(unsigned int));

			line = NULL;
		}
	}

	if (line) free(line);

	return 0;
}

static int
basic_block_analysis(FILE *f, hashtable_t *bb_ht)
{
	int r, i;

	bb_elm_t *entry_point = malloc(sizeof(bb_elm_t));
	if (entry_point == NULL) abort();
	entry_point->addr = 0x0;
	hashtable_store(bb_ht, (hashtable_elm_t *)entry_point,
			&entry_point->addr, sizeof(unsigned int));

	i = 0;
	while (1) {
		arm_instr_t instr;
		r = fread(&instr, sizeof(arm_instr_t), 1, f);
		if (r < 1) {
			if (feof(f)) break;
			return -1;
		}

		instr = htobe32(instr);
		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) {
			i += sizeof(arm_instr_t);
			continue;
		}

		if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
			int inserted;
			list_elm_t *elm;

			int cond = arm_instr_get_param(instr, ip, 0);
			int link = arm_instr_get_param(instr, ip, 1);
			int offset = arm_instr_get_param(instr, ip, 2);
			int target = arm_instr_branch_target(offset, i);


			/* basic block for fall-through */
			if (!link) {
				bb_elm_t *bbnext = malloc(sizeof(bb_elm_t));
				if (bbnext == NULL) abort();
				bbnext->addr = i + sizeof(arm_instr_t);

				hashtable_store(bb_ht,
						(hashtable_elm_t *)bbnext,
						&bbnext->addr,
						sizeof(unsigned int));
			}


			/* basic block for branch target */
			bb_elm_t *bbtarget = malloc(sizeof(bb_elm_t));
			if (bbtarget == NULL) abort();
			bbtarget->addr = target;

			hashtable_store(bb_ht,
					(hashtable_elm_t *)bbtarget,
					&bbtarget->addr, sizeof(unsigned int));

			/* reference */
			ref_elm_t *ref = malloc(sizeof(ref_elm_t));
			if (ref == NULL) abort();

			ref->source = i;
			ref->target = target;
			ref->cond = (cond != ARM_COND_AL);
			ref->link = link;

			hashtable_store(&ref_ht, (hashtable_elm_t *)ref,
					&ref->target, sizeof(unsigned int));
		} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_IMM) {
			int rd = arm_instr_get_param(instr, ip, 4);
			if (rd == 15) {
				bb_elm_t *bbnext = malloc(sizeof(bb_elm_t));
				if (bbnext == NULL) abort();
				bbnext->addr = i + sizeof(arm_instr_t);

				hashtable_store(bb_ht,
						(hashtable_elm_t *)bbnext,
						&bbnext->addr,
						sizeof(unsigned int));
			}
		} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
			int load = arm_instr_get_param(instr, ip, 5);
			int reglist = arm_instr_get_param(instr, ip, 7);
			if (load && (reglist & (1 << 15))) {
				bb_elm_t *bbnext = malloc(sizeof(bb_elm_t));
				if (bbnext == NULL) abort();
				bbnext->addr = i + sizeof(arm_instr_t);

				hashtable_store(bb_ht,
						(hashtable_elm_t *)bbnext,
						&bbnext->addr,
						sizeof(unsigned int));
			}
		}

		i += sizeof(arm_instr_t);
	}

	return 0;
}


int
main(int argc, char *argv[])
{
	int r, i;

	if (argc < 2) return EXIT_FAILURE;

	FILE *bin_file = fopen(argv[1], "rb");
	if (bin_file == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}


	/* read annotations */
	if (argc >= 3) {
		FILE *annot_file = fopen(argv[2], "r");
		if (annot_file == NULL) {
			perror("fopen");
			exit(EXIT_FAILURE);
		}

		r = hashtable_init(&annot_ht, 256);
		if (r < 0) {
			perror("hashtable_init");
			exit(EXIT_FAILURE);
		}

		r = annot_read(annot_file, &annot_ht);
		if (r < 0) {
			fprintf(stderr, "Unable to read annotations.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* basic block analysis */
	r = fseek(bin_file, 0, SEEK_SET);
	if (r < 0) {
		perror("fseeko");
		exit(EXIT_FAILURE);
	}

	r = hashtable_init(&bb_ht, 256);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	r = hashtable_init(&ref_ht, 256);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	r = basic_block_analysis(bin_file, &bb_ht);
	if (r < 0) {
		fprintf(stderr, "Unable to finish basic block analysis.\n");
		exit(EXIT_FAILURE);
	}

	/* print instructions */
	r = fseek(bin_file, 0, SEEK_SET);
	if (r < 0) {
		perror("fseeko");
		exit(EXIT_FAILURE);
	}

	i = 0;
	while (1) {
		arm_instr_t instr;
		r = fread(&instr, sizeof(arm_instr_t), 1, bin_file);
		if (r < 1) {
			if (feof(bin_file)) break;
			perror("fread");
			exit(EXIT_FAILURE);
		}

		instr = htobe32(instr);

		int bb_begin = 0;
		int ref_begin = 0;
		while (1) {
			/* basic block */
			bb_elm_t *bb = (bb_elm_t *)
				hashtable_lookup(&bb_ht, &i,
						 sizeof(unsigned int));
			if (bb == NULL) break;

			if (!bb_begin) {
				bb_begin = 1;
				if (i > 0) printf("\n");
			}

			list_elm_remove((list_elm_t *)bb);
			free(bb);
		}

		while (1) {
			/* references */
			ref_elm_t *ref = (ref_elm_t *)
				hashtable_lookup(&ref_ht, &i,
						 sizeof(unsigned int));
			if (ref == NULL) break;

			if (!ref_begin) {
				ref_begin = 1;
				printf("; reference from");
			} else printf(",");

			char *sourcestr = addr_string(ref->source);
			if (sourcestr == NULL) abort();

			printf(" %s(%s)%s", sourcestr,
			       (ref->cond ? "C" : "U"),
			       (ref->link ? "L" : ""));

			list_elm_remove((list_elm_t *)ref);
			free(ref);
		}
		if (ref_begin) printf("\n");

		list_t post_annot_list;
		list_init(&post_annot_list);
		while (1) {
			/* pre annotations */
			annot_elm_t *annot = (annot_elm_t *)
				hashtable_lookup(&annot_ht, &i,
						 sizeof(unsigned int));
			if (annot == NULL) break;

			list_elm_remove((list_elm_t *)annot);

			if (annot->pre) {
				printf("; %s", annot->line);
				free(annot->line);
				free(annot);
			} else {
				list_append(&post_annot_list,
					    (list_elm_t *)annot);
			}
		}

		/* instruction */
		printf("%08x\t%08x\t", i, instr);
		arm_instr_fprint(stdout, instr, i, addr_string);

		while (!list_is_empty(&post_annot_list)) {
			/* post annotations */
			annot_elm_t *annot = (annot_elm_t *)
				list_remove_head(&post_annot_list);

			printf("; %s", annot->line);
			free(annot->line);
			free(annot);
		}

		i += sizeof(arm_instr_t);
	}


#if 0
	/* inter translate instructions */
	r = fseek(bin_file, 0, SEEK_SET);
	if (r < 0) {
		perror("fseeko");
		exit(EXIT_FAILURE);
	}

	inter_context_t *ctx = inter_new_context();

	i = 0;
	while (1) {
		arm_instr_t instr;
		r = fread(&instr, sizeof(arm_instr_t), 1, bin_file);
		if (r < 1) {
			if (feof(bin_file)) break;
			perror("fread");
			exit(EXIT_FAILURE);
		}

		instr = htobe32(instr);
		inter_arm_append(ctx, instr, i);

		i += sizeof(arm_instr_t);
	}

	inter_fprint(stdout, ctx);
#endif

	return EXIT_SUCCESS;
}
