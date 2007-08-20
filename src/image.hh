/* image.hh */

#ifndef _IMAGE_HH
#define _IMAGE_HH

#include <map>
#include <set>

#include "types.hh"

using namespace std;


typedef struct {
	char *pre_text;
	size_t pre_textlen;
	char *post_text;
	size_t post_textlen;
} annot_t;

typedef struct {
	arm_addr_t addr;
	uint_t size;
	FILE *file;
	void *data;
	uint_t data_offset;
	uint_t mmap_size;
	bool read;
	bool write;
	bool big_endian;
} image_mapping_t;

typedef struct {
	map<arm_addr_t, image_mapping_t *> mappings;
	map<arm_addr_t, annot_t *> annot_map;
} image_t;


image_t *image_new();
void image_free(image_t *image);
int image_create_mapping(image_t *image, arm_addr_t addr, uint_t size,
			 const char *filename, uint_t offset, bool read,
			 bool write, bool big_endian);
void image_remove_mapping(image_t *image, arm_addr_t addr);
bool image_is_addr_mapped(image_t *image, arm_addr_t addr);
int image_read(image_t *image, arm_addr_t addr, void *dest, uint_t size);
int image_read_byte(image_t *image, arm_addr_t addr, uint8_t *dest);
int image_read_hword(image_t *image, arm_addr_t addr, uint16_t *dest);
int image_read_word(image_t *image, arm_addr_t addr, uint32_t *dest);

int image_add_annot(image_t *image, arm_addr_t addr,
		    char *text, size_t textlen, bool pre);
int image_add_annot_from_file(image_t *image, FILE *f);


#endif /* _IMAGE_HH */
