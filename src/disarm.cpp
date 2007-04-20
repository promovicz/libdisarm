/* disarm.cpp */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cctype>
#include <cerrno>

#include <iostream>
#include <iomanip>
#include <list>
#include <map>

#include "arm.hh"
#include "basicblock.hh"
#include "codesep.hh"
#include "endian.h"
#include "image.hh"
#include "symbol.hh"
#include "types.hh"

using namespace std;


typedef struct {
	image_t *image;
	arm_addr_t addr;
	uint_t size;
} image_mapping_t;


#define BLOCK_SEPARATOR  \
  "; --------------------------------------------------------------------"


static image_t *
image_for_addr(list<image_mapping_t *> *img_map, uint_t *addr)
{
	list<image_mapping_t *>::iterator iter;
	for (iter = img_map->begin(); iter != img_map->end(); iter++) {
		image_mapping_t *imap_elm = *iter;
		if (*addr >= imap_elm->addr &&
		    *addr < imap_elm->addr + imap_elm->size) {
			*addr -= imap_elm->addr;
			return imap_elm->image;
		}
	}

	return NULL;
}

static int
entry_point_add(list<arm_addr_t> *ep_list, arm_addr_t addr)
{
	ep_list->push_front(addr);

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
	list<image_mapping_t *> img_map;
	image_mapping_t *imap_elm = new image_mapping_t;
	imap_elm->image = image;
	imap_elm->addr = 0;
	imap_elm->size = image->size;
	img_map.push_back(imap_elm);

	/* add arm entry points */
	list<arm_addr_t> ep_list;
	for (arm_addr_t i = 0; i < 0x20; i += 4) {
		if (i == 0x14) continue;
		r = entry_point_add(&ep_list, i);
		if (r < 0) {
			perror("entry_point_add");
			exit(EXIT_FAILURE);
		}
	}

	/* load symbols */
	map<arm_addr_t, char *> sym_map;
	if (argc >= 4) {
		FILE *symbol_file = fopen(argv[3], "r");
		if (symbol_file == NULL) {
			perror("fopen");
			exit(EXIT_FAILURE);
		}

		r = symbol_add_from_file(&sym_map, symbol_file);
		if (r < 0) {
			perror("symbol_add_from_file");
			exit(EXIT_FAILURE);
		}

		fclose(symbol_file);
	}

	/* basic block analysis */
	map<arm_addr_t, bool> bb_map;
	map<arm_addr_t, list<ref_code_t *> *> coderefs_map;
	map<arm_addr_t, list<ref_data_t *> *> datarefs_map;

	r = bb_instr_analysis(&bb_map, &ep_list, &sym_map,
			      &coderefs_map, &datarefs_map, image);
	if (r < 0) {
		cerr << "Unable to finish basic block analysis." << endl;
		exit(EXIT_FAILURE);
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

	/* print instructions */
	uint_t i = 0;
	while (1) {
		arm_instr_t instr;
		r = image_read_instr(image, i, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		uint_t coderefs_printed = 0;
		uint_t datarefs_printed = 0;

		/* basic block */
		map<arm_addr_t, bool>::iterator bb_pos = bb_map.find(i);
		if (bb_pos != bb_map.end() && i > 0) {
			cout << endl << BLOCK_SEPARATOR << endl;
			bb_map.erase(bb_pos);
		}

		/* code references */
		map<arm_addr_t, list<ref_code_t *> *>::iterator coderefs_pos =
			coderefs_map.find(i);
		if (coderefs_pos != coderefs_map.end()) {
			list<ref_code_t *> *ref_list = (*coderefs_pos).second;
			while (!ref_list->empty()) {
				ref_code_t *ref = ref_list->front();
				ref_list->pop_front();

				if (coderefs_printed == 0) {
					cout << "; code reference from ";
				} else if (coderefs_printed % 4 == 0) {
					cout << "," << endl << ";\t\t ";
				} else {
					cout << ", ";
				}

				char *sourcestr = arm_addr_string(ref->source,
								  &sym_map);
				if (sourcestr == NULL) abort();

				cout << sourcestr
				     << "(" << (ref->cond ? "C" : "U") << ")"
				     << (ref->link ? "L" :
					 ((i > ref->source) ? "F" : "B"));
				coderefs_printed += 1;

				free(sourcestr);

				delete ref;
			}

			coderefs_map.erase(coderefs_pos);

			delete ref_list;
		}
		if (coderefs_printed > 0) cout << endl;

		/* data references */
		map<arm_addr_t, list<ref_data_t *> *>::iterator datarefs_pos =
			datarefs_map.find(i);
		if (datarefs_pos != datarefs_map.end()) {
			list<ref_data_t *> *ref_list = (*datarefs_pos).second;
			while (!ref_list->empty()) {
				ref_data_t *ref = ref_list->front();
				ref_list->pop_front();

				if (datarefs_printed == 0) {
					cout << "; data reference from ";
				} else if (datarefs_printed % 4 == 0) {
					cout << "," << endl << ";\t\t ";
				} else {
					cout << ", ";
				}

				char *sourcestr = arm_addr_string(ref->source,
								  &sym_map);
				if (sourcestr == NULL) abort();

				cout << sourcestr;
				datarefs_printed += 1;

				free(sourcestr);

				delete ref;
			}

			datarefs_map.erase(datarefs_pos);

			delete ref_list;
		}
		if (datarefs_printed > 0) cout << endl;

		/* find annotations */
		map<arm_addr_t, annot_t *>::iterator annot_pos =
			image->annot_map->find(i);
		annot_t *annot = NULL;
		if (annot_pos != image->annot_map->end()) {
			annot = (*annot_pos).second;
			image->annot_map->erase(annot_pos);
		}

		/* pre annotation */
		if (annot != NULL && annot->pre_text != NULL) {
			char *text = annot->pre_text;
			size_t textlen = annot->pre_textlen;
			while (*text != '\0') {
				char *nl = static_cast<char *>(
					memchr(text, '\n', textlen));
				if (nl == NULL || nl - text == 0) break;

				*nl = '\0';
				cout << "; " << text << endl;
				textlen -= nl - text - 1;
				text = nl + 1;
			}
			free(annot->pre_text);
		}

		/* symbol */
		map<arm_addr_t, char *>::iterator sym = sym_map.find(i);
		if (sym != sym_map.end()) {
			const char *sym_name = (*sym).second;
			cout << "; " << sym_name << ":" << endl;
		}

		/* instruction */
		cout << hex << setw(8) << setfill('0') << i << "\t"
		     << hex << setw(8) << setfill('0') << instr << "\t";
		arm_instr_fprint(stdout, instr, i, &sym_map);

		/* post annotation */
		if (annot != NULL && annot->post_text != NULL) {
			char *text = annot->post_text;
			size_t textlen = annot->post_textlen;
			while (*text != '\0') {
				char *nl = static_cast<char *>(
					memchr(text, '\n', textlen));
				if (nl == NULL || nl - text == 0) break;
				
				*nl = '\0';
				cout << "; " << text << endl;
				textlen -= nl - text - 1;
				text = nl + 1;
			}
			free(annot->post_text);
		}

		/* clean up annotations */
		if (annot != NULL) delete annot;

		i += sizeof(arm_instr_t);
	}

	/* clean up */
	image_free(image);
	delete imap_elm;

	/* clean up symbols */
	uint_t syms_cleaned = 0;
	for (map<arm_addr_t, char *>::iterator sym_iter = sym_map.begin();
	     sym_iter != sym_map.end(); sym_iter++) {
		char *sym_name = (*sym_iter).second;
		free(sym_name);
		syms_cleaned += 1;
	}

	cout << "Done. Cleaned: " << dec
	     << "syms: " << syms_cleaned << endl;

	return EXIT_SUCCESS;
}
