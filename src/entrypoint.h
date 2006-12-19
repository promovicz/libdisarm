/* entrypoint.h */

#ifndef _ENTRYPOINT_H
#define _ENTRYPOINT_H

#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "types.h"


typedef struct {
	list_elm_t elm;
	arm_addr_t addr;
} ep_elm_t;


#endif /* ! _ENTRYPOINT_H */
