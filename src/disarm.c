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


typedef unsigned int uint_t;


typedef struct {
	hashtable_elm_t elm;
	uint_t addr;
	int pre;
	char *line;
	size_t linelen;
} annot_elm_t;

typedef struct {
	hashtable_elm_t elm;
	uint_t target;
	uint_t source;
	int cond;
	int link;
} ref_elm_t;

typedef struct {
	hashtable_elm_t elm;
	uint_t addr;
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
	uint_t addr;
	size_t size;
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
addr_string(uint_t addr)
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
annot_read(FILE *f)
{
	int r;
	char *line = NULL;
	size_t linelen = 0;
	ssize_t read;

	uint_t addr = 0;
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
			annot_elm_t *annot = (annot_elm_t *)
				hashtable_lookup(&annot_ht, &addr,
						 sizeof(uint_t));

			if (annot == NULL || annot->pre != pre) {
				annot = (annot_elm_t *)
					malloc(sizeof(annot_elm_t));
				if (annot == NULL) abort();

				annot->addr = addr;
				annot->pre = pre;
				annot->line = line;
				annot->linelen = read;

				line = NULL;

				hashtable_store(&annot_ht,
						(hashtable_elm_t *)annot,
						&annot->addr, sizeof(uint_t));
			} else {
				annot->line =
					realloc(annot->line,
						annot->linelen + read + 1);
				if (annot->line == NULL) abort();

				memcpy(&annot->line[annot->linelen],
				       line, read);
				annot->linelen += read;
				annot->line[annot->linelen] = '\0';
			}
		}
	}

	if (line) free(line);

	return 0;
}

static void
basic_block_add(uint_t addr)
{
	hashtable_elm_t *helm = hashtable_lookup(&bb_ht,
						 &addr, sizeof(uint_t));
	if (helm == NULL) {
		bb_elm_t *bb = malloc(sizeof(bb_elm_t));
		if (bb == NULL) abort();
		bb->addr = addr;

		hashtable_store(&bb_ht, (hashtable_elm_t *)bb,
				&bb->addr, sizeof(uint_t));
	}
}

static int
basic_block_analysis(FILE *f)
{
	int r, i;

	bb_elm_t *entry_point = malloc(sizeof(bb_elm_t));
	if (entry_point == NULL) abort();
	entry_point->addr = 0x0;
	hashtable_store(&bb_ht, (hashtable_elm_t *)entry_point,
			&entry_point->addr, sizeof(uint_t));

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
			basic_block_add(i + sizeof(arm_instr_t));

			/* basic block for branch target */
			basic_block_add(target);

			/* reference */
			ref_elm_t *ref = malloc(sizeof(ref_elm_t));
			if (ref == NULL) abort();

			ref->source = i;
			ref->target = target;
			ref->cond = (cond != ARM_COND_AL);
			ref->link = link;

			hashtable_store(&ref_ht, (hashtable_elm_t *)ref,
					&ref->target, sizeof(uint_t));
		} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
			   ip->type == ARM_INSTR_TYPE_DATA_IMM) {
			int rd = arm_instr_get_param(instr, ip, 4);
			if (rd == 15) basic_block_add(i + sizeof(arm_instr_t));
		} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
			int load = arm_instr_get_param(instr, ip, 5);
			int reglist = arm_instr_get_param(instr, ip, 7);
			if (load && (reglist & (1 << 15))) {
				basic_block_add(i + sizeof(arm_instr_t));
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
	r = hashtable_init(&annot_ht, 1024);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	if (argc >= 3) {
		FILE *annot_file = fopen(argv[2], "r");
		if (annot_file == NULL) {
			perror("fopen");
			exit(EXIT_FAILURE);
		}

		r = annot_read(annot_file);
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

	r = hashtable_init(&bb_ht, 1024);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	r = hashtable_init(&ref_ht, 1024);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	r = basic_block_analysis(bin_file);
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
				hashtable_lookup(&bb_ht, &i, sizeof(uint_t));
			if (bb == NULL) break;

			if (!bb_begin) {
				bb_begin = 1;
				if (i > 0) printf("\n");
			}

			hashtable_remove_elm(&bb_ht, (hashtable_elm_t *)bb);
			free(bb);
		}

		while (1) {
			/* references */
			ref_elm_t *ref = (ref_elm_t *)
				hashtable_lookup(&ref_ht, &i, sizeof(uint_t));
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

			hashtable_remove_elm(&ref_ht, (hashtable_elm_t *)ref);
			free(ref);
		}
		if (ref_begin) printf("\n");

		list_t post_annot_list;
		list_init(&post_annot_list);
		while (1) {
			/* pre annotations */
			annot_elm_t *annot = (annot_elm_t *)
				hashtable_lookup(&annot_ht, &i,
						 sizeof(uint_t));
			if (annot == NULL) break;

			hashtable_remove_elm(&annot_ht,
					     (hashtable_elm_t *)annot);

			if (annot->pre) {
				char *line = annot->line;
				size_t linelen = annot->linelen;
				while (1) {
					char *nl = memchr(line, '\n',
							  linelen);
					if (nl == NULL) break;
					printf("; %.*s\n", nl - line, line);
					linelen -= nl - line - 1;
					line = nl + 1;
				}
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

			char *line = annot->line;
			size_t linelen = annot->linelen;
			while (1) {
				char *nl = memchr(line, '\n', linelen);
				if (nl == NULL) break;
				printf("; %.*s\n", nl - line, line);
				linelen -= nl - line - 1;
				line = nl + 1;
			}
			free(annot->line);
			free(annot);
		}

		i += sizeof(arm_instr_t);
	}

	/* clean up */
	hashtable_deinit(&annot_ht);
	hashtable_deinit(&bb_ht);
	hashtable_deinit(&ref_ht);

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
