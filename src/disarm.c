/* disarm.c */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include "arm.h"
#include "codesep.h"
#include "endian.h"
#include "entrypoint.h"
#include "hashtable.h"
#include "image.h"
#include "list.h"


typedef struct {
	hashtable_elm_t elm;
	arm_addr_t target;
	list_t refs;
} reflist_elm_t;

typedef struct {
	list_elm_t elm;
	arm_addr_t source;
	int cond;
	int link;
} ref_elm_t;

typedef struct {
	hashtable_elm_t elm;
	arm_addr_t addr;
} bb_elm_t;

typedef struct {
	list_elm_t elm;
	image_t *image;
	arm_addr_t addr;
	uint_t size;
} image_mapping_t;

typedef struct {
	hashtable_elm_t elm;
	arm_addr_t addr;
	char *name;
} sym_elm_t;


static hashtable_t reflist_ht;
static hashtable_t bb_ht;
static list_t img_map = LIST_INIT(img_map);
static list_t ep_list = LIST_INIT(ep_list);
static hashtable_t sym_ht;


static char *
addr_string(arm_addr_t addr)
{
	char *addrstr = NULL;
	int r;

	sym_elm_t *sym = (sym_elm_t *)hashtable_lookup(&sym_ht, &addr,
						       sizeof(arm_addr_t));
	if (sym != NULL) {
		static const char *format = "<%s>";

		r = snprintf(NULL, 0, format, sym->name);
		if (r > 0) {
			addrstr = (char *)malloc((r+1)*sizeof(char));
			if (addrstr == NULL) abort();
			r = snprintf(addrstr, r+1, format, sym->name);
		}
	} else {
		static const char *format = "0x%x";

		r = snprintf(NULL, 0, format, addr);
		if (r > 0) {
			addrstr = (char *)malloc((r+1)*sizeof(char));
			if (addrstr == NULL) abort();
			r = snprintf(addrstr, r+1, format, addr);
		}
	}

	if (r <= 0) {
		if (addrstr != NULL) free(addrstr);
		return NULL;
	}

	return addrstr;
}

static image_t *
image_for_addr(uint_t *addr)
{
	list_elm_t *elm;
	list_foreach(&img_map, elm) {
		image_mapping_t *imap_elm = (image_mapping_t *)elm;
		if (*addr >= imap_elm->addr &&
		    *addr < imap_elm->addr + imap_elm->size) {
			*addr -= imap_elm->addr;
			return imap_elm->image;
		}
	}

	return NULL;
}

static void
symbol_add(arm_addr_t addr, const char *name)
{
	int r;

	sym_elm_t *sym = malloc(sizeof(sym_elm_t));
	if (sym == NULL) abort();

	sym->addr = addr;
	sym->name = strdup(name);
	if (sym->name == NULL) abort();

	hashtable_elm_t *old;
	r = hashtable_insert(&sym_ht, (hashtable_elm_t *)sym,
			     &sym->addr, sizeof(arm_addr_t), &old);
	if (r < 0) {
		perror("hashtable_insert");
		exit(EXIT_FAILURE);
	}
	if (old != NULL) free(old);
}

static int
symbol_add_from_file(FILE *f)
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

			while (*endptr != '\0' && isblank(*endptr)) {
				endptr += 1;
			}

			if (*endptr == '\0' || *endptr == '\n') continue;

			char *nlptr = strchr(endptr, '\n');
			if (nlptr != NULL) *nlptr = '\0';

			symbol_add(addr, endptr);
		}
	}

	if (text) free(text);

	return 0;
}

static void
reference_add(arm_addr_t source, arm_addr_t target, int cond, int link)
{
	int r;

	ref_elm_t *ref = malloc(sizeof(ref_elm_t));
	if (ref == NULL) abort();

	ref->source = source;
	ref->cond = cond;
	ref->link = link;

	reflist_elm_t *reflist = (reflist_elm_t *)
		hashtable_lookup(&reflist_ht, &target, sizeof(arm_addr_t));
	if (reflist == NULL) {
		reflist = malloc(sizeof(reflist_elm_t));
		if (reflist == NULL) abort();
		reflist->target = target;
		list_init(&reflist->refs);

		hashtable_elm_t *old;
		r = hashtable_insert(&reflist_ht, (hashtable_elm_t *)reflist,
				     &reflist->target, sizeof(uint_t), &old);
		if (r < 0) {
			perror("hashtable_insert");
			exit(EXIT_FAILURE);
		}
		if (old != NULL) free(old);
	}

	list_append(&reflist->refs, (list_elm_t *)ref);

	if (link) {
		sym_elm_t *sym = (sym_elm_t *)
			hashtable_lookup(&sym_ht, &target, sizeof(arm_addr_t));
		if (sym == NULL) {
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

			symbol_add(target, funname);
			free(funname);
		}
	}
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

		hashtable_elm_t *old;
		r = hashtable_insert(&bb_ht, (hashtable_elm_t *)bb,
				     &bb->addr, sizeof(uint_t), &old);
		if (r < 0) {
			perror("hashtable_insert");
			exit(EXIT_FAILURE);
		}
		if (old != NULL) free(old);
	}
}

static int
basic_block_analysis(image_t *image)
{
	int r;

	bb_elm_t *entry_point = malloc(sizeof(bb_elm_t));
	if (entry_point == NULL) abort();
	entry_point->addr = 0x0;

	hashtable_elm_t *old;
	r = hashtable_insert(&bb_ht, (hashtable_elm_t *)entry_point,
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

		const arm_instr_pattern_t *ip =
			arm_instr_get_instr_pattern(instr);
		if (ip == NULL) {
			i += sizeof(arm_instr_t);
			continue;
		}

		if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
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
			reference_add(i, target, (cond != ARM_COND_AL), link);
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

static int
entry_point_add(uint_t addr)
{
	int r;

	ep_elm_t *ep = (ep_elm_t *)malloc(sizeof(ep_elm_t));
	if (ep == NULL) {
		errno = ENOMEM;
		return -1;
	}

	ep->addr = addr;

	list_prepend(&ep_list, (list_elm_t *)ep);

	return 0;
}


int
main(int argc, char *argv[])
{
	int r;

	if (argc < 2) return EXIT_FAILURE;

	/* load file */
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

		fclose(annot_file);
	}

	/* create address space mapping */
	image_mapping_t *imap_elm =
		(image_mapping_t *)malloc(sizeof(image_mapping_t));
	imap_elm->image = image;
	imap_elm->addr = 0;
	imap_elm->size = image->size;
	list_prepend(&img_map, (list_elm_t *)imap_elm);

	/* add arm entry points */
	for (int i = 0; i < 0x20; i += 4) {
		if (i == 0x14) continue;
		r = entry_point_add(i);
		if (r < 0) {
			perror("entry_point_add");
			exit(EXIT_FAILURE);
		}
	}

	/* load symbols */
	r = hashtable_init(&sym_ht, 0);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	if (argc >= 4) {
		FILE *symbol_file = fopen(argv[3], "r");
		if (symbol_file == NULL) {
			perror("fopen");
			exit(EXIT_FAILURE);
		}

		r = symbol_add_from_file(symbol_file);
		if (r < 0) {
			perror("symbol_add_from_file");
			exit(EXIT_FAILURE);
		}

		fclose(symbol_file);
	}

#if 0
	r = code_data_separate(&ep_list, image, NULL);
	if (r < 0) {
		perror("code_data_separate");
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
#endif

	/* basic block analysis */
	r = hashtable_init(&bb_ht, 0);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	r = hashtable_init(&reflist_ht, 0);
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
	uint_t i = 0;
	while (1) {
		arm_instr_t instr;
		r = image_read_instr(image, i, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		int ref_begin = 0;

		/* basic block */
		bb_elm_t *bb = (bb_elm_t *)
			hashtable_lookup(&bb_ht, &i, sizeof(uint_t));
		if (bb != NULL && i > 0) {
			printf("\n");

			r = hashtable_remove(&bb_ht, (hashtable_elm_t *)bb);
			if (r < 0) {
				perror("hashtable_remove");
				exit(EXIT_FAILURE);
			}

			free(bb);
		}

		/* references */
		reflist_elm_t *reflist = (reflist_elm_t *)
			hashtable_lookup(&reflist_ht, &i, sizeof(uint_t));
		if (reflist != NULL) {
			while (!list_is_empty(&reflist->refs)) {
				ref_elm_t *ref = (ref_elm_t *)
					list_remove_head(&reflist->refs);
				if (!ref_begin) {
					ref_begin = 1;
					printf("; reference from");
				} else printf(",");

				char *sourcestr = addr_string(ref->source);
				if (sourcestr == NULL) abort();

				printf(" %s(%s)%s", sourcestr,
				       (ref->cond ? "C" : "U"),
				       (ref->link ? "L" : ""));

				free(sourcestr);

				free(ref);
			}

			r = hashtable_remove(&reflist_ht,
					     (hashtable_elm_t *)reflist);
			if (r < 0) {
				perror("hashtable_remove");
				exit(EXIT_FAILURE);
			}

			free(reflist);
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

		/* symbol */
		sym_elm_t *sym = (sym_elm_t *)
			hashtable_lookup(&sym_ht, &i, sizeof(arm_addr_t));
		if (sym != NULL) printf("; %s:\n", sym->name);

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
	hashtable_deinit(&reflist_ht);
	hashtable_deinit(&sym_ht);


	return EXIT_SUCCESS;
}
