/* codesep.hh */

#ifndef _CODESEP_HH
#define _CODESEP_HH

#include <list>

#include "image.hh"
#include "types.hh"


int codesep_analysis(std::list<arm_addr_t> *ep_list, image_t *image,
		     uint8_t **code_bitmap, FILE *f);


#endif /* ! _CODESEP_HH */
