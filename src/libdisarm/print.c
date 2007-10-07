/*
 * print.c - Instruction printer functions
 *
 * Copyright (C) 2007  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "args.h"
#include "macros.h"
#include "print.h"
#include "types.h"


static const char *const da_cond_map[] = {
	"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
	"hi", "ls", "ge", "lt", "gt", "le", "", "nv"
};

static const char *const da_shift_map[] = {
	"lsl", "lsr", "asr", "ror"
};

static const char *const da_data_op_map[] = {
	"and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
	"tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn"
};


static void
da_reglist_fprint(FILE *f, da_uint_t reglist)
{
	int comma = 0;
	int range_start = -1;
	int i = 0;
	for (i = 0; reglist; i++) {
		if (reglist & 1 && range_start == -1) {
			range_start = i;
		}

		reglist >>= 1;

		if (!(reglist & 1)) {
			if (range_start == i) {
				if (comma) fprintf(f, ",");
				fprintf(f, " r%d", i);
				comma = 1;
			} else if (i > 0 && range_start == i-1) {
				if (comma) fprintf(f, ",");
				fprintf(f, " r%d, r%d",
					range_start, i);
				comma = 1;
			} else if (range_start >= 0) {
				if (comma) fprintf(f, ",");
				fprintf(f, " r%d-r%d", range_start, i);
				comma = 1;
			}
			range_start = -1;
		}
	}
}


static void
da_instr_fprint_bkpt(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t imm_hi = DA_ARG(instr, 8, 0xfff);
	da_uint_t imm_lo = DA_ARG(instr, 0, 0xf);
	
	fprintf(f, "bkpt%s\t0x%x", da_cond_map[cond],
		((imm_hi & 0xfff) << 4) | (imm_lo & 0xf));
}

static void
da_instr_fprint_bl(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t link = DA_ARG_BOOL(instr, 24);
	da_uint_t off = DA_ARG(instr, 0, 0xffffff);

	da_uint_t target = da_instr_branch_target(off, addr);

	fprintf(f, "b%s%s\t0x%x", (link ? "l" : ""), da_cond_map[cond],
		target);
}

static void
da_instr_fprint_blx_reg(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t link = DA_ARG_BOOL(instr, 5);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	fprintf(f, "b%sx%s\tr%d", (link ? "l" : ""),
		da_cond_map[cond], rm);
}

static void
da_instr_fprint_blx_imm(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_uint_t h = DA_ARG_BOOL(instr, 24);
	da_uint_t offset = DA_ARG(instr, 0, 0xffffff);
	
	da_uint_t target = da_instr_branch_target(offset, addr);

	fprintf(f, "blx\t0x%x", target | h);
}

static void
da_instr_fprint_clz(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	fprintf(f, "clz%s\tr%d, r%d", da_cond_map[cond], rd, rm);
}

static void
da_instr_fprint_cp_data(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t op_1 = DA_ARG(instr, 20, 0xf);
	da_cpreg_t crn = DA_ARG_CPREG(instr, 16);
	da_cpreg_t crd = DA_ARG_CPREG(instr, 12);
	da_uint_t cp_num = DA_ARG(instr, 8, 0xf);
	da_uint_t op_2 = DA_ARG(instr, 5, 0x7);
	da_cpreg_t crm = DA_ARG_CPREG(instr, 0);

	fprintf(f, "cdp%s\tp%d, %d, cr%d, cr%d, cr%d, %d",
		(cond != DA_COND_NV ? da_cond_map[cond] : "2"),
		cp_num, op_1, crd, crn, crm, op_2);
}

static void
da_instr_fprint_cp_ls(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t unsign = DA_ARG_BOOL(instr, 23);
	da_uint_t n = DA_ARG_BOOL(instr, 22);
	da_uint_t write = DA_ARG_BOOL(instr, 21);
	da_uint_t load = DA_ARG_BOOL(instr, 20);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_cpreg_t crd = DA_ARG_CPREG(instr, 12);
	da_uint_t cp_num = DA_ARG(instr, 8, 0xf);
	da_uint_t imm = DA_ARG(instr, 0, 0xff);

	fprintf(f, "%sc%s%s\tp%d, cr%d, [r%d", (load ? "ld" : "st"),
		(cond != DA_COND_NV ? da_cond_map[cond] : "2"),
		(n ? "l" : ""), cp_num, crd, rn);

	if (!p) fprintf(f, "]");

	if (!(unsign || p)) fprintf(f, ", {%d}", imm);
	else if (imm > 0) {
		fprintf(f, ", #%s0x%x", (unsign ? "" : "-"), (imm << 2));
	}

	if (p) fprintf(f, "]%s", (write ? "!" : ""));
}

static void
da_instr_fprint_cp_reg(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t op_1 = DA_ARG(instr, 21, 0x7);
	da_uint_t load = DA_ARG_BOOL(instr, 20);
	da_cpreg_t crn = DA_ARG_CPREG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t cp_num = DA_ARG(instr, 8, 0xf);
	da_uint_t op_2 = DA_ARG(instr, 5, 0x7);
	da_cpreg_t crm = DA_ARG_CPREG(instr, 0);

	fprintf(f, "m%s%s\tp%d, %d, r%d, cr%d, cr%d, %d", (load ? "rc" : "cr"),
		(cond != DA_COND_NV ? da_cond_map[cond] : "2"), cp_num, op_1,
		rd, crn, crm, op_2);
}

static void
da_instr_fprint_data_imm(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_data_op_t op = DA_ARG_DATA_OP(instr, 21);
	da_uint_t flags = DA_ARG_BOOL(instr, 20);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t rot = DA_ARG(instr, 8, 0xf);
	da_uint_t imm = DA_ARG(instr, 0, 0xff);

	fprintf(f, "%s%s%s\t", da_data_op_map[op], da_cond_map[cond],
		((flags && (op < DA_DATA_OP_TST || op > DA_DATA_OP_CMN)) ?
		 "s" : ""));

	if (op >= DA_DATA_OP_TST && op <= DA_DATA_OP_CMN) {
		fprintf(f, "r%d", rn);
	} else if (op == DA_DATA_OP_MOV || op == DA_DATA_OP_MVN) {
		fprintf(f, "r%d", rd);
	} else {
		fprintf(f, "r%d, r%d", rd, rn);
	}

	da_uint_t rot_imm = (imm >> (rot << 1)) | (imm << (32 - (rot << 1)));
	
	fprintf(f, ", #0x%x", rot_imm);

	if (rn == DA_REG_R15) {
		if (op == DA_DATA_OP_ADD) {
			fprintf(f, "\t; 0x%x", addr + 8 + rot_imm);
		} else if (op == DA_DATA_OP_SUB) {
			fprintf(f, "\t; 0x%x", addr + 8 - rot_imm);
		}
	}
}

static void
da_instr_fprint_data_imm_sh(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_data_op_t op = DA_ARG_DATA_OP(instr, 21);
	da_uint_t flags = DA_ARG_BOOL(instr, 20);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t sha = DA_ARG(instr, 7, 0x1f);
	da_shift_t sh = DA_ARG_SHIFT(instr, 5);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	fprintf(f, "%s%s%s\t", da_data_op_map[op], da_cond_map[cond],
		((flags && (op < DA_DATA_OP_TST || op > DA_DATA_OP_CMN)) ?
		 "s" : ""));

	if (op >= DA_DATA_OP_TST && op <= DA_DATA_OP_CMN) {
		fprintf(f, "r%d, r%d", rn, rm);
	} else if (op == DA_DATA_OP_MOV || op == DA_DATA_OP_MVN) {
		fprintf(f, "r%d, r%d", rd, rm);
	} else {
		fprintf(f, "r%d, r%d, r%d", rd, rn, rm);
	}

	if (sh == DA_SHIFT_LSR || sh == DA_SHIFT_ASR) {
		sha = ((sha > 0) ? sha : 32);
	}

	if (sha > 0) {
		fprintf(f, ", %s #0x%x", da_shift_map[sh], sha);
	} else if (sh == DA_SHIFT_ROR) {
		fprintf(f, ", rrx");
	}
}

static void
da_instr_fprint_data_reg_sh(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_data_op_t op = DA_ARG_DATA_OP(instr, 21);
	da_uint_t flags = DA_ARG_BOOL(instr, 20);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_reg_t rs = DA_ARG_REG(instr, 8);
	da_shift_t sh = DA_ARG_SHIFT(instr, 5);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	fprintf(f, "%s%s%s\t", da_data_op_map[op], da_cond_map[cond],
		((flags && (op < DA_DATA_OP_TST || op > DA_DATA_OP_CMN)) ?
		 "s" : ""));

	if (op >= DA_DATA_OP_TST && op <= DA_DATA_OP_CMN) {
		fprintf(f, "r%d, r%d", rn, rm);
	} else if (op == DA_DATA_OP_MOV || op == DA_DATA_OP_MVN) {
		fprintf(f, "r%d, r%d", rd, rm);
	} else {
		fprintf(f, "r%d, r%d, r%d", rd, rn, rm);
	}

	fprintf(f, ", %s r%d", da_shift_map[sh], rs);
}

static void
da_instr_fprint_dsp_add_sub(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t op = DA_ARG(instr, 21, 0x3);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_reg_t rm = DA_ARG_REG(instr, 0);
	
	fprintf(f, "q%s%s%s\tr%d, r%d, r%d", ((op & 2) ? "d" : ""),
		((op & 1) ? "sub" : "add"), da_cond_map[cond], rd, rm, rn);
}

static void
da_instr_fprint_dsp_mul(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t op = DA_ARG(instr, 21, 0x3);
	da_reg_t rd = DA_ARG_REG(instr, 16);
	da_reg_t rn = DA_ARG_REG(instr, 12);
	da_reg_t rs = DA_ARG_REG(instr, 12);
	da_uint_t y = DA_ARG_BOOL(instr, 6);
	da_uint_t x = DA_ARG_BOOL(instr, 5);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	switch (op) {
	case 0:
		fprintf(f, "smla%s%s%s\tr%d, r%d, r%d, r%d", (x ? "t" : "b"),
			(y ? "t" : "b"), da_cond_map[cond], rd, rm, rs, rn);
		break;
	case 1:
		fprintf(f, "s%sw%s%s\tr%d, r%d, r%d", (x ? "mul" : "mla"),
			(y ? "t" : "b"), da_cond_map[cond], rd, rm, rs);
		if (!x) fprintf(f, ", r%d", rn);
		break;
	case 2:
		fprintf(f, "smlal%s%s%s\tr%d, r%d, r%d, r%d", (x ? "t" : "b"),
			(y ? "t" : "b"), da_cond_map[cond], rn, rd, rm, rs);
		break;
	case 3:
		fprintf(f, "smul%s%s%s\tr%d, r%d, r%d", (x ? "t" : "b"),
			(y ? "t" : "b"), da_cond_map[cond], rd, rm, rs);
		break;
	}
}

static void
da_instr_fprint_l_sign_imm_off(FILE *f, const da_instr_t *instr,
			       da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t unsign = DA_ARG_BOOL(instr, 23);
	da_uint_t write = DA_ARG_BOOL(instr, 21);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t off_hi = DA_ARG(instr, 8, 0xf);
	da_uint_t hword = DA_ARG_BOOL(instr, 5);
	da_uint_t off_lo = DA_ARG(instr, 0, 0xf);

	fprintf(f, "ldr%ss%s\tr%d, [r%d", da_cond_map[cond],
		(hword ? "h" : "b"), rd, rn);

	if (!p) fprintf(f, "]");

	if (off_hi || off_lo) {
		da_uint_t off = (off_hi << 4) | off_lo;
		fprintf(f, ", #%s0x%x", (unsign ? "" : "-"), off);
	}

	if (p) fprintf(f, "]%s", (write ? "!" : ""));

	if (rn == DA_REG_R15) {
		da_uint_t off = ((off_hi << 4) | off_lo) * (unsign ? 1 : -1);
		fprintf(f, "\t; 0x%x", addr + 8 + off);
	}
}

static void
da_instr_fprint_l_sign_reg_off(FILE *f, const da_instr_t *instr,
			       da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t unsign = DA_ARG_BOOL(instr, 23);
	da_uint_t write = DA_ARG_BOOL(instr, 21);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t hword = DA_ARG_BOOL(instr, 5);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	fprintf(f, "ldr%ss%s\tr%d, [r%d", da_cond_map[cond],
		(hword ? "h" : "b"), rd, rn);

	if (!p) fprintf(f, "]");

	fprintf(f, ", %sr%d", (unsign ? "" : "-"), rm);

	if (p) fprintf(f, "]%s", (write ? "!" : ""));
}

static void
da_instr_fprint_ls_hw_imm_off(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t unsign = DA_ARG_BOOL(instr, 23);
	da_uint_t write = DA_ARG_BOOL(instr, 21);
	da_uint_t load = DA_ARG_BOOL(instr, 20);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t off_hi = DA_ARG(instr, 8, 0xf);
	da_uint_t off_lo = DA_ARG(instr, 0, 0xf);
	
	fprintf(f, "%sr%sh\tr%d, [r%d", (load ? "ld" : "st"),
		da_cond_map[cond], rd, rn);

	if (!p) fprintf(f, "]");

	if (off_hi || off_lo) {
		da_uint_t off = (off_hi << 4) | off_lo;
		fprintf(f, ", #%s0x%x", (unsign ? "" : "-"), off);
	}

	if (p) fprintf(f, "]%s", (write ? "!" : ""));

	if (rn == DA_REG_R15) {
		da_uint_t off = ((off_hi << 4) | off_lo) * (unsign ? 1 : -1);
		fprintf(f, "\t; 0x%x", addr + 8 + off);
	}
}

static void
da_instr_fprint_ls_hw_reg_off(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t unsign = DA_ARG_BOOL(instr, 23);
	da_uint_t write = DA_ARG_BOOL(instr, 21);
	da_uint_t load = DA_ARG_BOOL(instr, 20);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_reg_t rm = DA_ARG_REG(instr,  0);
	
	fprintf(f, "%sr%sh\tr%d, [r%d", (load ? "ld" : "st"),
		da_cond_map[cond], rd, rn);

	if (!p) fprintf(f, "]");

	fprintf(f, ", %sr%d", (unsign ? "" : "-"), rm);

	if (p) fprintf(f, "]%s", (write ? "!" : ""));
}

static void
da_instr_fprint_ls_imm_off(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t unsign = DA_ARG_BOOL(instr, 23);
	da_uint_t byte = DA_ARG_BOOL(instr, 22);
	da_uint_t w = DA_ARG_BOOL(instr, 21);
	da_uint_t load = DA_ARG_BOOL(instr, 20);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t imm = DA_ARG(instr, 0, 0xfff);

	fprintf(f, "%sr%s%s%s\tr%d, [r%d", (load ? "ld" : "st"),
		da_cond_map[cond], (byte ? "b" : ""),
		((!p && w) ? "t" : ""), rd, rn);

	if (!p) fprintf(f, "]");

	if (imm > 0) fprintf(f, ", #%s0x%x", (unsign ? "" : "-"), imm);

	if (p) fprintf(f, "]%s", (w ? "!" : ""));

	if (rn == DA_REG_R15) {
		da_uint_t off = (unsign ? 1 : -1) * imm;
		fprintf(f, "\t; 0x%x", addr + 8 + off);
	}
}

static void
da_instr_fprint_ls_multi(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t u = DA_ARG_BOOL(instr, 23);
	da_uint_t s = DA_ARG_BOOL(instr, 22);
	da_uint_t write = DA_ARG_BOOL(instr, 21);
	da_uint_t load = DA_ARG_BOOL(instr, 20);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_uint_t reglist = DA_ARG(instr, 0, 0xffff);

	fprintf(f, "%sm%s%s%s\tr%d%s, {", (load ? "ld" : "st"),
		da_cond_map[cond], (u ? "i" : "d"), (p ? "b" : "a"),
		rn, (write ? "!" : ""));
      
	da_reglist_fprint(f, reglist);

	fprintf(f, " }%s", (s ? "^" : ""));
}

static void
da_instr_fprint_ls_reg_off(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t unsign = DA_ARG_BOOL(instr, 23);
	da_uint_t byte = DA_ARG_BOOL(instr, 22);
	da_uint_t write = DA_ARG_BOOL(instr, 21);
	da_uint_t load = DA_ARG_BOOL(instr, 20);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t sha = DA_ARG(instr, 7, 0x1f);
	da_shift_t sh = DA_ARG_SHIFT(instr, 5);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	fprintf(f, "%sr%s%s%s\tr%d, [r%d", (load ? "ld" : "st"),
		da_cond_map[cond], (byte ? "b" : ""),
		((!p && write) ? "t" : ""), rd, rn);

	if (!p) fprintf(f, "]");

	fprintf(f, ", %sr%d", (unsign ? "" : "-"), rm);

	if (sh == DA_SHIFT_LSR || sh == DA_SHIFT_ASR) {
		sha = (sha ? sha : 32);
	}

	if (sha > 0) {
		fprintf(f, ", %s #0x%x", da_shift_map[sh], sha);
	} else if (sh == DA_SHIFT_ROR) {
		fprintf(f, ", rrx");
	}

	if (p) fprintf(f, "]%s", (write ? "!" : ""));
}

static void
da_instr_fprint_ls_two_imm_off(FILE *f, const da_instr_t *instr,
			       da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t unsign = DA_ARG_BOOL(instr, 23);
	da_uint_t write = DA_ARG_BOOL(instr, 21);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t off_hi = DA_ARG(instr, 8, 0xf);
	da_uint_t store = DA_ARG_BOOL(instr, 5);
	da_uint_t off_lo = DA_ARG(instr, 0, 0xf);

	fprintf(f, "%sr%sd\tr%d, [r%d", (store ? "st" : "ld"),
		da_cond_map[cond], rd, rn);

	if (!p) fprintf(f, "]");

	if (off_hi || off_lo) {
		da_uint_t off = (off_hi << 4) | off_lo;
		fprintf(f, ", #%s0x%x", (unsign ? "" : "-"), off);
	}

	if (p) fprintf(f, "]%s", (write ? "!" : ""));

	if (rn == DA_REG_R15) {
		da_uint_t off = ((off_hi << 4) | off_lo) * (unsign ? 1 : -1);
		fprintf(f, "\t; 0x%x", addr + 8 + off);
	}
}

static void
da_instr_fprint_ls_two_reg_off(FILE *f, const da_instr_t *instr,
			       da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t p = DA_ARG_BOOL(instr, 24);
	da_uint_t unsign = DA_ARG_BOOL(instr, 23);
	da_uint_t write = DA_ARG_BOOL(instr, 21);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_uint_t store = DA_ARG_BOOL(instr, 5);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	fprintf(f, "%sr%sd\tr%d, [r%d", (store ? "st" : "ld"),
		da_cond_map[cond], rd, rn);

	if (!p) fprintf(f, "]");

	fprintf(f, ", %sr%d", (unsign ? "" : "-"), rm);

	if (p) fprintf(f, "]%s", (write ? "!" : ""));
}

static void
da_instr_fprint_mrs(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t r = DA_ARG_BOOL(instr, 22);
	da_reg_t rd = DA_ARG_REG(instr, 12);

	fprintf(f, "mrs%s\tr%d, %s", da_cond_map[cond],
		rd, (r ? "SPSR" : "CPSR"));
}

static void
da_instr_fprint_msr(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t r = DA_ARG_BOOL(instr, 22);
	da_uint_t mask = DA_ARG(instr, 16, 0xf);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	fprintf(f, "msr%s\t%s_%s%s%s%s", da_cond_map[cond],
		(r ? "SPSR" : "CPSR"), ((mask & 1) ? "c" : ""),
		((mask & 2) ? "x" : ""), ((mask & 4) ? "s" : ""),
		((mask & 8) ? "f" : ""));

	fprintf(f, ", r%d", rm);
}

static void
da_instr_fprint_msr_imm(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t r = DA_ARG_BOOL(instr, 22);
	da_uint_t mask = DA_ARG(instr, 16, 0xf);
	da_uint_t rot = DA_ARG(instr, 8, 0xf);
	da_uint_t imm = DA_ARG(instr, 0, 0xff);
	
	fprintf(f, "msr%s\t%s_%s%s%s%s", da_cond_map[cond],
		(r ? "SPSR" : "CPSR"), ((mask & 1) ? "c" : ""),
		((mask & 2) ? "x" : ""), ((mask & 4) ? "s" : ""),
		((mask & 8) ? "f" : ""));

	da_uint_t rot_imm = (imm >> (rot << 1)) | (imm << (32 - (rot << 1)));

	fprintf(f, ", #0x%x", rot_imm);
}

static void
da_instr_fprint_mul(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t acc = DA_ARG_BOOL(instr, 21);
	da_uint_t flags = DA_ARG_BOOL(instr, 20);
	da_reg_t rd = DA_ARG_REG(instr, 16);
	da_reg_t rn = DA_ARG_REG(instr, 12);
	da_reg_t rs = DA_ARG_REG(instr, 8);
	da_reg_t rm = DA_ARG_REG(instr, 0);
	
	fprintf(f, "m%s%s%s\tr%d, r%d, r%d", (acc ? "la" : "ul"),
		da_cond_map[cond], (flags ? "s" : ""), rd, rm, rs);

	if (acc) fprintf(f, ", r%d", rn);
}

static void
da_instr_fprint_mull(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t sign = DA_ARG_BOOL(instr, 22);
	da_uint_t acc = DA_ARG_BOOL(instr, 21);
	da_uint_t flags = DA_ARG_BOOL(instr, 20);
	da_reg_t rd_hi = DA_ARG_REG(instr, 16);
	da_reg_t rd_lo = DA_ARG_REG(instr, 12);
	da_reg_t rs = DA_ARG_REG(instr, 8);
	da_reg_t rm = DA_ARG_REG(instr, 0);

	fprintf(f, "%sm%sl%s%s\tr%d, r%d, r%d, r%d",
		(sign ? "s" : "u"), (acc ? "la" : "ul"), da_cond_map[cond],
		(flags ? "s" : ""), rd_lo, rd_hi, rm, rs);
}

static void
da_instr_fprint_swi(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t imm = DA_ARG(instr, 0, 0xffffff);
	
	fprintf(f, "swi%s\t0x%x", da_cond_map[cond], imm);
}

static void
da_instr_fprint_swp(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	da_cond_t cond = DA_ARG_COND(instr, 28);
	da_uint_t byte = DA_ARG_BOOL(instr, 22);
	da_reg_t rn = DA_ARG_REG(instr, 16);
	da_reg_t rd = DA_ARG_REG(instr, 12);
	da_reg_t rm = DA_ARG_REG(instr, 10);
	
	fprintf(f, "swp%s%s\tr%d, r%d, [r%d]", da_cond_map[cond],
		(byte ? "b" : ""), rd, rm, rn);
}

DA_API void
da_instr_fprint(FILE *f, const da_instr_t *instr, da_addr_t addr)
{
	switch (instr->group) {
	case DA_GROUP_BKPT:
		da_instr_fprint_bkpt(f, instr, addr);
		break;
	case DA_GROUP_BL:
		da_instr_fprint_bl(f, instr, addr);
		break;
	case DA_GROUP_BLX_REG:
		da_instr_fprint_blx_reg(f, instr, addr);
		break;
	case DA_GROUP_BLX_IMM:
		da_instr_fprint_blx_imm(f, instr, addr);
		break;
	case DA_GROUP_CLZ:
		da_instr_fprint_clz(f, instr, addr);
		break;
	case DA_GROUP_CP_DATA:
		da_instr_fprint_cp_data(f, instr, addr);
		break;
	case DA_GROUP_CP_LS:
		da_instr_fprint_cp_ls(f, instr, addr);
		break;
	case DA_GROUP_CP_REG:
		da_instr_fprint_cp_reg(f, instr, addr);
		break;
	case DA_GROUP_DATA_IMM:
		da_instr_fprint_data_imm(f, instr, addr);
		break;
	case DA_GROUP_DATA_IMM_SH:
		da_instr_fprint_data_imm_sh(f, instr, addr);
		break;
	case DA_GROUP_DATA_REG_SH:
		da_instr_fprint_data_reg_sh(f, instr, addr);
		break;
	case DA_GROUP_DSP_ADD_SUB:
		da_instr_fprint_dsp_add_sub(f, instr, addr);
		break;
	case DA_GROUP_DSP_MUL:
		da_instr_fprint_dsp_mul(f, instr, addr);
		break;
	case DA_GROUP_L_SIGN_IMM_OFF:
		da_instr_fprint_l_sign_imm_off(f, instr, addr);
		break;
	case DA_GROUP_L_SIGN_REG_OFF:
		da_instr_fprint_l_sign_reg_off(f, instr, addr);
		break;
	case DA_GROUP_LS_HW_IMM_OFF:
		da_instr_fprint_ls_hw_imm_off(f, instr, addr);
		break;
	case DA_GROUP_LS_HW_REG_OFF:
		da_instr_fprint_ls_hw_reg_off(f, instr, addr);
		break;
	case DA_GROUP_LS_IMM_OFF:
		da_instr_fprint_ls_imm_off(f, instr, addr);
		break;
	case DA_GROUP_LS_MULTI:
		da_instr_fprint_ls_multi(f, instr, addr);
		break;
	case DA_GROUP_LS_REG_OFF:
		da_instr_fprint_ls_reg_off(f, instr, addr);
		break;
	case DA_GROUP_LS_TWO_IMM_OFF:
		da_instr_fprint_ls_two_imm_off(f, instr, addr);
		break;
	case DA_GROUP_LS_TWO_REG_OFF:
		da_instr_fprint_ls_two_reg_off(f, instr, addr);
		break;
	case DA_GROUP_MRS:
		da_instr_fprint_mrs(f, instr, addr);
		break;
	case DA_GROUP_MSR:
		da_instr_fprint_msr(f, instr, addr);
		break;
	case DA_GROUP_MSR_IMM:
		da_instr_fprint_msr_imm(f, instr, addr);
		break;
	case DA_GROUP_MUL:
		da_instr_fprint_mul(f, instr, addr);
		break;
	case DA_GROUP_MULL:
		da_instr_fprint_mull(f, instr, addr);
		break;
	case DA_GROUP_SWI:
		da_instr_fprint_swi(f, instr, addr);
		break;
	case DA_GROUP_SWP:
		da_instr_fprint_swp(f, instr, addr);
		break;
	case DA_GROUP_UNDEF_1:
	case DA_GROUP_UNDEF_2:
	case DA_GROUP_UNDEF_3:
	case DA_GROUP_UNDEF_4:
	case DA_GROUP_UNDEF_5:
		fprintf(f, "undefined");
		break;
	default:
		fprintf(f, "unknown");
		break;
	}
}
