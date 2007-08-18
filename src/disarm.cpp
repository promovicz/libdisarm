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
#include "endian.hh"
#include "image.hh"
#include "symbol.hh"
#include "types.hh"

using namespace std;


#define BLOCK_SEPARATOR  \
  "; --------------------------------------------------------------------"

static char
get_print_char(uint8_t data)
{
	if (data >= 32 && data <= 126) return data;
	else return '.';
}

static void
print_data(uint32_t data)
{
	cout << hex << setw(2) << setfill('0')
	     << ((data & 0xff000000) >> 24) << " "
	     << hex << setw(2) << setfill('0')
	     << ((data & 0xff0000) >> 16) << " "
	     << hex << setw(2) << setfill('0')
	     << ((data & 0xff00) >> 8) << " "
	     << hex << setw(2) << setfill('0')
	     << (data & 0xff) << "\t|"
	     << get_print_char((data & 0xff000000) >> 24)
	     << get_print_char((data & 0xff0000) >> 16)
	     << get_print_char((data & 0xff00) >> 8)
	     << get_print_char(data & 0xff)
	     << "|" << endl;
}

static int
entry_point_add(list<arm_addr_t> *ep_list, arm_addr_t addr)
{
	ep_list->push_front(addr);

	return 0;
}

static void
print_code_references(basic_block_t *bb, map<arm_addr_t, char *> *sym_map)
{
	uint_t coderefs_printed = 0;

	/* code references */
	multimap<basic_block_t *, ref_code_t *>::iterator coderefs_iter;
	coderefs_iter = bb->c.in_refs.begin();
	while (coderefs_iter != bb->c.in_refs.end()) {
		ref_code_t *ref = coderefs_iter->second;

		if (ref->remove) {
			delete ref;
			coderefs_iter++;
			continue;
		}

		if (coderefs_printed == 0) {
			cout << "; code reference from ";
		} else if (coderefs_printed % 4 == 0) {
			cout << "," << endl << ";\t\t ";
		} else {
			cout << ", ";
		}

		char *sourcestr = arm_addr_string(ref->source, sym_map);
		if (sourcestr == NULL) abort();

		cout << sourcestr << "(" << (ref->cond ? "C" : "U") << ")"
		     << (ref->link ? "L" :
			 ((bb->addr > ref->source) ? "F" : "B"));
		coderefs_printed += 1;

		free(sourcestr);

		coderefs_iter++;
	}
	if (coderefs_printed > 0) cout << endl;
}

static void
print_data_references(basic_block_t *bb, map<arm_addr_t, char *> *sym_map)
{
	uint_t datarefs_printed = 0;

	multimap<basic_block_t *, ref_data_t *>::iterator datarefs_iter;
	datarefs_iter = bb->d.data_refs.begin();
	while (datarefs_iter != bb->d.data_refs.end()) {
		ref_data_t *ref = datarefs_iter->second;

		if (ref->remove) {
			delete ref;
			datarefs_iter++;
			continue;
		}

		if (datarefs_printed == 0) {
			cout << "; data reference from ";
		} else if (datarefs_printed % 2 == 0) {
			cout << "," << endl << ";\t\t ";
		} else {
			cout << ", ";
		}

		char *sourcestr = arm_addr_string(ref->source, sym_map);
		if (sourcestr == NULL) abort();

		char *targetstr = arm_addr_string(ref->target, sym_map);
		if (targetstr == NULL) abort();

		cout << sourcestr << "(" << targetstr << "("
		     << ref->size << "))";
		datarefs_printed += 1;

		free(sourcestr);
		free(targetstr);

		datarefs_iter++;
	}
	if (datarefs_printed > 0) cout << endl;
}

static int
print_basic_block_code(basic_block_t *bb, image_t *image,
		       map<arm_addr_t, char *> *sym_map)
{
	int r;

	arm_instr_t instr;
	r = image_read_word(image, bb->addr, &instr);
	if (r == 0) return 0;

	cout << endl
	     << BLOCK_SEPARATOR << endl;

	/* reg use/change analysis */
	uint_t bb_use_regs = 0;
	uint_t bb_change_regs = 0;
	uint_t bb_use_flags = 0;
	uint_t bb_change_flags = 0;

	arm_addr_t addr;

	addr = bb->addr + bb->size;
	while (addr > bb->addr) {
		addr -= sizeof(arm_addr_t);

		arm_instr_t instr;
		r = image_read_word(image, addr, &instr);
		if (r == 0) break;
		if (r < 0) {
			perror("image_read_word");
			break;
		}

		/* regs */
		uint_t change_regs;
		r = arm_instr_changed_regs(instr, &change_regs);
		if (r < 0) break;

		bb_change_regs |= change_regs;

		uint_t use_regs;
		r = arm_instr_used_regs(instr, &use_regs);
		if (r < 0) break;

		bb_use_regs &= (~change_regs & ARM_REG_MASK);
		bb_use_regs |= use_regs;

		/* flags */
		uint_t change_flags;
		r = arm_instr_changed_flags(instr, &change_flags);
		if (r < 0) break;

		bb_change_flags |= change_flags;

		uint_t use_flags;
		r = arm_instr_used_flags(instr, &use_flags);
		if (r < 0) break;

		bb_use_flags &= (~change_flags & ARM_FLAG_MASK);
		bb_use_flags |= use_flags;
	}

	bool change_printed = false;
	if (bb_change_regs != 0) {
		cout << "; changed reg(s): {";
		arm_reglist_fprint(stdout, bb_change_regs);
		cout << " }";
		change_printed = true;
	}
	if (bb_change_flags != 0) {
		if (change_printed) cout << ", ";
		else cout << "; ";
		cout << "changed flag(s): {";
		arm_flaglist_fprint(stdout, bb_change_flags);
		cout << " }";
		change_printed = true;
	}
	if (change_printed) cout << endl;

	bool use_printed = false;
	if (bb_use_regs != 0) {
		cout << "; reg(s) depended on: {";
		arm_reglist_fprint(stdout, bb_use_regs);
		cout << " }";
		use_printed = true;
	}
	if (bb_use_flags != 0) {
		if (use_printed) cout << ", ";
		else cout << "; ";
		cout << "flag(s) depended on: {";
		arm_flaglist_fprint(stdout, bb_use_flags);
		cout << " }";
		use_printed = true;
	}
	if (use_printed) cout << endl;

	/* code references */
	print_code_references(bb, sym_map);

	/* data references */
	print_data_references(bb, sym_map);

	/* find annotations */
	annot_t *annot = NULL;
	arm_addr_t annot_addr;
	map<arm_addr_t, annot_t *>::iterator annot_iter;
	annot_iter = image->annot_map->lower_bound(bb->addr);
	if (annot_iter != image->annot_map->end()) {
		annot_addr = annot_iter->first;
		annot = annot_iter->second;
	}

	/* find symbols */
	const char *symbol_name = NULL;
	arm_addr_t symbol_addr;
	map<arm_addr_t, char *>::iterator symbol_iter;
	symbol_iter = sym_map->lower_bound(bb->addr);
	if (symbol_iter != sym_map->end()) {
		symbol_addr = symbol_iter->first;
		symbol_name = symbol_iter->second;
	}

	addr = bb->addr;
	while (addr < bb->addr + bb->size) {
		arm_instr_t instr;
		r = image_read_word(image, addr, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		/* pre annotation */
		if (annot != NULL && annot_addr == addr &&
		    annot->pre_text != NULL) {
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
		if (symbol_name != NULL && symbol_addr == addr) {
			cout << "; " << symbol_name << ":" << endl;
			symbol_iter++;
			if (symbol_iter != sym_map->end()) {
				symbol_addr = symbol_iter->first;
				symbol_name = symbol_iter->second;
			} else {
				symbol_name = NULL;
			}
		}

		/* print instruction */
		cout << hex << setw(8) << setfill('0') << addr << "\t";
		cout << hex << setw(8) << setfill('0') << instr << "\t";
		arm_instr_fprint(stdout, instr, addr, sym_map, image);

		/* post annotation */
		if (annot != NULL && annot_addr == addr &&
		    annot->post_text != NULL) {
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
		if (annot != NULL && annot_addr == addr) {
			image->annot_map->erase(annot_iter);
			delete annot;

			annot_iter++;
			if (annot_iter != image->annot_map->end()) {
				annot = annot_iter->second;
				image->annot_map->erase(annot_iter);
			} else {
				annot = NULL;
			}
		}

		addr += sizeof(arm_instr_t);
	}

	return 0;
}

static int
print_basic_block_data(basic_block_t *bb, image_t *image,
		       map<arm_addr_t, char *> *sym_map)
{
	int r;

	arm_instr_t instr;
	r = image_read_word(image, bb->addr, &instr);
	if (r == 0) return 0;

	cout << endl
	     << BLOCK_SEPARATOR << endl
	     << "; data block" << endl;

	/* code references */
	print_code_references(bb, sym_map);

	/* data references */
	print_data_references(bb, sym_map);

	arm_addr_t addr = bb->addr;
	while (addr < bb->addr + bb->size) {
		arm_instr_t instr;
		r = image_read_word(image, addr, &instr);
		if (r == 0) break;
		else if (r < 0) return -1;

		/* print data */
		cout << hex << setw(8) << setfill('0') << addr << "\t";
		print_data(instr);

		addr += sizeof(arm_instr_t);
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int r;

	if (argc < 2) return EXIT_FAILURE;

	/* create memory image */
	image_t *image = image_new();
	if (image == NULL) {
		perror("image_new");
		exit(EXIT_FAILURE);
	}

	/* create file mapping */
	r = image_create_mapping(image, 0, 0x200000, argv[1], 500,
				 true, false, true);
	if (r < 0) {
		cerr << "Unable to create file mapping: `" << argv[1] << "'."
		     << endl;
		perror("image_create_mapping");
		exit(EXIT_FAILURE);
	}

	/* create mapping */
	r = image_create_mapping(image, 0x22000000, 0x40000, NULL, 0,
				 true, true, true);
	if (r < 0) {
		perror("image_create_mapping");
		exit(EXIT_FAILURE);
	}

	/* create mapping */
	r = image_create_mapping(image, 0x24000000, 0x200000, argv[1], 500,
				 true, false, true);
	if (r < 0) {
		perror("image_create_mapping");
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
	map<arm_addr_t, basic_block_t *> bb_map;
	r = basicblock_analysis(&bb_map, &ep_list, image);
	if (r < 0) {
		cerr << "Unable to finish basic block analysis." << endl;
		exit(EXIT_FAILURE);
	}


	/* print instructions */
	map<arm_addr_t, basic_block_t *>::iterator bb_iter;
	for (bb_iter = bb_map.begin(); bb_iter != bb_map.end(); bb_iter++) {
		basic_block_t *bb = bb_iter->second;

		if (bb->code) {
			r = print_basic_block_code(bb, image, &sym_map);
			if (r < 0) {
				perror("print_basic_block_code");
				exit(EXIT_FAILURE);
			}
		} else {
			r = print_basic_block_data(bb, image, &sym_map);
			if (r < 0) {
				perror("print_basic_block_data");
				exit(EXIT_FAILURE);
			}
		}
	}

	/* clean up */
	image_free(image);

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
