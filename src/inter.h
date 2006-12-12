/* inter.h */

#ifndef _INTER_H
#define _INTER_H

#include <stdio.h>
#include <stdlib.h>

#include "arm.h"


typedef struct _inter_context inter_context_t;


inter_context_t *inter_new_context();
void inter_clear_context(inter_context_t *ctx);

void inter_arm_append(inter_context_t *ctx, arm_instr_t instr,
		      unsigned int address);

void inter_fprint(FILE *f, inter_context_t *ctx);


#endif /* ! _INTER_H */
