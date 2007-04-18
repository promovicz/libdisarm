/* codesim.h */

#ifndef _CODESIM_H
#define _CODESIM_H

#include "types.h"


typedef int (*mem_read_func)(arm_addr_t addr, uint_t *val);

typedef enum {
	CODESIM_REG_R0 = 0, CODESIM_REG_R1,
	CODESIM_REG_R2,     CODESIM_REG_R3,
	CODESIM_REG_R4,     CODESIM_REG_R5,
	CODESIM_REG_R6,     CODESIM_REG_R7,
	CODESIM_REG_R8,     CODESIM_REG_R9,
	CODESIM_REG_R10,    CODESIM_REG_R11,
	CODESIM_REG_R12,    CODESIM_REG_R13,
	CODESIM_REG_R14,    CODESIM_REG_R15,
	CODESIM_REG_ZERO,   CODESIM_REG_CARRY,
	CODESIM_REG_NEG,    CODESIM_REG_OVER,
	CODESIM_REG_MAX
} codesim_reg_t;

typedef struct {
	int reg_defined[CODESIM_REG_MAX];
	uint_t reg_val[CODESIM_REG_MAX];
	mem_read_func mem_read;
} codesim_ctx_t;


codesim_ctx_t *codesim_new(mem_read_func mem_read);
void codesim_free(codesim_ctx_t *codesim);
void codesim_reset(codesim_ctx_t *codesim);
int codesim_execute(codesim_ctx_t *codesim,
		    arm_instr_t instr, arm_addr_t addr);
void codesim_set_reg(codesim_ctx_t *codesim, codesim_reg_t reg, uint_t val);
void codesim_undef_reg(codesim_ctx_t *codesim, codesim_reg_t reg);
int codesim_get_reg(codesim_ctx_t *codesim, codesim_reg_t reg, uint_t *val);


#endif /* ! _CODESIM_H */
