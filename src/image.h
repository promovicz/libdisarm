/* image.h */

#ifndef _IMAGE_H
#define _IMAGE_H

#include <stdio.h>
#include <stdlib.h>

#include "hashtable.h"
#include "image.h"
#include "types.h"


typedef struct {
	FILE *file;
	uint_t size;
	void *data;
	int big_endian;
	hashtable_t annot_ht;
} image_t;

typedef struct {
	hashtable_elm_t elm;
	arm_addr_t addr;
	int pre;
	char *text;
	size_t textlen;
} annot_elm_t;


image_t *image_new(const char *filename, int big_endian);
void image_free(image_t *image);
int image_read_instr(image_t *image, uint_t addr, arm_instr_t *instr);
int image_add_annot(image_t *image, uint_t addr,
		    char *text, size_t textlen, int pre);
int image_add_annot_from_file(image_t *image, FILE *f);


#endif /* _IMAGE_H */
