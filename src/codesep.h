/* codesep.h */

#ifndef _CODESEP_H
#define _CODESEP_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "image.h"
#include "list.h"


int codesep_analysis(list_t *ep_list, image_t *image,
		     uint8_t **code_bitmap, FILE *f);


#endif /* ! _CODESEP_H */
