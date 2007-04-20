/* inter.hh */

#ifndef _INTER_HH
#define _INTER_HH

#include "types.hh"


typedef struct _inter_ctx inter_ctx_t;


inter_ctx_t *inter_new_ctx();
void inter_clear_ctx(inter_ctx_t *ctx);

void inter_arm_append(inter_ctx_t *ctx, arm_instr_t instr, arm_addr_t addr);

void inter_fprint(FILE *f, inter_ctx_t *ctx);


#endif /* ! _INTER_HH */
