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
	char *text;
	size_t textlen;
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

typedef struct {
	list_elm_t elm;
	FILE *file;
	hashtable_t annot_ht;
} image_t;


static hashtable_t ref_ht;
static hashtable_t bb_ht;
static list_t image_list = LIST_INIT(image_list);


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
image_add_annot(image_t *image, uint_t addr,
		char *text, size_t textlen, int pre)
{
	int r;
	hashtable_t *annot_ht = &image->annot_ht;
	annot_elm_t *annot = (annot_elm_t *)
		hashtable_lookup(annot_ht, &addr, sizeof(uint_t));

	if (annot == NULL || annot->pre != pre) {
		annot = (annot_elm_t *)malloc(sizeof(annot_elm_t));
		if (annot == NULL) {
			errno = ENOMEM;
			return -1;
		}

		annot->addr = addr;
		annot->pre = pre;
		annot->text = text;
		annot->textlen = textlen;

		r = hashtable_insert(annot_ht, (hashtable_elm_t *)annot,
				     &annot->addr, sizeof(uint_t));
		if (r < 0) {
			perror("hashtable_insert");
			exit(EXIT_FAILURE);
		}
	} else {
		annot->text = realloc(annot->text,
				      annot->textlen + textlen + 1);
		if (annot->text == NULL) {
			errno = ENOMEM;
			return -1;
		}

		memcpy(&annot->text[annot->textlen], text, textlen);
		annot->textlen += textlen;
		annot->text[annot->textlen] = '\0';

		free(text);
	}

	return 0;
}

static int
image_add_annot_from_file(image_t *image, FILE *f)
{
	int r;
	char *text = NULL;
	size_t textlen = 0;
	ssize_t read;

	uint_t addr = 0;
	int reading_addr = 1;
	int pre = 1;

	hashtable_t *annot_ht = &image->annot_ht;

	while ((read = getline(&text, &textlen, f)) != -1) {
		if (read == 0 || !strcmp(text, "\n")) {
			reading_addr = 1;
		} else if (reading_addr) {
			errno = 0;
			char *endptr;
			addr = strtol(text, &endptr, 16);

			if ((errno == ERANGE &&
			     (addr == LONG_MAX || addr == LONG_MIN))
			    || (errno != 0 && addr == 0)
			    || (endptr == text)) {
				free(text);
				fprintf(stderr, "Unable to parse address.\n");
				return -1;
			}

			reading_addr = 0;
			pre = 1;
		} else if (!strcmp(text, "--\n")) {
			pre = 0;
		} else {
			r = image_add_annot(image, addr, text, read, pre);
			if (r < 0) {
				if (errno == ENOMEM) abort();
				else {
					perror("image_add_annot");
					return -1;
				}
			}
			text = NULL;
		}
	}

	if (text) free(text);

	return 0;
}

static void
basic_block_add(uint_t addr)
{
	int r;
	hashtable_elm_t *helm = hashtable_lookup(&bb_ht,
						 &addr, sizeof(uint_t));
	if (helm == NULL) {
		bb_elm_t *bb = malloc(sizeof(bb_elm_t));
		if (bb == NULL) abort();
		bb->addr = addr;

		r = hashtable_insert(&bb_ht, (hashtable_elm_t *)bb,
				     &bb->addr, sizeof(uint_t));
		if (r < 0) {
			perror("hashtable_insert");
			exit(EXIT_FAILURE);
		}
	}
}

static int
basic_block_analysis(image_t *image)
{
	int r, i;

	r = fseeko(image->file, 0, SEEK_SET);
	if (r < 0) {
		perror("fseeko");
		exit(EXIT_FAILURE);
	}

	bb_elm_t *entry_point = malloc(sizeof(bb_elm_t));
	if (entry_point == NULL) abort();
	entry_point->addr = 0x0;
	r = hashtable_insert(&bb_ht, (hashtable_elm_t *)entry_point,
			     &entry_point->addr, sizeof(uint_t));
	if (r < 0) {
		perror("hashtable_insert");
		exit(EXIT_FAILURE);
	}

	i = 0;
	while (1) {
		arm_instr_t instr;
		r = fread(&instr, sizeof(arm_instr_t), 1, image->file);
		if (r < 1) {
			if (feof(image->file)) break;
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

			r = hashtable_insert(&ref_ht, (hashtable_elm_t *)ref,
					     &ref->target, sizeof(uint_t));
			if (r < 0) {
				perror("hashtable_insert");
				exit(EXIT_FAILURE);
			}
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

static image_t *
image_new(const char *filename)
{
	int r;

	image_t *image = (image_t *)malloc(sizeof(image_t));
	if (image == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	image->file = fopen(filename, "rb");
	if (image->file == NULL) return NULL;

	r = hashtable_init(&image->annot_ht, 0);
	if (r < 0) return NULL;

	return image;
}

static void
image_free(image_t *image)
{
	hashtable_deinit(&image->annot_ht);
	fclose(image->file);
	free(image);
}


int
main(int argc, char *argv[])
{
	int r, i;

	if (argc < 2) return EXIT_FAILURE;

	image_t *image = image_new(argv[1]);
	if (image == NULL) {
		perror("image_new");
		exit(EXIT_FAILURE);
	}

	/* read annotations */
	if (argc >= 3) {
		FILE *annot_file = fopen(argv[2], "r");
		if (annot_file == NULL) {
			perror("fopen");
			exit(EXIT_FAILURE);
		}

		r = image_add_annot_from_file(image, annot_file);
		if (r < 0) {
			perror("image_add_annot_from_file");
			exit(EXIT_FAILURE);
		}
	}

	/* basic block analysis */
	r = hashtable_init(&bb_ht, 0);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	r = hashtable_init(&ref_ht, 0);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	r = basic_block_analysis(image);
	if (r < 0) {
		fprintf(stderr, "Unable to finish basic block analysis.\n");
		exit(EXIT_FAILURE);
	}

	/* print instructions */
	r = fseeko(image->file, 0, SEEK_SET);
	if (r < 0) {
		perror("fseeko");
		exit(EXIT_FAILURE);
	}

	i = 0;
	while (1) {
		arm_instr_t instr;
		r = fread(&instr, sizeof(arm_instr_t), 1, image->file);
		if (r < 1) {
			if (feof(image->file)) break;
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

			r = hashtable_remove(&bb_ht, (hashtable_elm_t *)bb);
			if (r < 0) {
				perror("hashtable_remove");
				exit(EXIT_FAILURE);
			}

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

			r = hashtable_remove(&ref_ht, (hashtable_elm_t *)ref);
			if (r < 0) {
				perror("hashtable_remove");
				exit(EXIT_FAILURE);
			}

			free(ref);
		}
		if (ref_begin) printf("\n");

		list_t post_annot_list;
		list_init(&post_annot_list);
		while (1) {
			/* pre annotations */
			annot_elm_t *annot = (annot_elm_t *)
				hashtable_lookup(&image->annot_ht, &i,
						 sizeof(uint_t));
			if (annot == NULL) break;

			r = hashtable_remove(&image->annot_ht,
					     (hashtable_elm_t *)annot);
			if (r < 0) {
				perror("hashtable_remove");
				exit(EXIT_FAILURE);
			}

			if (annot->pre) {
				char *text = annot->text;
				size_t textlen = annot->textlen;
				while (1) {
					char *nl = memchr(text, '\n',
							  textlen);
					if (nl == NULL) break;
					printf("; %.*s\n", nl - text, text);
					textlen -= nl - text - 1;
					text = nl + 1;
				}
				free(annot->text);
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

			char *text = annot->text;
			size_t textlen = annot->textlen;
			while (1) {
				char *nl = memchr(text, '\n', textlen);
				if (nl == NULL) break;
				printf("; %.*s\n", nl - text, text);
				textlen -= nl - text - 1;
				text = nl + 1;
			}
			free(annot->text);
			free(annot);
		}

		i += sizeof(arm_instr_t);
	}

	/* clean up */
	image_free(image);
	hashtable_deinit(&bb_ht);
	hashtable_deinit(&ref_ht);


	return EXIT_SUCCESS;
}
