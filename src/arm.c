/* arm.c */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include "arm.h"


static const arm_param_pattern_t branch_xchg_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t clz_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t bkpt_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x000fff00,  8, ARM_PARAM_TYPE_UINT, "immediateHi" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_UINT, "immediateLo" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t move_status_reg_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "R" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t move_reg_status_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "R" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_UINT, "mask" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t swap_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "B" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },	
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t dsp_add_sub_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00600000, 21, ARM_PARAM_TYPE_UINT, "op" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t dsp_mul_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00600000, 21, ARM_PARAM_TYPE_UINT, "op" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x00000f00, 12, ARM_PARAM_TYPE_REG, "Rs" },
	{ 0x00000040,  6, ARM_PARAM_TYPE_UINT, "y" },
	{ 0x00000020,  5, ARM_PARAM_TYPE_UINT, "x" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t mul_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "A" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_REG, "Rs" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t mul_long_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "A" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "RdHi" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "RdLo" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_REG, "Rs" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t ls_hword_reg_off_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "L" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t ls_hword_imm_off_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "L" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "HiOffset" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_UINT, "LoOffset" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t ls_two_reg_off_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000020,  5, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t l_signed_reg_off_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000020,  5, ARM_PARAM_TYPE_UINT, "H" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t ls_two_imm_off_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "HiOffset" },
	{ 0x00000020,  5, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_UINT, "LoOffset" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t l_signed_imm_off_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "HiOffset" },
	{ 0x00000020,  5, ARM_PARAM_TYPE_UINT, "H" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_UINT, "LoOffset" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t move_imm_status_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "R" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_UINT, "mask" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "rotate" },
	{ 0x000000ff,  0, ARM_PARAM_TYPE_UINT, "immediate" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t branch_link_thumb_pattern[] = {
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "H" },
	{ 0x00ffffff,  0, ARM_PARAM_TYPE_UINT, "offset" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t data_reg_shift_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01e00000, 21, ARM_PARAM_TYPE_DATA_OP, "opcode" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_REG, "Rs" },
	{ 0x00000060,  5, ARM_PARAM_TYPE_UINT, "shift" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t cp_data_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00f00000, 20, ARM_PARAM_TYPE_UINT, "opcode1" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_CPREG, "CRn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_CPREG, "CRd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "cp_num" },
	{ 0x000000e0,  5, ARM_PARAM_TYPE_UINT, "opcode2" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_CPREG, "CRm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t cp_reg_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00e00000, 21, ARM_PARAM_TYPE_UINT, "opcode1" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "L" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_CPREG, "CRn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "cp_num" },
	{ 0x000000e0,  5, ARM_PARAM_TYPE_UINT, "opcode2" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_CPREG, "CRm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t data_imm_shift_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01e00000, 21, ARM_PARAM_TYPE_DATA_OP, "opcode" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000f80,  7, ARM_PARAM_TYPE_UINT, "shift amount" },
	{ 0x00000060,  5, ARM_PARAM_TYPE_UINT, "shift" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t ls_reg_off_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "B" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "L" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000f80,  7, ARM_PARAM_TYPE_UINT, "shift amount" },
	{ 0x00000060,  5, ARM_PARAM_TYPE_UINT, "shift" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t swi_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00ffffff,  0, ARM_PARAM_TYPE_UINT, "swi number" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t data_imm_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01e00000, 21, ARM_PARAM_TYPE_DATA_OP, "opcode" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "rotate" },
	{ 0x000000ff,  0, ARM_PARAM_TYPE_UINT, "immediate" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t ls_imm_off_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "B" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "L" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000fff,  0, ARM_PARAM_TYPE_UINT, "immediate" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t ls_multi_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "L" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000ffff,  0, ARM_PARAM_TYPE_REGLIST, "register list" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t branch_link_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "L" },
	{ 0x00ffffff,  0, ARM_PARAM_TYPE_UINT, "offset" },
	{ 0, 0, 0, NULL }
};

static const arm_param_pattern_t cp_ls_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "P" },
	{ 0x00800000, 23, ARM_PARAM_TYPE_UINT, "U" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "N" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "W" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "L" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_CPREG, "CRd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "cp_num" },
	{ 0x000000ff,  0, ARM_PARAM_TYPE_UINT, "offset" },
	{ 0, 0, 0, NULL }
};


static const arm_instr_pattern_t instr_pattern[] = {
	{ 0x0ffffff0, 0x012fff10,
	  ARM_INSTR_TYPE_BRANCH_XCHG, branch_xchg_pattern },
	{ 0x0ffffff0, 0x012fff30,
	  ARM_INSTR_TYPE_BRANCH_LINK_XCHG, branch_xchg_pattern },
	{ 0x0fbf0fff, 0x010f0000,
	  ARM_INSTR_TYPE_MOVE_STATUS_REG, move_status_reg_pattern },
	{ 0x0fff0ff0, 0x016f0f10,
	  ARM_INSTR_TYPE_CLZ, clz_pattern },
	{ 0x0fb0fff0, 0x0120f000,
	  ARM_INSTR_TYPE_MOVE_REG_STATUS, move_reg_status_pattern },
	{ 0x0fb00ff0, 0x01000090,
	  ARM_INSTR_TYPE_SWAP, swap_pattern },
	{ 0x0f900ff0, 0x01000050,
	  ARM_INSTR_TYPE_DSP_ADD_SUB, dsp_add_sub_pattern },
	{ 0x0ff000f0, 0x01200070,
	  ARM_INSTR_TYPE_BKPT, bkpt_pattern },
	{ 0x0e400ff0, 0x000000b0,
	  ARM_INSTR_TYPE_LS_HWORD_REG_OFF, ls_hword_reg_off_pattern },
	{ 0x0e500fd0, 0x000000d0,
	  ARM_INSTR_TYPE_LS_TWO_REG_OFF, ls_two_reg_off_pattern },
	{ 0x0e500fd0, 0x001000d0,
	  ARM_INSTR_TYPE_L_SIGNED_REG_OFF, l_signed_reg_off_pattern },
	{ 0x0fb0f000, 0x0320f000,
	  ARM_INSTR_TYPE_MOVE_IMM_STATUS, move_imm_status_pattern },
	{ 0x0fc000f0, 0x00000090,
	  ARM_INSTR_TYPE_MUL, mul_pattern },
	{ 0x0f8000f0, 0x00800090,
	  ARM_INSTR_TYPE_MUL_LONG, mul_long_pattern },
	{ 0x0f900090, 0x01000080,
	  ARM_INSTR_TYPE_DSP_MUL, dsp_mul_pattern },
	{ 0xff000000, 0xff000000,
	  ARM_INSTR_TYPE_UNDEF_5, NULL },
	{ 0x0e4000f0, 0x004000b0,
	  ARM_INSTR_TYPE_LS_HWORD_IMM_OFF, ls_hword_imm_off_pattern },
	{ 0x0e5000d0, 0x004000d0,
	  ARM_INSTR_TYPE_LS_TWO_IMM_OFF, ls_two_imm_off_pattern },
	{ 0x0e5000d0, 0x005000d0,
	  ARM_INSTR_TYPE_L_SIGNED_IMM_OFF, l_signed_imm_off_pattern },
	{ 0x0fb00000, 0x03000000,
	  ARM_INSTR_TYPE_UNDEF_1, NULL },
	{ 0xfe000000, 0xf8000000,
	  ARM_INSTR_TYPE_UNDEF_4, NULL },
	{ 0xfe000000, 0xfa000000,
	  ARM_INSTR_TYPE_BRANCH_LINK_THUMB, branch_link_thumb_pattern },
	{ 0x0e000090, 0x00000010,
	  ARM_INSTR_TYPE_DATA_REG_SHIFT, data_reg_shift_pattern },
	{ 0xf8000000, 0xf0000000,
	  ARM_INSTR_TYPE_UNDEF_3, NULL },
	{ 0x0f000010, 0x0e000000,
	  ARM_INSTR_TYPE_CP_DATA, cp_data_pattern },
	{ 0x0f000010, 0x0e000010,
	  ARM_INSTR_TYPE_CP_REG, cp_reg_pattern },
	{ 0x0e000010, 0x00000000,
	  ARM_INSTR_TYPE_DATA_IMM_SHIFT, data_imm_shift_pattern },
	{ 0x0e000010, 0x06000000,
	  ARM_INSTR_TYPE_LS_REG_OFF, ls_reg_off_pattern },
	{ 0x0e000010, 0x06000010,
	  ARM_INSTR_TYPE_UNDEF_2, NULL },
	{ 0x0f000000, 0x0f000000,
	  ARM_INSTR_TYPE_SWI, swi_pattern },
	{ 0x0e000000, 0x02000000,
	  ARM_INSTR_TYPE_DATA_IMM, data_imm_pattern },
	{ 0x0e000000, 0x04000000,
	  ARM_INSTR_TYPE_LS_IMM_OFF, ls_imm_off_pattern },
	{ 0x0e000000, 0x08000000,
	  ARM_INSTR_TYPE_LS_MULTI, ls_multi_pattern },
	{ 0x0e000000, 0x0a000000,
	  ARM_INSTR_TYPE_BRANCH_LINK, branch_link_pattern },
	{ 0x0e000000, 0x0c000000,
	  ARM_INSTR_TYPE_CP_LS, cp_ls_pattern },

	{ 0, 0, 0, NULL }
};


const arm_instr_pattern_t *
arm_instr_get_instr_pattern(arm_instr_t instr)
{
	int i;
	for (i = 0; instr_pattern[i].mask != 0; i++) {
		if ((instr & instr_pattern[i].mask) ==
		    instr_pattern[i].value) {
			return &instr_pattern[i];
		}
	}

	return NULL;
}

const arm_param_pattern_t *
arm_instr_get_param_pattern(const arm_instr_pattern_t *ip, unsigned int param)
{
	if (ip->param == NULL) return NULL;

	const arm_param_pattern_t *pp = &ip->param[param];

	if (pp->mask == 0) return NULL;

	return pp;
}

unsigned int
arm_instr_get_param(arm_instr_t instr,
		    const arm_instr_pattern_t *ip, unsigned int param)
{
	const arm_param_pattern_t *pp = &ip->param[param];
	return (instr & pp->mask) >> pp->shift;
}

int
arm_instr_get_params(arm_instr_t instr, const arm_instr_pattern_t *ip,
		     unsigned int params, ...)
{
	va_list ap;
	va_start(ap, params);

	const arm_param_pattern_t *pp = ip->param;

	for (int i = 0; i < params; i++) {
		if (pp[i].mask == 0) {
			va_end(ap);
			return -1;
		}

		int *p = va_arg(ap, int *);
		if (p != NULL) *p = (pp[i].mask & instr) >> pp[i].shift;
	}

	va_end(ap);
	return 0;
}

unsigned int
arm_instr_branch_target(int offset, unsigned int address)
{
	if (offset & 0x800000) {
		offset = -1 * (((~offset) + 1) & 0xffffff);
	}
	return (offset << 2) + address + 0x8;
}


void
arm_instr_fprint(FILE *f, arm_instr_t instr, unsigned int address,
		 char *(*addr_string)(unsigned int addr))
{
	int ret;

	const arm_instr_pattern_t *ip = arm_instr_get_instr_pattern(instr);

	if (ip == NULL) {
		fprintf(f, "undefined\n");
		return;
	}

	if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT) {
		int cond, opcode, s, rn, rd, sha, sh, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &opcode, &s,
					   &rn, &rd, &sha, &sh, &rm);
		if (ret < 0) abort();

		fprintf(f, "%s%s%s\t", data_opcode_map[opcode],
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			((s && (opcode < ARM_DATA_OPCODE_TST ||
				opcode > ARM_DATA_OPCODE_CMN)) ? "s" : ""));

		if (opcode >= ARM_DATA_OPCODE_TST &&
		    opcode <= ARM_DATA_OPCODE_CMN) {
			fprintf(f, "r%d, r%d", rn, rm);
		} else if (opcode == ARM_DATA_OPCODE_MOV ||
			   opcode == ARM_DATA_OPCODE_MVN) {
			fprintf(f, "r%d, r%d", rd, rm);
		} else {
			fprintf(f, "r%d, r%d, r%d", rd, rn, rm);
		}

		if (sh == ARM_DATA_SHIFT_LSR ||
		    sh == ARM_DATA_SHIFT_ASR) {
			sha = (sha ? sha : 32);
		}

		if (sha > 0) {
			fprintf(f, ", %s #%s%x", data_shift_map[sh],
				(sha < 10 ? "" : "0x"), sha);
		} else if (sh == ARM_DATA_SHIFT_ROR) {
			fprintf(f, ", rrx");
		}
	} else if (ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT) {
		int cond, opcode, s, rn, rd, rs, sh, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &opcode, &s,
					   &rn, &rd, &rs, &sh, &rm);
		if (ret < 0) abort();

		fprintf(f, "%s%s%s\t", data_opcode_map[opcode],
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			((s && (opcode < ARM_DATA_OPCODE_TST ||
				opcode > ARM_DATA_OPCODE_CMN)) ? "s" : ""));

		if (opcode >= ARM_DATA_OPCODE_TST &&
		    opcode <= ARM_DATA_OPCODE_CMN) {
			fprintf(f, "r%d, r%d", rn, rm);
		} else if (opcode == ARM_DATA_OPCODE_MOV ||
			   opcode == ARM_DATA_OPCODE_MVN) {
			fprintf(f, "r%d, r%d", rd, rm);
		} else {
			fprintf(f, "r%d, r%d, r%d", rd, rn, rm);
		}

		fprintf(f, ", %s r%d", data_shift_map[sh], rs);
	} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM) {
		int cond, opcode, s, rn, rd, rot, imm;
		ret = arm_instr_get_params(instr, ip, 7, &cond, &opcode, &s,
					   &rn, &rd, &rot, &imm);
		if (ret < 0) abort();

		fprintf(f, "%s%s%s\t", data_opcode_map[opcode],
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			((s && (opcode < ARM_DATA_OPCODE_TST ||
				opcode > ARM_DATA_OPCODE_CMN)) ? "s" : ""));

		if (opcode >= ARM_DATA_OPCODE_TST &&
		    opcode <= ARM_DATA_OPCODE_CMN) {
			fprintf(f, "r%d", rn);
		} else if (opcode == ARM_DATA_OPCODE_MOV || 
			   opcode == ARM_DATA_OPCODE_MVN) {
			fprintf(f, "r%d", rd);
		} else {
			fprintf(f, "r%d, r%d", rd, rn);
		}

		unsigned int rot_imm =
			(imm >> (rot << 1)) | (imm << (32 - (rot << 1)));

		fprintf(f, ", #%s%x", (rot_imm < 10 ? "" : "0x"), rot_imm);
	} else if (ip->type == ARM_INSTR_TYPE_UNDEF_1 ||
		   ip->type == ARM_INSTR_TYPE_UNDEF_2 ||
		   ip->type == ARM_INSTR_TYPE_UNDEF_3 ||
		   ip->type == ARM_INSTR_TYPE_UNDEF_4 ||
		   ip->type == ARM_INSTR_TYPE_UNDEF_5) {
		fprintf(f, "undefined");
	} else if (ip->type == ARM_INSTR_TYPE_SWI) {
		int cond, imm;
		ret = arm_instr_get_params(instr, ip, 2, &cond, &imm);
		if (ret < 0) abort();

		fprintf(f, "swi%s\t0x%x",
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			imm);
	} else if (ip->type == ARM_INSTR_TYPE_CLZ) {
		int cond, rd, rm;
		ret = arm_instr_get_params(instr, ip, 3, &cond, &rd, &rm);
		if (ret < 0) abort();

		fprintf(f, "clz%s\t, r%d, r%m",
			(cond != ARM_COND_AL ? cond_map[cond] : ""), rd, rm);
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_IMM_STATUS) {
		int cond, r, mask, rot, imm;
		ret = arm_instr_get_params(instr, ip, 5, &cond, &r, &mask,
					   &rot, &imm);
		if (ret < 0) abort();

		fprintf(f, "msr%s\t%s_%s%s%s%s",
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(r ? "SPSR" : "CPSR"), ((mask & 1) ? "c" : ""),
			((mask & 2) ? "x" : ""), ((mask & 4) ? "s" : ""),
			((mask & 8) ? "f" : ""));

		unsigned int rot_imm =
			(imm >> (rot << 1)) | (imm << (32 - (rot << 1)));

		fprintf(f, ", #%s%x", (rot_imm < 10 ? "" : "0x"), rot_imm);
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_REG_STATUS) {
		int cond, r, mask, rm;
		ret = arm_instr_get_params(instr, ip, 4, &cond, &r, &mask,
					   &rm);
		if (ret < 0) abort();

		fprintf(f, "msr%s\t%s_%s%s%s%s",
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(r ? "SPSR" : "CPSR"), ((mask & 1) ? "c" : ""),
			((mask & 2) ? "x" : ""), ((mask & 4) ? "s" : ""),
			((mask & 8) ? "f" : ""));

		fprintf(f, ", r%d", rm);
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_STATUS_REG) {
		int cond, r, rd;
		ret = arm_instr_get_params(instr, ip, 3, &cond, &r, &rd);
		if (ret < 0) abort();

		fprintf(f, "mrs%s\tr%d, %s",
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			rd, (r ? "SPSR" : "CPSR"));
	} else if (ip->type == ARM_INSTR_TYPE_LS_IMM_OFF) {
		int cond, p, u, b, w, load, rn, rd, imm;
		ret = arm_instr_get_params(instr, ip, 9, &cond, &p, &u, &b, &w,
					   &load, &rn, &rd, &imm);
		if (ret < 0) abort();

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
	} else if (ip->type == ARM_INSTR_TYPE_LS_REG_OFF) {
		int cond, p, u, b, w, load, rn, rd, sha, sh, rm;
		ret = arm_instr_get_params(instr, ip, 11, &cond, &p, &u, &b,
					   &w, &load, &rn, &rd, &sha, &sh,
					   &rm);
		if (ret < 0) abort();

		fprintf(f, "%sr%s%s%s\tr%d, [r%d",
			(load ? "ld" : "st"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(b ? "b" : ""), ((!p && w) ? "t" : ""), rd, rn);

		if (!p) fprintf(f, "]");

		fprintf(f, ", %sr%d", (u ? "" : "-"), rm);

		if (sh == ARM_DATA_SHIFT_LSR ||
		    sh == ARM_DATA_SHIFT_ASR) {
			sha = (sha ? sha : 32);
		}

		if (sha > 0) {
			fprintf(f, ", %s #%s%x", data_shift_map[sh],
				(sha < 10 ? "" : "0x"), sha);
		} else if (sh == ARM_DATA_SHIFT_ROR) {
			fprintf(f, ", rrx");
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
		int cond, link, offset;
		ret = arm_instr_get_params(instr, ip, 3, &cond, &link,
					   &offset);
		if (ret < 0) abort();

		unsigned int target = arm_instr_branch_target(offset, address);

		char *targetstr = addr_string(target);
		if (targetstr == NULL) abort();

		fprintf(f, "b%s%s\t%s",
			(link ? "l" : ""),
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			targetstr);
	} else if (ip->type == ARM_INSTR_TYPE_BKPT) {
		int imm_hi, imm_lo;
		ret = arm_instr_get_params(instr, ip, 2, &imm_hi, &imm_lo);
		if (ret < 0) abort();

		fprintf(f, "bkpt\t0x%x",
			((imm_hi & 0xfff) << 4) | (imm_lo & 0xf));
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK_THUMB) {
		int h, offset;
		ret = arm_instr_get_params(instr, ip, 2, &h, &offset);
		if (ret < 0) abort();

		unsigned int target = arm_instr_branch_target(offset, address);

		char *targetstr = addr_string(target);
		if (targetstr == NULL) abort();

		fprintf(f, "blx\t%s", targetstr);
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_XCHG) {
		int cond, rm;
		ret = arm_instr_get_params(instr, ip, 2, &cond, &rm);
		if (ret < 0) abort();

		fprintf(f, "bx%s\tr%d",
			(cond != ARM_COND_AL ? cond_map[cond] : ""), rm);
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK_XCHG) {
		int cond, rm;
		ret = arm_instr_get_params(instr, ip, 2, &cond, &rm);
		if (ret < 0) abort();

		fprintf(f, "blx%s\tr%d",
			(cond != ARM_COND_AL ? cond_map[cond] : ""), rm);
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_XCHG) {
		int cond, rm;
		ret = arm_instr_get_params(instr, ip, 2, &cond, &rm);
		if (ret < 0) abort();

		fprintf(f, "bx%s\tr%d",
			(cond != ARM_COND_AL ? cond_map[cond] : ""), rm);
	} else if (ip->type == ARM_INSTR_TYPE_CP_DATA) {
		int cond, opcode_1, crn, crd, cp_num, opcode_2, crm;
		ret = arm_instr_get_params(instr, ip, 7, &cond, &opcode_1,
					   &crn, &crd, &cp_num, &opcode_2,
					   &crm);
		if (ret < 0) abort();

		fprintf(f, "cdp%s\tp%d, %d, cr%d, cr%d, cr%d, %d",
			(cond != ARM_COND_NV ?
			 (cond != ARM_COND_AL ? cond_map[cond] : "") : "2"),
			cp_num, opcode_1, crd, crn, crm, opcode_2);
	} else if (ip->type == ARM_INSTR_TYPE_CP_LS) {
		int cond, p, u, n, w, load, rn, crd, cp_num, offset;
		ret = arm_instr_get_params(instr, ip, 10, &cond, &p, &u, &n,
					   &w, &load, &rn, &crd, &cp_num,
					   &offset);
		if (ret < 0) abort();

		fprintf(f, "%sc%s%s\tp%d, cr%d, [r%d",
			(load ? "ld" : "st"),
			(cond != ARM_COND_NV ?
			 (cond != ARM_COND_AL ? cond_map[cond] : "") : "2"),
			(n ? "l" : ""), cp_num, crd, rn);

		if (!p) fprintf(f, "]");

		if (!(u || p)) fprintf(f, ", {%d}", offset);
		else if (offset > 0) {
			fprintf(f, ", #%s%s%x", (u ? "" : "-"),
				((offset << 2) < 10 ? "" : "0x"),
				(offset << 2));
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_CP_REG) {
		int cond, opcode_1, load, crn, rd, cp_num, opcode_2, crm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &opcode_1,
					   &load, &crn, &rd, &cp_num,
					   &opcode_2, &crm);
		if (ret < 0) abort();

		fprintf(f, "m%s%s\tp%d, %d, r%d, cr%d, cr%d, %d",
			(load ? "rc" : "cr"),
			(cond != ARM_COND_NV ?
			 (cond != ARM_COND_AL ? cond_map[cond] : "") : "2"),
			cp_num, opcode_1, rd, crn, crm, opcode_2);
	} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
		int cond, p, u, s, w, load, rn, reglist;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &p, &u, &s, &w,
					   &load, &rn, &reglist);
		if (ret < 0) abort();
		
		fprintf(f, "%sm%s%s%s\tr%d%s, {", (load ? "ld" : "st"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(u ? "i" : "d"), (p ? "b" : "a"),
			rn, (w ? "!" : ""));

		int comma = 0;
		int range_start = -1;
		for (int i = 0; reglist; i++) {
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

		fprintf(f, " }%s", (s ? "^" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_MUL) {
		int cond, a, s, rd, rn, rs, rm;
		ret = arm_instr_get_params(instr, ip, 7, &cond, &a, &s, &rd,
					   &rn, &rs, &rm);
		if (ret < 0) abort();

		fprintf(f, "m%s%s%s\tr%d, r%d, r%d", (a ? "la" : "ul"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(s ? "s" : ""), rd, rm, rs);

		if (a) fprintf(f, ", r%d", rn);
	} else if (ip->type == ARM_INSTR_TYPE_MUL_LONG) {
		int cond, u, a, s, rdhi, rdlo, rs, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &u, &a, &s,
					   &rdhi, &rdlo, &rs, &rm);
		if (ret < 0) abort();

		fprintf(f, "%sm%sl%s%s\tr%d, r%d, r%d, r%d",
			(u ? "s" : "u"), (a ? "la" : "ul"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(s ? "s" : ""), rdlo, rdhi, rm, rs);
	} else if (ip->type == ARM_INSTR_TYPE_SWAP) {
		int cond, b, rn, rd, rm;
		ret = arm_instr_get_params(instr, ip, 5, &cond, &b, &rn, &rd,
					   &rm);
		if (ret < 0) abort();

		fprintf(f, "swp%s%s\tr%d, r%d, [r%d]",
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(b ? "b" : ""), rd, rm, rn);
	} else if (ip->type == ARM_INSTR_TYPE_LS_HWORD_REG_OFF) {
		int cond, p, u, w, load, rn, rd, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &p, &u, &w,
					   &load, &rn, &rd, &rm);
		if (ret < 0) abort();

		fprintf(f, "%sr%sh\tr%d, [r%d", (load ? "ld" : "st"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""), rd, rn);

		if (!p) fprintf(f, "]");

		fprintf(f, ", %sr%d", (u ? "" : "-"), rm);

		if (p) fprintf(f, "]%s", (w ? "!" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_LS_HWORD_IMM_OFF) {
		int cond, p, u, w, load, rn, rd, off_hi, off_lo;
		ret = arm_instr_get_params(instr, ip, 9, &cond, &p, &u, &w,
					   &load, &rn, &rd, &off_hi, &off_lo);
		if (ret < 0) abort();

		fprintf(f, "%sr%sh\tr%d, [r%d", (load ? "ld" : "st"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""), rd, rn);

		if (!p) fprintf(f, "]");

		if (off_hi || off_lo) {
			int off = (off_hi << 4) | off_lo;
			fprintf(f, ", #%s%s%x", (u ? "" : "-"),
				(off < 10 ? "" : "0x"), off);
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_LS_TWO_REG_OFF) {
		int cond, p, u, w, rn, rd, store, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &p, &u, &w,
					   &rn, &rd, &store, &rm);
		if (ret < 0) abort();

		fprintf(f, "%sr%sd\tr%d, [r%d", (store ? "st" : "ld"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			rd, rn);

		if (!p) fprintf(f, "]");

		fprintf(f, ", %sr%d", (u ? "" : "-"), rm);

		if (p) fprintf(f, "]%s", (w ? "!" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_LS_TWO_IMM_OFF) {
		int cond, p, u, w, rn, rd, off_hi, store, off_lo;
		ret = arm_instr_get_params(instr, ip, 9, &cond, &p, &u, &w,
					   &rn, &rd, &off_hi, &store, &off_lo);
		if (ret < 0) abort();

		fprintf(f, "%sr%sd\tr%d, [r%d", (store ? "st" : "ld"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""), rd, rn);

		if (!p) fprintf(f, "]");

		if (off_hi || off_lo) {
			int off = (off_hi << 4) | off_lo;
			fprintf(f, ", #%s%s%x", (u ? "" : "-"),
				(off < 10 ? "" : "0x"), off);
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_L_SIGNED_REG_OFF) {
		int cond, p, u, w, rn, rd, h, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &p, &u, &w,
					   &rn, &rd, &h, &rm);
		if (ret < 0) abort();

		fprintf(f, "ldr%ss%s\tr%d, [r%d",
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(h ? "h" : "b"), rd, rn);

		if (!p) fprintf(f, "]");

		fprintf(f, ", %sr%d", (u ? "" : "-"), rm);

		if (p) fprintf(f, "]%s", (w ? "!" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_L_SIGNED_IMM_OFF) {
		int cond, p, u, w, rn, rd, off_hi, h, off_lo;
		ret = arm_instr_get_params(instr, ip, 9, &cond, &p, &u, &w,
					   &rn, &rd, &off_hi, &h, &off_lo);
		if (ret < 0) abort();

		fprintf(f, "ldr%ss%s\tr%d, [r%d",
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			(h ? "h" : "b"), rd, rn);

		if (!p) fprintf(f, "]");

		if (off_hi || off_lo) {
			int off = (off_hi << 4) | off_lo;
			fprintf(f, ", #%s%s%x", (u ? "" : "-"),
				(off < 10 ? "" : "0x"), off);
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_DSP_ADD_SUB) {
		int cond, op, rn, rd, rm;
		ret = arm_instr_get_params(instr, ip, 5, &cond, &op, &rn, &rd,
					   &rm);
		if (ret < 0) abort();

		fprintf(f, "q%s%s%s\tr%d, r%d, r%d", ((op & 2) ? "d" : ""),
			((op & 1) ? "sub" : "add"),
			(cond != ARM_COND_AL ? cond_map[cond] : ""),
			rd, rm, rn);
	} else if (ip->type == ARM_INSTR_TYPE_DSP_MUL) {
		int cond, op, rd, rn, rs, y, x, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &op, &rd, &rn,
					   &rs, &y, &x, &rm);
		if (ret < 0) abort();

		switch (op) {
		case 0:
			fprintf(f, "smla%s%s%s\tr%d, r%d, r%d, r%d",
				(x ? "t" : "b"), (y ? "t" : "b"),
				(cond != ARM_COND_AL ? cond_map[cond] : ""),
				rd, rm, rs, rn);
			break;
		case 1:
			fprintf(f, "s%sw%s%s\tr%d, r%d, r%d",
				(x ? "mul" : "mla"), (y ? "t" : "b"),
				(cond != ARM_COND_AL ? cond_map[cond] : ""));
			if (!x) fprintf(f, ", r%d", rn);
			break;
		case 2:
			fprintf(f, "smlal%s%s%s\tr%d, r%d, r%d, r%d",
				(x ? "t" : "b"), (y ? "t" : "b"),
				(cond != ARM_COND_AL ? cond_map[cond] : ""),
				rn, rd, rm, rs);
			break;
		case 3:
			fprintf(f, "smul%s%s%s\tr%d, r%d, r%d",
				(x ? "t" : "b"), (y ? "t" : "b"),
				(cond != ARM_COND_AL ? cond_map[cond] : ""),
				rd, rm, rs);
			break;
		default:
			abort();
		}
	} else {
		fprintf(f, "undefined", instr);
	}

	fprintf(f, "\n");
}
