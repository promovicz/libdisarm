/* image.c */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

#include "image.h"
#include "endian.h"
#include "hashtable.h"
#include "types.h"


image_t *
image_new(const char *filename)
{
	int r;

	image_t *image = (image_t *)malloc(sizeof(image_t));
	if (image == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	image->file = fopen(filename, "rb");
	if (image->file == NULL) {
		int errsave = errno;
		free(image);
		errno = errsave;
		return NULL;
	}

	struct stat s;
	r = fstat(fileno(image->file), &s);
	if (r < 0) {
		int errsave = errno;
		fclose(image->file);
		free(image);
		errno = errsave;
		return NULL;
	}
	image->size = s.st_size;

	image->data = mmap(NULL, image->size, PROT_READ, MAP_PRIVATE,
			   fileno(image->file), 0);
	if (image->data == MAP_FAILED) {
		int errsv = errno;
		fclose(image->file);
		free(image);
		errno = errsv;
		return NULL;
	}

	r = hashtable_init(&image->annot_ht, 0);
	if (r < 0) return NULL;

	return image;
}

void
image_free(image_t *image)
{
	hashtable_deinit(&image->annot_ht);
	munmap(image->data, image->size);       
	fclose(image->file);
	free(image);
}

int
image_read_instr(image_t *image, uint_t addr, arm_instr_t *instr)
{
	if (addr > image->size) {
		errno = EINVAL;
		return 0;
	}

	arm_instr_t *insdata = (arm_instr_t *)image->data;

	*instr = htobe32(insdata[addr >> 2]);

	return 1;
}

int
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

		hashtable_elm_t *old;
		r = hashtable_insert(annot_ht, (hashtable_elm_t *)annot,
				     &annot->addr, sizeof(uint_t), &old);
		if (r < 0) {
			perror("hashtable_insert");
			exit(EXIT_FAILURE);
		}
		if (old != NULL) free(old);
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
