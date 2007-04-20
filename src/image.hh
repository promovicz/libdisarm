/* image.hh */

#ifndef _IMAGE_HH
#define _IMAGE_HH

#include <map>

#include "image.hh"
#include "types.hh"

using namespace std;


typedef struct {
	char *pre_text;
	size_t pre_textlen;
	char *post_text;
	size_t post_textlen;
} annot_t;

typedef struct {
	FILE *file;
	uint_t size;
	void *data;
	bool big_endian;
	map<arm_addr_t, annot_t *> *annot_map;
} image_t;


image_t *image_new(const char *filename, bool big_endian);
void image_free(image_t *image);
int image_read_instr(image_t *image, arm_addr_t addr, arm_instr_t *instr);
int image_add_annot(image_t *image, arm_addr_t addr,
		    char *text, size_t textlen, bool pre);
int image_add_annot_from_file(image_t *image, FILE *f);


#endif /* _IMAGE_HH */
