/* entrypoint.hh */

#ifndef _ENTRYPOINT_H
#define _ENTRYPOINT_H

#include "list.hh"
#include "types.hh"


typedef struct {
	list_elm_t elm;
	arm_addr_t addr;
} ep_elm_t;


#endif /* ! _ENTRYPOINT_H */
