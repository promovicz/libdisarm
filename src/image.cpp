/* image.cpp */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cerrno>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <iostream>
#include <map>

#include "image.hh"
#include "endian.hh"
#include "types.hh"

using namespace std;


image_t *
image_new(const char *filename, bool big_endian)
{
	int r;

	image_t *image = new image_t;
	if (image == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	image->file = fopen(filename, "rb");
	if (image->file == NULL) {
		int errsave = errno;
		delete image;
		errno = errsave;
		return NULL;
	}

	struct stat s;
	r = fstat(fileno(image->file), &s);
	if (r < 0) {
		int errsave = errno;
		fclose(image->file);
		delete image;
		errno = errsave;
		return NULL;
	}
	image->size = s.st_size;

	image->data = mmap(NULL, image->size, PROT_READ, MAP_PRIVATE,
			   fileno(image->file), 0);
	if (image->data == MAP_FAILED) {
		int errsv = errno;
		fclose(image->file);
		delete image;
		errno = errsv;
		return NULL;
	}

	image->big_endian = big_endian;

	image->annot_map = new map<arm_addr_t, annot_t *>();

	return image;
}

void
image_free(image_t *image)
{
	delete image->annot_map;
	munmap(image->data, image->size);
	fclose(image->file);
	delete image;
}

int
image_read_instr(image_t *image, uint_t addr, arm_instr_t *instr)
{
	if (addr > image->size) {
		errno = EINVAL;
		return 0;
	}

	arm_instr_t *insdata = (arm_instr_t *)image->data;

	if (image->big_endian) *instr = be32toh(insdata[addr >> 2]);
	else *instr = le32toh(insdata[addr >> 2]);

	return 1;
}

int
image_add_annot(image_t *image, arm_addr_t addr,
		char *text, size_t textlen, bool pre)
{
	int r;
	map<arm_addr_t, annot_t *>::iterator annot_pos =
		image->annot_map->find(addr);

	if (annot_pos == image->annot_map->end()) {
		annot_t *annot = new annot_t;
		if (annot == NULL) {
			errno = ENOMEM;
			return -1;
		}

		if (pre) {
			annot->pre_text = text;
			annot->pre_textlen = textlen;
			annot->post_text = NULL;
			annot->post_textlen = 0;
		} else {
			annot->pre_text = NULL;
			annot->pre_textlen = 0;
			annot->post_text = text;
			annot->post_textlen = textlen;
		}

		(*image->annot_map)[addr] = annot;
	} else {
		annot_t *annot = (*annot_pos).second;
		char **annot_text;
		size_t *annot_textlen;

		if (pre) {
			annot_text = &annot->pre_text;
			annot_textlen = &annot->pre_textlen;
		} else {
			annot_text = &annot->post_text;
			annot_textlen = &annot->post_textlen;
		}

		*annot_text = static_cast<char *>
			(realloc(*annot_text, *annot_textlen + textlen + 1));
		if (*annot_text == NULL) {
			errno = ENOMEM;
			return -1;
		}

		memcpy(&(*annot_text)[*annot_textlen], text, textlen);
		*annot_textlen += textlen;
		(*annot_text)[*annot_textlen] = '\0';

		delete text;
	}

	return 0;
}

int
image_add_annot_from_file(image_t *image, FILE *f)
{
	int r;
	char *text = NULL;
	size_t textlen = 0;
	ssize_t read;

	uint_t addr = 0;
	int reading_addr = 1;
	int pre = 1;

	uint_t line = 1;
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
				delete text;
				cerr << "Unable to parse address at line " 
				     << dec << line << "." << endl;
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
		line += 1;
	}

	if (text) delete text;

	return 0;
}
