/* codesim.cpp */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include "arm.hh"
#include "codesim.hh"
#include "types.hh"


codesim_ctx_t *
codesim_new(mem_read_func mem_read)
{
	codesim_ctx_t *codesim = new codesim_ctx_t;
	if (codesim == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	codesim_reset(codesim);
	codesim->mem_read = mem_read;

	return codesim;
}

void
codesim_free(codesim_ctx_t *codesim)
{
	delete codesim;
}

void
codesim_reset(codesim_ctx_t *codesim)
{
	memset(codesim->reg_defined, '\0', CODESIM_REG_MAX);	
}

int
codesim_execute(codesim_ctx_t *codesim, arm_instr_t instr, arm_addr_t addr)
{
	int ret;

	codesim_set_reg(codesim, CODESIM_REG_R15, addr + 8);

	const arm_instr_pattern_t *ip =
		arm_instr_get_instr_pattern(instr);
	if (ip == NULL) {
		fprintf(stderr, "Undefined instruction encountered in"
			" code simulation at 0x%x.\n", addr);
		codesim_reset(codesim);
		return 0;
	}

	if (ip->type == ARM_INSTR_TYPE_LS_IMM_OFF) {
		int cond, p, u, b, w, load, rn, rd, imm;
		ret = arm_instr_get_params(instr, ip, 9, &cond, &p, &u, &b, &w,
					   &load, &rn, &rd, &imm);
		if (ret < 0) abort();

		/* execute instruction */
		/*
		fprintf(f, "%sr%s%s%s\tr%d, [r%d",
			(load ? "ld" : "st"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(b ? "b" : ""), ((!p && w) ? "t" : ""), rd, rn);

		if (!p) fprintf(f, "]");

		if (imm > 0) {
			fprintf(f, ", #%s%s%x", (u ? "" : "-"),
				(imm < 10 ? "" : "0x"), imm);
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));

		if (rn == 15) {
			int off = imm * (u ? 1 : -1);
			char *targetstr = addr_string(addr + 8 + off,
						      user_data);
			if (targetstr == NULL) abort();

			fprintf(f, "\t; %s", targetstr);
			free(targetstr);
		}
		*/
	} else {
		codesim_reset(codesim);
	}
}

void
codesim_set_reg(codesim_ctx_t *codesim, codesim_reg_t reg, uint_t val)
{
	codesim->reg_val[reg] = val;
	codesim->reg_defined[reg] = 1;
}

void
codesim_undef_reg(codesim_ctx_t *codesim, codesim_reg_t reg)
{
	codesim->reg_defined[reg] = 0;
}

int
codesim_get_reg(codesim_ctx_t *codesim, codesim_reg_t reg, uint_t *val)
{
	if (!codesim->reg_defined[reg]) return -1;

	*val = codesim->reg_val[reg];
	return 0;
}
