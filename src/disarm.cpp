/* disarm.cpp */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cctype>
#include <cerrno>

#include <iostream>
#include <iomanip>

#include "arm.hh"
#include "basicblock.hh"
#include "codesep.hh"
#include "endian.h"
#include "entrypoint.hh"
#include "hashtable.hh"
#include "image.hh"
#include "list.hh"
#include "symbol.hh"
#include "types.hh"

using namespace std;


typedef struct {
	list_elm_t elm;
	image_t *image;
	arm_addr_t addr;
	uint_t size;
} image_mapping_t;


#define BLOCK_SEPARATOR  \
  "; --------------------------------------------------------------------"


static char *
addr_string(arm_addr_t addr, void *data)
{
	char *addrstr = NULL;
	int r;

	hashtable_t *sym_ht = (hashtable_t *)data;

	sym_elm_t *sym = (sym_elm_t *)hashtable_lookup(sym_ht, &addr,
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
image_for_addr(list_t *img_map, uint_t *addr)
{
	list_elm_t *elm;
	list_foreach(img_map, elm) {
		image_mapping_t *imap_elm = (image_mapping_t *)elm;
		if (*addr >= imap_elm->addr &&
		    *addr < imap_elm->addr + imap_elm->size) {
			*addr -= imap_elm->addr;
			return imap_elm->image;
		}
	}

	return NULL;
}

static int
entry_point_add(list_t *ep_list, uint_t addr)
{
	int r;

	ep_elm_t *ep = (ep_elm_t *)malloc(sizeof(ep_elm_t));
	if (ep == NULL) {
		errno = ENOMEM;
		return -1;
	}

	ep->addr = addr;

	list_prepend(ep_list, (list_elm_t *)ep);

	return 0;
}


int
main(int argc, char *argv[])
{
	int r;

	if (argc < 2) return EXIT_FAILURE;

	/* load file */
	image_t *image = image_new(&argv[1][1], (argv[1][0] == 'B' ? 1 : 0));
	if (image == NULL) {
		cerr << "Unable to open file: `"
		     << &argv[1][1] << "'." << endl;
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
	list_t img_map;
	list_init(&img_map);
	image_mapping_t *imap_elm =
		(image_mapping_t *)malloc(sizeof(image_mapping_t));
	imap_elm->image = image;
	imap_elm->addr = 0;
	imap_elm->size = image->size;
	list_prepend(&img_map, (list_elm_t *)imap_elm);

	/* add arm entry points */
	list_t ep_list;
	list_init(&ep_list);
	for (int i = 0; i < 0x20; i += 4) {
		if (i == 0x14) continue;
		r = entry_point_add(&ep_list, i);
		if (r < 0) {
			perror("entry_point_add");
			exit(EXIT_FAILURE);
		}
	}

	/* load symbols */
	hashtable_t sym_ht;
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

		r = symbol_add_from_file(&sym_ht, symbol_file);
		if (r < 0) {
			perror("symbol_add_from_file");
			exit(EXIT_FAILURE);
		}

		fclose(symbol_file);
	}

	/* code/data separation */
	FILE *jump_file = NULL;
	if (argc >= 5) {
		jump_file = fopen(argv[4], "r");
		if (jump_file == NULL) {
			perror("fopen");
			exit(EXIT_FAILURE);
		}
	}

	uint8_t *image_codemap;
	r = codesep_analysis(&ep_list, image, &image_codemap, jump_file);
	if (r < 0) {
		perror("code_data_separate");
		exit(EXIT_FAILURE);
	}

	if (jump_file) fclose(jump_file);

	FILE *codemap_out = fopen("code_bitmap", "wb");
	if (codemap_out == NULL) return -1;
	size_t write = fwrite(image_codemap, sizeof(uint8_t),
			      image->size >> 2, codemap_out);
	if (write < (image->size >> 2)) {
		cerr << "Unable to write codemap." << endl;
	}
	fclose(codemap_out);

	/* basic block analysis */
	hashtable_t bb_ht;
	r = hashtable_init(&bb_ht, 0);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	hashtable_t reflist_ht;
	r = hashtable_init(&reflist_ht, 0);
	if (r < 0) {
		perror("hashtable_init");
		exit(EXIT_FAILURE);
	}

	r = basicblock_analysis(&bb_ht, &sym_ht, &reflist_ht, image,
				image_codemap);
	if (r < 0) {
		cerr << "Unable to finish basic block analysis." << endl;
		exit(EXIT_FAILURE);
	}

	/* print instructions */
	uint_t i = 0;
	while (1) {
		arm_instr_t instr;
		r = image_read_instr(image, i, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		int refs_printed = 0;

		/* basic block */
		bb_elm_t *bb = (bb_elm_t *)
			hashtable_lookup(&bb_ht, &i, sizeof(uint_t));
		if (bb != NULL && i > 0) {
			cout << endl << BLOCK_SEPARATOR << endl;

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
				if (refs_printed == 0) {
					cout << "; reference from ";
				} else if (refs_printed % 4 == 0) {
					cout << "," << endl << ";\t\t ";
				} else {
					cout << ", ";
				}

				char *sourcestr = addr_string(ref->source,
							      &sym_ht);
				if (sourcestr == NULL) abort();

				cout << sourcestr
				     << "(" << (ref->cond ? "C" : "U") << ")"
				     << (ref->link ? "L" :
					 ((i > ref->source) ? "F" : "B"));
				refs_printed += 1;

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
		if (refs_printed > 0) cout << endl;

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
				while (*text != '\0') {
					char *nl = static_cast<char *>(
						memchr(text, '\n', textlen));
					if (nl == NULL || nl - text == 0) {
						break;
					}

					*nl = '\0';
					cout << "; " << text << endl;
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
		if (sym != NULL) cout << "; " << sym->name << ":" << endl;

		/* instruction */
		cout << hex << setw(8) << setfill('0') << i << "\t"
		     << hex << setw(8) << setfill('0') << instr << "\t";
		arm_instr_fprint(stdout, instr, i, addr_string, &sym_ht);

		while (!list_is_empty(&post_annot_list)) {
			/* post annotations */
			annot_elm_t *annot = (annot_elm_t *)
				list_remove_head(&post_annot_list);

			char *text = annot->text;
			size_t textlen = annot->textlen;
			while (*text != '\0') {
				char *nl = static_cast<char *>(
					memchr(text, '\n', textlen));
				if (nl == NULL || nl - text == 0) break;

				*nl = '\0';
				cout << "; " << text << endl;
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