/* codesep.hh */

#ifndef _CODESEP_HH
#define _CODESEP_HH

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "image.hh"
#include "list.hh"


int codesep_analysis(list_t *ep_list, image_t *image,
		     uint8_t **code_bitmap, FILE *f);


#endif /* ! _CODESEP_HH */
