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
#include <list>
#include <map>

#include "image.hh"
#include "endian.hh"
#include "types.hh"

using namespace std;


image_t *
image_new()
{
	image_t *image = new image_t;
	if (image == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	return image;
}

static void
image_free_mapping(image_mapping_t *mapping)
{
	if (mapping->data != NULL) munmap(mapping->data, mapping->mmap_size);
	if (mapping->file != NULL) fclose(mapping->file);
	delete mapping;
}

void
image_free(image_t *image)
{
	map<arm_addr_t, image_mapping_t *>::iterator mapping_iter;

	mapping_iter = image->mappings.begin();
	while (mapping_iter != image->mappings.end()) {
		image_mapping_t *mapping = (mapping_iter++)->second;
		image_free_mapping(mapping);
		image->mappings.erase(mapping_iter);
	}
}

int
image_create_mapping(image_t *image, arm_addr_t addr, uint_t size,
		     const char *filename, uint_t offset, bool read,
		     bool write, bool big_endian)
{
	int r;

	image_mapping_t *mapping = new image_mapping_t;
	if (mapping == NULL) {
		errno = ENOMEM;
		return -1;
	}

	mapping->addr = addr;

	if (filename != NULL) {
		mapping->file = fopen(filename, "rb");
		if (mapping->file == NULL) {
			int errsv = errno;
			delete mapping;
			errno = errsv;
			return -1;
		}

		struct stat s;
		r = fstat(fileno(mapping->file), &s);
		if (r < 0) {
			int errsv = errno;
			fclose(mapping->file);
			delete mapping;
			errno = errsv;
			return -1;
		}
		mapping->size = max(static_cast<uint_t>(0),
				    min(size, static_cast<uint_t>
					(s.st_size - offset)));

		uint_t page_size = getpagesize();
		uint_t mmap_offset =
			static_cast<uint_t>(offset / page_size) * page_size;

		mapping->mmap_size = mapping->size + offset - mmap_offset;
		mapping->data = mmap(NULL, mapping->mmap_size, PROT_READ,
				     MAP_PRIVATE, fileno(mapping->file),
				     mmap_offset);
		if (mapping->data == MAP_FAILED) {
			int errsv = errno;
			fclose(mapping->file);
			delete mapping;
			errno = errsv;
			return -1;
		}
		mapping->data_offset = offset - mmap_offset;
	} else {
		mapping->file = NULL;
		mapping->size = size;
		mapping->data = NULL;
		mapping->data_offset = 0;
		mapping->mmap_size = 0;
	}

	mapping->read = read;
	mapping->write = write;
	mapping->big_endian = big_endian;

	image->mappings[mapping->addr] = mapping;

	return 0;
}

void
image_remove_mapping(image_t *image, arm_addr_t addr)
{
	map<arm_addr_t, image_mapping_t *>::iterator mapping_iter;

	mapping_iter = image->mappings.upper_bound(addr);
	if (mapping_iter != image->mappings.begin()) {
		image_mapping_t *mapping = (--mapping_iter)->second;
		if (addr < mapping->addr + mapping->size) {
			image_free_mapping(mapping);
			image->mappings.erase(mapping_iter);
		}
	}
}

static image_mapping_t *
image_find_mapping(image_t *image, arm_addr_t addr)
{
	map<arm_addr_t, image_mapping_t *>::iterator mapping_iter;

	mapping_iter = image->mappings.upper_bound(addr);
	if (mapping_iter != image->mappings.begin()) {
		image_mapping_t *mapping = (--mapping_iter)->second;
		if (addr < mapping->addr + mapping->size) {
			return mapping;
		}
	}	

	return NULL;
}

bool
image_is_addr_mapped(image_t *image, arm_addr_t addr)
{
	image_mapping_t *mapping = image_find_mapping(image, addr);
	return (mapping != NULL && mapping->file != NULL);
}

/* no endianness correction is done */
int
image_read(image_t *image, arm_addr_t addr, void *dest, uint_t size)
{
	image_mapping_t *src_mapping = image_find_mapping(image, addr);
	if (src_mapping == NULL) return 0;

	if ((addr + size) > (src_mapping->addr + src_mapping->size)) {
		return 0;
	}

	uint8_t *bytesrc = static_cast<uint8_t *>(src_mapping->data);
	void *src = &bytesrc[src_mapping->data_offset + addr -
			     src_mapping->addr];
	memcpy(dest, src, size);

	return size;
}

int
image_read_byte(image_t *image, arm_addr_t addr, uint8_t *dest)
{
	image_mapping_t *src_mapping = image_find_mapping(image, addr);
	if (src_mapping == NULL) return 0;

	if ((addr + sizeof(uint8_t)) >
	    (src_mapping->addr + src_mapping->size)) return 0;

	uint8_t *bytesrc = static_cast<uint8_t *>(src_mapping->data);
	*dest = *static_cast<uint8_t *>
		(&bytesrc[src_mapping->data_offset + addr -
			  src_mapping->addr]);

	return sizeof(uint8_t);
}

int
image_read_hword(image_t *image, arm_addr_t addr, uint16_t *dest)
{
	image_mapping_t *src_mapping = image_find_mapping(image, addr);
	if (src_mapping == NULL) return 0;

	if ((addr + sizeof(uint16_t)) >
	    (src_mapping->addr + src_mapping->size)) return 0;

	uint8_t *bytesrc = static_cast<uint8_t *>(src_mapping->data);
	*dest = *reinterpret_cast<uint16_t *>
		(&bytesrc[src_mapping->data_offset + addr -
			  src_mapping->addr]);
	if (src_mapping->big_endian) *dest = be16toh(*dest);
	else *dest = le16toh(*dest);

	return sizeof(uint16_t);
}

int
image_read_word(image_t *image, arm_addr_t addr, uint32_t *dest)
{
	image_mapping_t *src_mapping = image_find_mapping(image, addr);
	if (src_mapping == NULL) return 0;

	if ((addr + sizeof(uint32_t)) >
	    (src_mapping->addr + src_mapping->size)) return 0;

	uint8_t *bytesrc = static_cast<uint8_t *>(src_mapping->data);
	*dest = *reinterpret_cast<uint32_t *>
		(&bytesrc[src_mapping->data_offset + addr -
			  src_mapping->addr]);
	if (src_mapping->big_endian) *dest = be32toh(*dest);
	else *dest = le32toh(*dest);

	return sizeof(uint32_t);
}

int
image_add_annot(image_t *image, arm_addr_t addr,
		char *text, size_t textlen, bool pre)
{
	int r;
	map<arm_addr_t, annot_t *>::iterator annot_pos =
		image->annot_map.find(addr);

	if (annot_pos == image->annot_map.end()) {
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

		image->annot_map[addr] = annot;
	} else {
		annot_t *annot = annot_pos->second;
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
