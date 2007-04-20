/* symbol.cpp */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cctype>
#include <cerrno>

#include <iostream>
#include <map>

#include "symbol.hh"
#include "types.hh"

using namespace std;


int
symbol_add(map<arm_addr_t, char *> *sym_map,
	   arm_addr_t addr, const char *name, bool overwrite)
{
	map<arm_addr_t, char *>::iterator sym_old = sym_map->find(addr);
	if (sym_old != sym_map->end()) {
		if (!overwrite) {
			return 0;
		} else {
			char *old_name = (*sym_old).second;
			sym_map->erase(sym_old);
			delete old_name;
		}
	}

	char *dup_name = strdup(name);
	if (dup_name == NULL) return -1;

	(*sym_map)[addr] = dup_name;

	return 0;
}

int
symbol_add_from_file(map<arm_addr_t, char *> *sym_map, FILE *f)
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
				delete text;
				cerr << "Unable to parse address." << endl;
				return -1;
			}

			while (isblank(*endptr)) endptr += 1;

			if (*endptr == '\0' || *endptr == '\n') continue;

			char *nlptr = strchr(endptr, '\n');
			if (nlptr != NULL) *nlptr = '\0';

			r = symbol_add(sym_map, addr, endptr, 1);
			if (r < 0) return -1;
		}
	}

	if (text) delete text;

	return 0;
}
