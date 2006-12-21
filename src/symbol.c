/* symbol.c */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include "symbol.h"
#include "types.h"


int
symbol_add(hashtable_t *sym_ht, arm_addr_t addr, const char *name,
	   int overwrite)
{
	int r;

	if (!overwrite) {
		sym_elm_t *sym = (sym_elm_t *)
			hashtable_lookup(sym_ht, &addr, sizeof(arm_addr_t));
		if (sym != NULL) return 0;
	}

	sym_elm_t *sym = malloc(sizeof(sym_elm_t));
	if (sym == NULL) {
		errno = ENOMEM;
		return -1;
	}

	sym->addr = addr;
	sym->name = strdup(name);
	if (sym->name == NULL) {
		int errsv = errno;
		free(sym);
		errno = errsv;
		return -1;
	}

	hashtable_elm_t *old;
	r = hashtable_insert(sym_ht, (hashtable_elm_t *)sym,
			     &sym->addr, sizeof(arm_addr_t), &old);
	if (r < 0) {
		int errsv = errno;
		free(sym->name);
		free(sym);
		errno = errsv;
		return -1;
	}
	if (old != NULL) free(old);

	return 0;
}

int
symbol_add_from_file(hashtable_t *sym_ht, FILE *f)
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

			while (isblank(*endptr)) endptr += 1;

			if (*endptr == '\0' || *endptr == '\n') continue;

			char *nlptr = strchr(endptr, '\n');
			if (nlptr != NULL) *nlptr = '\0';

			r = symbol_add(sym_ht, addr, endptr, 1);
			if (r < 0) return -1;
		}
	}

	if (text) free(text);

	return 0;
}
