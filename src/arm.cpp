/* arm.cpp */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cassert>

#include <map>
#include <iostream>

#include "arm.hh"
#include "image.hh"

using namespace std;


static const arm_param_pattern_t branch_xchg_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t clz_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t bkpt_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x000fff00,  8, ARM_PARAM_TYPE_UINT, "immediateHi" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_UINT, "immediateLo" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t move_status_reg_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "R" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t move_reg_status_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "R" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_UINT, "mask" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t swap_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "B" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },	
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t dsp_add_sub_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00600000, 21, ARM_PARAM_TYPE_UINT, "op" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t mul_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00200000, 21, ARM_PARAM_TYPE_UINT, "A" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_REG, "Rs" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_REG, "Rm" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t move_imm_status_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00400000, 22, ARM_PARAM_TYPE_UINT, "R" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_UINT, "mask" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "rotate" },
	{ 0x000000ff,  0, ARM_PARAM_TYPE_UINT, "immediate" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t branch_link_thumb_pattern[] = {
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "H" },
	{ 0x00ffffff,  0, ARM_PARAM_TYPE_UINT, "offset" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t cp_data_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00f00000, 20, ARM_PARAM_TYPE_UINT, "opcode1" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_CPREG, "CRn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_CPREG, "CRd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "cp_num" },
	{ 0x000000e0,  5, ARM_PARAM_TYPE_UINT, "opcode2" },
	{ 0x0000000f,  0, ARM_PARAM_TYPE_CPREG, "CRm" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t swi_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x00ffffff,  0, ARM_PARAM_TYPE_UINT, "swi number" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t data_imm_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01e00000, 21, ARM_PARAM_TYPE_DATA_OP, "opcode" },
	{ 0x00100000, 20, ARM_PARAM_TYPE_UINT, "S" },
	{ 0x000f0000, 16, ARM_PARAM_TYPE_REG, "Rn" },
	{ 0x0000f000, 12, ARM_PARAM_TYPE_REG, "Rd" },
	{ 0x00000f00,  8, ARM_PARAM_TYPE_UINT, "rotate" },
	{ 0x000000ff,  0, ARM_PARAM_TYPE_UINT, "immediate" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
};

static const arm_param_pattern_t branch_link_pattern[] = {
	{ 0xf0000000, 28, ARM_PARAM_TYPE_COND, "cond" },
	{ 0x01000000, 24, ARM_PARAM_TYPE_UINT, "L" },
	{ 0x00ffffff,  0, ARM_PARAM_TYPE_UINT, "offset" },
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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
	{ 0, 0, ARM_PARAM_TYPE_NONE, NULL }
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

	{ 0, 0, ARM_INSTR_TYPE_NONE, NULL }
};


static char *
arm_addr_simple_string(arm_addr_t addr)
{
	char *addrstr = NULL;
	int r;

	static const char *format = "0x%x";

	r = snprintf(NULL, 0, format, addr);
	if (r > 0) {
		addrstr = new char[r+1];
		if (addrstr == NULL) abort();
		r = snprintf(addrstr, r+1, format, addr);
		if (r <= 0) {
			delete addrstr;
			return NULL;
		}
	} else {
		return NULL;
	}

	return addrstr;
}

char *
arm_addr_string(arm_addr_t addr, map<arm_addr_t, char *> *sym_map)
{
	char *addrstr = NULL;
	int r;

	if (sym_map != NULL) {
		map<arm_addr_t, char *>::iterator sym;
		sym = sym_map->find(addr);

		if (sym == sym_map->end()) {
			return arm_addr_simple_string(addr);
		}

		static const char *format = "<%s:0x%x>";
		arm_addr_t sym_addr = sym->first;
		const char *sym_name = sym->second;

		r = snprintf(NULL, 0, format, sym_name, sym_addr,
			     addr - sym_addr);
		if (r > 0) {
			addrstr = new char[r+1];
			if (addrstr == NULL) abort();
			r = snprintf(addrstr, r+1, format, sym_name, sym_addr,
				     addr - sym_addr);
			if (r <= 0) {
				delete addrstr;
				return NULL;
			}
		} else {
			return NULL;
		}
	} else {
		return arm_addr_simple_string(addr);
	}

	return addrstr;
}

const arm_instr_pattern_t *
arm_instr_get_instr_pattern(arm_instr_t instr)
{
	int i;
	for (i = 0; instr_pattern[i].type != ARM_INSTR_TYPE_NONE; i++) {
		if ((instr & instr_pattern[i].mask) ==
		    instr_pattern[i].value) {
			return &instr_pattern[i];
		}
	}

	return NULL;
}

const arm_param_pattern_t *
arm_instr_get_param_pattern(const arm_instr_pattern_t *ip, uint_t param)
{
	if (ip->param == NULL) return NULL;

	const arm_param_pattern_t *pp = &ip->param[param];

	if (pp->mask == 0) return NULL;

	return pp;
}

uint_t
arm_instr_get_param(arm_instr_t instr,
		    const arm_instr_pattern_t *ip, uint_t param)
{
	assert(ip->param != NULL);
	const arm_param_pattern_t *pp = &ip->param[param];
	return (instr & pp->mask) >> pp->shift;
}

int
arm_instr_get_params(arm_instr_t instr, const arm_instr_pattern_t *ip,
		     uint_t params, ...)
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

int
arm_instr_get_cond(arm_instr_t instr, arm_cond_t *cond)
{
	const arm_instr_pattern_t *ip =
		arm_instr_get_instr_pattern(instr);
	if (ip == NULL) return -1;

	switch (ip->type) {
	ARM_INSTR_TYPE_BRANCH_LINK_THUMB:
		*cond = ARM_COND_AL;
		break;
	default:
		if (ip->param != NULL) {
			*cond = static_cast<arm_cond_t>
				(arm_instr_get_param(instr, ip, 0));
		} else return -1;
		break;
	}

	return 0;
}

static int
arm_instr_get_type(arm_instr_t instr, arm_instr_type_t *type)
{
	const arm_instr_pattern_t *ip =
		arm_instr_get_instr_pattern(instr);
	if (ip == NULL) return -1;

	*type = ip->type;

	return 0;
}

int
arm_instr_is_unpredictable(arm_instr_t instr, arm_addr_t addr,
			   bool *unpredictable)
{
	const arm_instr_pattern_t *ip =
		arm_instr_get_instr_pattern(instr);
	if (ip == NULL) {
		*unpredictable = true;
		return 0;
	}

	*unpredictable = false;

	if (ip->type == ARM_INSTR_TYPE_CLZ) {
		int rd = arm_instr_get_param(instr, ip, 1);
		int rm = arm_instr_get_param(instr, ip, 2);

		if (rd == 15 || rm == 15) *unpredictable = true;
	} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
		int rn = arm_instr_get_param(instr, ip, 6);
		int reglist = arm_instr_get_param(instr, ip, 7);

		if (rn == 15 || reglist == 0) *unpredictable = true;
	} else if (ip->type == ARM_INSTR_TYPE_UNDEF_1 ||
		   ip->type == ARM_INSTR_TYPE_UNDEF_2 ||
		   ip->type == ARM_INSTR_TYPE_UNDEF_3 ||
		   ip->type == ARM_INSTR_TYPE_UNDEF_4 ||
		   ip->type == ARM_INSTR_TYPE_UNDEF_5) {
		*unpredictable = true;
	}

	return 0;
}

arm_addr_t
arm_instr_branch_target(int offset, arm_addr_t addr)
{
	if (offset & 0x800000) {
		offset = -1 * (((~offset) + 1) & 0xffffff);
	}
	return (offset << 2) + addr + 0x8;
}

int
arm_instr_is_reg_used(arm_instr_t instr, uint_t reg)
{
	int r;
	uint_t reglist;
	r = arm_instr_used_regs(instr, &reglist);
	if (r < 0) return -1;

	return (reglist & (1 << reg));
}

int
arm_instr_is_reg_changed(arm_instr_t instr, uint_t reg)
{
	int r;
	uint_t reglist;
	r = arm_instr_changed_regs(instr, &reglist);
	if (r < 0) return -1;

	return (reglist & (1 << reg));
}

int
arm_instr_is_flag_used(arm_instr_t instr, uint_t flag)
{
	int r;
	uint_t flags;
	r = arm_instr_used_flags(instr, &flags);
	if (r < 0) return -1;

	return (flags & (1 << flag));
}

int
arm_instr_is_flag_changed(arm_instr_t instr, uint_t flag)
{
	int r;
	uint_t flags;
	r = arm_instr_changed_flags(instr, &flags);
	if (r < 0) return -1;

	return (flags & (1 << flag));
}

int
arm_instr_used_regs(arm_instr_t instr, uint_t *reglist)
{
	const arm_instr_pattern_t *ip =
		arm_instr_get_instr_pattern(instr);
	if (ip == NULL) return -1;

	*reglist = ARM_REG_MASK;

	if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int opcode = arm_instr_get_param(instr, ip, 1);
		int rn = arm_instr_get_param(instr, ip, 3);
		int rm = arm_instr_get_param(instr, ip, 7);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist = (1 << rm);
			if (opcode != ARM_DATA_OPCODE_MOV &&
			    opcode != ARM_DATA_OPCODE_MVN) {
				*reglist |= (1 << rn);
			}
		}
	} else if (ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int opcode = arm_instr_get_param(instr, ip, 1);
		int rn = arm_instr_get_param(instr, ip, 3);
		int rs = arm_instr_get_param(instr, ip, 5);
		int rm = arm_instr_get_param(instr, ip, 7);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist = (1 << rs) | (1 << rm);
			if (opcode != ARM_DATA_OPCODE_MOV &&
			    opcode != ARM_DATA_OPCODE_MVN) {
				*reglist |= (1 << rn);
			}
		}
	} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int opcode = arm_instr_get_param(instr, ip, 1);
		int rn = arm_instr_get_param(instr, ip, 3);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			if (opcode != ARM_DATA_OPCODE_MOV &&
			    opcode != ARM_DATA_OPCODE_MVN) {
				*reglist = (1 << rn);
			}
		}
	} else if (ip->type == ARM_INSTR_TYPE_SWI) {
		*reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_CLZ) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int rm = arm_instr_get_param(instr, ip, 2);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist = (1 << rm);
		}
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_IMM_STATUS) {
		*reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_REG_STATUS) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int rm = arm_instr_get_param(instr, ip, 3);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist = (1 << rm);
		}
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_STATUS_REG) {
		*reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_LS_IMM_OFF) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int load = arm_instr_get_param(instr, ip, 5);
		int rn = arm_instr_get_param(instr, ip, 6);
		int rd = arm_instr_get_param(instr, ip, 7);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist = (1 << rn);
			if (!load) *reglist |= (1 << rd);
		}
	} else if (ip->type == ARM_INSTR_TYPE_LS_REG_OFF) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int load = arm_instr_get_param(instr, ip, 5);
		int rn = arm_instr_get_param(instr, ip, 6);
		int rd = arm_instr_get_param(instr, ip, 7);
		int rm = arm_instr_get_param(instr, ip, 10);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist = (1 << rn) | (1 << rm);
			if (!load) *reglist |= (1 << rd);
		}
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
		*reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_BKPT) {
		*reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK_THUMB) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_XCHG) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK_XCHG) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_CP_DATA) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_CP_LS) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_CP_REG) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int load = arm_instr_get_param(instr, ip, 5);
		int rn = arm_instr_get_param(instr, ip, 6);
		int rlist = arm_instr_get_param(instr, ip, 7);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist = (1 << rn);
			if (!load) *reglist |= rlist;
		}
	} else if (ip->type == ARM_INSTR_TYPE_MUL) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_MUL_LONG) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_SWAP) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_LS_HWORD_REG_OFF) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int load = arm_instr_get_param(instr, ip, 4);
		int rn = arm_instr_get_param(instr, ip, 5);
		int rd = arm_instr_get_param(instr, ip, 6);
		int rm = arm_instr_get_param(instr, ip, 7);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist = (1 << rn) | (1 << rm);
			if (!load) *reglist |= (1 << rd);
		}
	} else if (ip->type == ARM_INSTR_TYPE_LS_HWORD_IMM_OFF) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int load = arm_instr_get_param(instr, ip, 4);
		int rn = arm_instr_get_param(instr, ip, 5);
		int rd = arm_instr_get_param(instr, ip, 6);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist = (1 << rn);
			if (!load) *reglist |= (1 << rd);
		}
	} else if (ip->type == ARM_INSTR_TYPE_LS_TWO_REG_OFF ||
		   ip->type == ARM_INSTR_TYPE_LS_TWO_IMM_OFF) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_L_SIGNED_REG_OFF ||
		   ip->type == ARM_INSTR_TYPE_L_SIGNED_IMM_OFF) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_DSP_ADD_SUB) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_DSP_MUL) {
		/* TODO */
	}

	return 0;
}

int
arm_instr_changed_regs(arm_instr_t instr, uint_t *reglist)
{
	int ret;

	const arm_instr_pattern_t *ip =
		arm_instr_get_instr_pattern(instr);
	if (ip == NULL) return -1;

	*reglist = ARM_REG_MASK;

	if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT ||
	    ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
	    ip->type == ARM_INSTR_TYPE_DATA_IMM) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int opcode = arm_instr_get_param(instr, ip, 1);
		int rd = arm_instr_get_param(instr, ip, 4);

		if ((opcode < ARM_DATA_OPCODE_TST ||
		     opcode > ARM_DATA_OPCODE_CMN) &&
		    cond != ARM_COND_NV) {
			*reglist = (1 << rd);
		} else *reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_SWI) {
		*reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_CLZ) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int rd = arm_instr_get_param(instr, ip, 1);
		if (cond != ARM_COND_NV) *reglist = (1 << rd);
		else *reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_IMM_STATUS ||
		   ip->type == ARM_INSTR_TYPE_MOVE_REG_STATUS) {
		*reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_STATUS_REG) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int rd = arm_instr_get_param(instr, ip, 2);
		if (cond != ARM_COND_NV) *reglist = (1 << rd);
		else *reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_LS_IMM_OFF ||
		   ip->type == ARM_INSTR_TYPE_LS_REG_OFF) {
		int cond, p, w, load, rn, rd;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &p, NULL,
					   NULL, &w, &load, &rn, &rd);
		if (ret < 0) abort();

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			if (load) *reglist |= (1 << rd);
			if (!p || (p && w)) *reglist |= (1 << rn);
		}
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int link = arm_instr_get_param(instr, ip, 1);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			if (link) {
				*reglist |= (1 << 14);
				/* standard calling conventions assumed */
				*reglist |= (1 << 0) | (1 << 1) |
					(1 << 2) | (1 << 3);
			} else {
				*reglist |= (1 << 15);
			}
		}
	} else if (ip->type == ARM_INSTR_TYPE_BKPT) {
		*reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK_THUMB) {
		*reglist = (1 << 14);
		/* standard calling conventions assumed */
		*reglist |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_XCHG) {
		int cond = arm_instr_get_param(instr, ip, 0);
		if (cond != ARM_COND_NV) *reglist = (1 << 15) | (1 << 14);
		else *reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK_XCHG) {
		int cond = arm_instr_get_param(instr, ip, 0);
		if (cond != ARM_COND_NV) {
			*reglist = (1 << 14);
			/* standard calling conventions assumed */
			*reglist |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
		} else {
			*reglist = 0;
		}
	} else if (ip->type == ARM_INSTR_TYPE_CP_DATA) {
		*reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_CP_LS) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int w = arm_instr_get_param(instr, ip, 4);
		int rn = arm_instr_get_param(instr, ip, 6);

		if (cond != ARM_COND_NV && w) {
			*reglist = (1 << rn);
		} else *reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_CP_REG) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int load = arm_instr_get_param(instr, ip, 2);
		int rd = arm_instr_get_param(instr, ip, 4);

		*reglist = 0;
		if (cond != ARM_COND_NV && load) {
			if (rd != 15) *reglist = (1 << rd);
		}
	} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int w = arm_instr_get_param(instr, ip, 4);
		int load = arm_instr_get_param(instr, ip, 5);
		int rn = arm_instr_get_param(instr, ip, 6);
		int rlist = arm_instr_get_param(instr, ip, 7);

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			if (load) *reglist |= rlist;
			if (w) *reglist |= (1 << rn);
		}
	} else if (ip->type == ARM_INSTR_TYPE_MUL) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int rd = arm_instr_get_param(instr, ip, 3);

		if (cond != ARM_COND_NV) {
			*reglist = (1 << rd);
		} else *reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_MUL_LONG) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int rdhi = arm_instr_get_param(instr, ip, 4);
		int rdlo = arm_instr_get_param(instr, ip, 5);

		if (cond != ARM_COND_NV) {
			*reglist = (1 << rdhi) | (1 << rdlo);
		} else *reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_SWAP) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int rd = arm_instr_get_param(instr, ip, 3);

		if (cond != ARM_COND_NV) {
			*reglist = (1 << rd);
		} else *reglist = 0;
	} else if (ip->type == ARM_INSTR_TYPE_LS_HWORD_REG_OFF ||
		   ip->type == ARM_INSTR_TYPE_LS_HWORD_IMM_OFF) {
		int cond, p, w, load, rn, rd;
		ret = arm_instr_get_params(instr, ip, 7, &cond, &p, NULL,
					   &w, &load, &rn, &rd);
		if (ret < 0) abort();

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			if (load) *reglist |= (1 << rd);
			if (!p || (p && w)) *reglist |= (1 << rn);
		}
	} else if (ip->type == ARM_INSTR_TYPE_LS_TWO_REG_OFF ||
		   ip->type == ARM_INSTR_TYPE_LS_TWO_IMM_OFF) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_L_SIGNED_REG_OFF ||
		   ip->type == ARM_INSTR_TYPE_L_SIGNED_IMM_OFF) {
		int cond, p, u, w, rn, rd;
		ret = arm_instr_get_params(instr, ip, 6, &cond, &p, &u, &w,
					   &rn, &rd);
		if (ret < 0) abort();

		*reglist = 0;
		if (cond != ARM_COND_NV) {
			*reglist |= (1 << rd);
			if (!p || (p && w)) *reglist |= (1 << rn);
		}
	} else if (ip->type == ARM_INSTR_TYPE_DSP_ADD_SUB) {
		/* TODO */
	} else if (ip->type == ARM_INSTR_TYPE_DSP_MUL) {
		/* TODO */
	}

	return 0;
}

int
arm_instr_used_flags(arm_instr_t instr, uint_t *flags)
{
	int r;

	const arm_instr_pattern_t *ip =
		arm_instr_get_instr_pattern(instr);
	if (ip == NULL) return -1;

	arm_cond_t cond;
	r = arm_instr_get_cond(instr, &cond);
	if (r < 0) return -1;

	*flags = 0;

	switch (cond) {
	case ARM_COND_EQ:
	case ARM_COND_NE:
		*flags |= (1 << ARM_FLAG_Z);
		break;
	case ARM_COND_CS:
	case ARM_COND_CC:
		*flags |= (1 << ARM_FLAG_C);
		break;
	case ARM_COND_MI:
	case ARM_COND_PL:
		*flags |= (1 << ARM_FLAG_N);
		break;
	case ARM_COND_VS:
	case ARM_COND_VC:
		*flags |= (1 << ARM_FLAG_V);
		break;
	case ARM_COND_HI:
	case ARM_COND_LS:
		*flags |= (1 << ARM_FLAG_C) | (1 << ARM_FLAG_Z);
		break;
	case ARM_COND_GE:
	case ARM_COND_LT:
		*flags |= (1 << ARM_FLAG_N) | (1 << ARM_FLAG_V);
		break;
	case ARM_COND_GT:
	case ARM_COND_LE:
		*flags |= (1 << ARM_FLAG_Z);
		*flags |= (1 << ARM_FLAG_N) | (1 << ARM_FLAG_V);
		break;
	case ARM_COND_AL:
	case ARM_COND_NV:
	default:
		break;
	}

	if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT ||
	    ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
	    ip->type == ARM_INSTR_TYPE_DATA_IMM) {
		int opcode = arm_instr_get_param(instr, ip, 1);

		if (opcode == ARM_DATA_OPCODE_ADC ||
		    opcode == ARM_DATA_OPCODE_SBC ||
		    opcode == ARM_DATA_OPCODE_RSC) {
			*flags |= (1 << ARM_FLAG_C);
		}
	}

	return 0;
}

int
arm_instr_changed_flags(arm_instr_t instr, uint_t *flags)
{
	const arm_instr_pattern_t *ip =
		arm_instr_get_instr_pattern(instr);
	if (ip == NULL) return -1;

	*flags = ARM_FLAG_MASK;

	if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT ||
	    ip->type == ARM_INSTR_TYPE_DATA_REG_SHIFT ||
	    ip->type == ARM_INSTR_TYPE_DATA_IMM) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int opcode = arm_instr_get_param(instr, ip, 1);
		int s = arm_instr_get_param(instr, ip, 2);

		*flags = 0;
		if ((cond != ARM_COND_NV) && s) {
			*flags = (1 << ARM_FLAG_N) | (1 << ARM_FLAG_Z) |
				(1 << ARM_FLAG_C);

			switch (opcode) {
			case ARM_DATA_OPCODE_ADD:
			case ARM_DATA_OPCODE_ADC:
			case ARM_DATA_OPCODE_CMP:
			case ARM_DATA_OPCODE_CMN:
			case ARM_DATA_OPCODE_RSB:
			case ARM_DATA_OPCODE_RSC:
			case ARM_DATA_OPCODE_SBC:
			case ARM_DATA_OPCODE_SUB:
				*flags |= (1 << ARM_FLAG_V);
				break;
			}
		}
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_IMM_STATUS ||
		   ip->type == ARM_INSTR_TYPE_MOVE_REG_STATUS) {
		int cond = arm_instr_get_param(instr, ip, 0);

		*flags = 0;
		if (cond != ARM_COND_NV) *flags = ARM_FLAG_MASK;
	} else if (ip->type == ARM_INSTR_TYPE_CP_REG) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int load = arm_instr_get_param(instr, ip, 2);
		int rd = arm_instr_get_param(instr, ip, 4);

		*flags = 0;
		if (cond != ARM_COND_NV && load && rd == 15) {
			*flags = ARM_FLAG_MASK;
		}
	} else if (ip->type == ARM_INSTR_TYPE_MUL) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int s = arm_instr_get_param(instr, ip, 2);

		*flags = 0;
		if (cond != ARM_COND_NV && s) {
			*flags = (1 << ARM_FLAG_N) | (1 << ARM_FLAG_Z);
		}
	} else if (ip->type == ARM_INSTR_TYPE_MUL_LONG) {
		int cond = arm_instr_get_param(instr, ip, 0);
		int s = arm_instr_get_param(instr, ip, 3);

		*flags = 0;
		if (cond != ARM_COND_NV && s) {
			*flags = (1 << ARM_FLAG_N) | (1 << ARM_FLAG_Z);
		}
	} else if (ip->type == ARM_INSTR_TYPE_SWI ||
		   ip->type == ARM_INSTR_TYPE_CLZ ||
		   ip->type == ARM_INSTR_TYPE_MOVE_STATUS_REG ||
		   ip->type == ARM_INSTR_TYPE_LS_IMM_OFF ||
		   ip->type == ARM_INSTR_TYPE_LS_REG_OFF ||
		   ip->type == ARM_INSTR_TYPE_BRANCH_LINK ||
		   ip->type == ARM_INSTR_TYPE_BKPT ||
		   ip->type == ARM_INSTR_TYPE_BRANCH_LINK_THUMB ||
		   ip->type == ARM_INSTR_TYPE_BRANCH_XCHG ||
		   ip->type == ARM_INSTR_TYPE_BRANCH_LINK_XCHG ||
		   ip->type == ARM_INSTR_TYPE_CP_DATA ||
		   ip->type == ARM_INSTR_TYPE_CP_LS ||
		   ip->type == ARM_INSTR_TYPE_LS_MULTI ||
		   ip->type == ARM_INSTR_TYPE_SWAP ||
		   ip->type == ARM_INSTR_TYPE_LS_HWORD_REG_OFF ||
		   ip->type == ARM_INSTR_TYPE_LS_HWORD_IMM_OFF ||
		   ip->type == ARM_INSTR_TYPE_LS_TWO_REG_OFF ||
		   ip->type == ARM_INSTR_TYPE_LS_TWO_IMM_OFF ||
		   ip->type == ARM_INSTR_TYPE_L_SIGNED_REG_OFF ||
		   ip->type == ARM_INSTR_TYPE_L_SIGNED_IMM_OFF ||
		   ip->type == ARM_INSTR_TYPE_DSP_ADD_SUB ||
		   ip->type == ARM_INSTR_TYPE_DSP_MUL) {
		*flags = 0;
	}

	return 0;
}

void
arm_reglist_fprint(FILE *f, uint_t reglist)
{
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
}

void
arm_flaglist_fprint(FILE *f, uint_t flaglist)
{
	int comma = 0;
	for (int i = 0; flaglist; i++) {
		if (flaglist & 1) {
			if (comma) fprintf(f, ",");
			fprintf(f, " %s", arm_flag_map[i]);
			comma = 1;
		}

		flaglist >>= 1;
	}
}

void
arm_instr_fprint(FILE *f, arm_instr_t instr, arm_addr_t addr,
		 map<arm_addr_t, char *> *sym_map, image_t *image)
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
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
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
			fprintf(f, ", %s #%s%x", arm_data_shift_map[sh],
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
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
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

		fprintf(f, ", %s r%d", arm_data_shift_map[sh], rs);
	} else if (ip->type == ARM_INSTR_TYPE_DATA_IMM) {
		int cond, opcode, s, rn, rd, rot, imm;
		ret = arm_instr_get_params(instr, ip, 7, &cond, &opcode, &s,
					   &rn, &rd, &rot, &imm);
		if (ret < 0) abort();

		fprintf(f, "%s%s%s\t", data_opcode_map[opcode],
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
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

		uint_t rot_imm =
			(imm >> (rot << 1)) | (imm << (32 - (rot << 1)));

		fprintf(f, ", #%s%x", (rot_imm < 10 ? "" : "0x"), rot_imm);

		if (rn == 15) {
			if (opcode == ARM_DATA_OPCODE_ADD) {
				char *immstr =
					arm_addr_string(addr + 8 + rot_imm,
							sym_map);
				if (immstr == NULL) abort();

				fprintf(f, "\t; %s", immstr);
				delete immstr;
			} else if (opcode == ARM_DATA_OPCODE_SUB) {
				char *immstr =
					arm_addr_string(addr + 8 - rot_imm,
							sym_map);
				if (immstr == NULL) abort();

				fprintf(f, "\t; %s", immstr);
				delete immstr;
			}
		}
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
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			imm);
	} else if (ip->type == ARM_INSTR_TYPE_CLZ) {
		int cond, rd, rm;
		ret = arm_instr_get_params(instr, ip, 3, &cond, &rd, &rm);
		if (ret < 0) abort();

		fprintf(f, "clz%s\t, r%d, r%m",
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			rd, rm);
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_IMM_STATUS) {
		int cond, r, mask, rot, imm;
		ret = arm_instr_get_params(instr, ip, 5, &cond, &r, &mask,
					   &rot, &imm);
		if (ret < 0) abort();

		fprintf(f, "msr%s\t%s_%s%s%s%s",
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			(r ? "SPSR" : "CPSR"), ((mask & 1) ? "c" : ""),
			((mask & 2) ? "x" : ""), ((mask & 4) ? "s" : ""),
			((mask & 8) ? "f" : ""));

		uint_t rot_imm =
			(imm >> (rot << 1)) | (imm << (32 - (rot << 1)));

		fprintf(f, ", #%s%x", (rot_imm < 10 ? "" : "0x"), rot_imm);
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_REG_STATUS) {
		int cond, r, mask, rm;
		ret = arm_instr_get_params(instr, ip, 4, &cond, &r, &mask,
					   &rm);
		if (ret < 0) abort();

		fprintf(f, "msr%s\t%s_%s%s%s%s",
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			(r ? "SPSR" : "CPSR"), ((mask & 1) ? "c" : ""),
			((mask & 2) ? "x" : ""), ((mask & 4) ? "s" : ""),
			((mask & 8) ? "f" : ""));

		fprintf(f, ", r%d", rm);
	} else if (ip->type == ARM_INSTR_TYPE_MOVE_STATUS_REG) {
		int cond, r, rd;
		ret = arm_instr_get_params(instr, ip, 3, &cond, &r, &rd);
		if (ret < 0) abort();

		fprintf(f, "mrs%s\tr%d, %s",
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			rd, (r ? "SPSR" : "CPSR"));
	} else if (ip->type == ARM_INSTR_TYPE_LS_IMM_OFF) {
		int cond, p, u, b, w, load, rn, rd, imm;
		ret = arm_instr_get_params(instr, ip, 9, &cond, &p, &u, &b, &w,
					   &load, &rn, &rd, &imm);
		if (ret < 0) abort();

		fprintf(f, "%sr%s%s%s\tr%d, [r%d",
			(load ? "ld" : "st"),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			(b ? "b" : ""), ((!p && w) ? "t" : ""), rd, rn);

		if (!p) fprintf(f, "]");

		if (imm > 0) {
			fprintf(f, ", #%s%s%x", (u ? "" : "-"),
				(imm < 10 ? "" : "0x"), imm);
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));

		if (rn == 15) {
			int off = imm * (u ? 1 : -1);
			char *targetstr = arm_addr_string(addr + 8 + off,
							  sym_map);
			if (targetstr == NULL) abort();
			
			fprintf(f, "\t; %s", targetstr);
			delete targetstr;

			if (load && b) {
				uint8_t value;
				int r = image_read_byte(image, addr + 8 + off,
							&value);
				if (r >= 0) {
					fprintf(f, ": 0x%02x", value);
				}
			} else if (load && !b) {
				uint32_t value;
				int r = image_read_word(image, addr + 8 + off,
							&value);
				if (r >= 0) {
					fprintf(f, ": 0x%08x", value);
				}
			}
		}
	} else if (ip->type == ARM_INSTR_TYPE_LS_REG_OFF) {
		int cond, p, u, b, w, load, rn, rd, sha, sh, rm;
		ret = arm_instr_get_params(instr, ip, 11, &cond, &p, &u, &b,
					   &w, &load, &rn, &rd, &sha, &sh,
					   &rm);
		if (ret < 0) abort();

		fprintf(f, "%sr%s%s%s\tr%d, [r%d",
			(load ? "ld" : "st"),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			(b ? "b" : ""), ((!p && w) ? "t" : ""), rd, rn);

		if (!p) fprintf(f, "]");

		fprintf(f, ", %sr%d", (u ? "" : "-"), rm);

		if (sh == ARM_DATA_SHIFT_LSR ||
		    sh == ARM_DATA_SHIFT_ASR) {
			sha = (sha ? sha : 32);
		}

		if (sha > 0) {
			fprintf(f, ", %s #%s%x", arm_data_shift_map[sh],
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

		unsigned int target = arm_instr_branch_target(offset, addr);

		char *targetstr = arm_addr_string(target, sym_map);
		if (targetstr == NULL) abort();

		fprintf(f, "b%s%s\t%s",
			(link ? "l" : ""),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			targetstr);

		delete targetstr;
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

		unsigned int target = arm_instr_branch_target(offset, addr);

		char *targetstr = arm_addr_string(target, sym_map);
		if (targetstr == NULL) abort();

		fprintf(f, "blx\t%s", targetstr);

		delete targetstr;
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_XCHG) {
		int cond, rm;
		ret = arm_instr_get_params(instr, ip, 2, &cond, &rm);
		if (ret < 0) abort();

		fprintf(f, "bx%s\tr%d",
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""), rm);
	} else if (ip->type == ARM_INSTR_TYPE_BRANCH_LINK_XCHG) {
		int cond, rm;
		ret = arm_instr_get_params(instr, ip, 2, &cond, &rm);
		if (ret < 0) abort();

		fprintf(f, "blx%s\tr%d",
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""), rm);
	} else if (ip->type == ARM_INSTR_TYPE_CP_DATA) {
		int cond, opcode_1, crn, crd, cp_num, opcode_2, crm;
		ret = arm_instr_get_params(instr, ip, 7, &cond, &opcode_1,
					   &crn, &crd, &cp_num, &opcode_2,
					   &crm);
		if (ret < 0) abort();

		fprintf(f, "cdp%s\tp%d, %d, cr%d, cr%d, cr%d, %d",
			(cond != ARM_COND_NV ?
			 (cond != ARM_COND_AL ?
			  arm_cond_map[cond] : "") : "2"),
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
			 (cond != ARM_COND_AL ?
			  arm_cond_map[cond] : "") : "2"), (n ? "l" : ""),
			cp_num, crd, rn);

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
			 (cond != ARM_COND_AL ?
			  arm_cond_map[cond] : "") : "2"), cp_num, opcode_1,
			rd, crn, crm, opcode_2);
	} else if (ip->type == ARM_INSTR_TYPE_LS_MULTI) {
		int cond, p, u, s, w, load, rn, reglist;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &p, &u, &s, &w,
					   &load, &rn, &reglist);
		if (ret < 0) abort();

		fprintf(f, "%sm%s%s%s\tr%d%s, {", (load ? "ld" : "st"),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			(u ? "i" : "d"), (p ? "b" : "a"),
			rn, (w ? "!" : ""));

		arm_reglist_fprint(f, reglist);

		fprintf(f, " }%s", (s ? "^" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_MUL) {
		int cond, a, s, rd, rn, rs, rm;
		ret = arm_instr_get_params(instr, ip, 7, &cond, &a, &s, &rd,
					   &rn, &rs, &rm);
		if (ret < 0) abort();

		fprintf(f, "m%s%s%s\tr%d, r%d, r%d", (a ? "la" : "ul"),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			(s ? "s" : ""), rd, rm, rs);

		if (a) fprintf(f, ", r%d", rn);
	} else if (ip->type == ARM_INSTR_TYPE_MUL_LONG) {
		int cond, u, a, s, rdhi, rdlo, rs, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &u, &a, &s,
					   &rdhi, &rdlo, &rs, &rm);
		if (ret < 0) abort();

		fprintf(f, "%sm%sl%s%s\tr%d, r%d, r%d, r%d",
			(u ? "s" : "u"), (a ? "la" : "ul"),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			(s ? "s" : ""), rdlo, rdhi, rm, rs);
	} else if (ip->type == ARM_INSTR_TYPE_SWAP) {
		int cond, b, rn, rd, rm;
		ret = arm_instr_get_params(instr, ip, 5, &cond, &b, &rn, &rd,
					   &rm);
		if (ret < 0) abort();

		fprintf(f, "swp%s%s\tr%d, r%d, [r%d]",
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			(b ? "b" : ""), rd, rm, rn);
	} else if (ip->type == ARM_INSTR_TYPE_LS_HWORD_REG_OFF) {
		int cond, p, u, w, load, rn, rd, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &p, &u, &w,
					   &load, &rn, &rd, &rm);
		if (ret < 0) abort();

		fprintf(f, "%sr%sh\tr%d, [r%d", (load ? "ld" : "st"),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			rd, rn);

		if (!p) fprintf(f, "]");

		fprintf(f, ", %sr%d", (u ? "" : "-"), rm);

		if (p) fprintf(f, "]%s", (w ? "!" : ""));
	} else if (ip->type == ARM_INSTR_TYPE_LS_HWORD_IMM_OFF) {
		int cond, p, u, w, load, rn, rd, off_hi, off_lo;
		ret = arm_instr_get_params(instr, ip, 9, &cond, &p, &u, &w,
					   &load, &rn, &rd, &off_hi, &off_lo);
		if (ret < 0) abort();

		fprintf(f, "%sr%sh\tr%d, [r%d", (load ? "ld" : "st"),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			rd, rn);

		if (!p) fprintf(f, "]");

		if (off_hi || off_lo) {
			int off = (off_hi << 4) | off_lo;
			fprintf(f, ", #%s%s%x", (u ? "" : "-"),
				(off < 10 ? "" : "0x"), off);
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));

		if (rn == 15) {
			int off = ((off_hi << 4) | off_lo) * (u ? 1 : -1);
			char *targetstr = arm_addr_string(addr + 8 + off,
							  sym_map);
			if (targetstr == NULL) abort();

			fprintf(f, "\t; %s", targetstr);
			delete targetstr;

			if (load) {
				uint16_t value;
				int r = image_read_hword(image, addr + 8 + off,
							 &value);
				if (r >= 0) {
					fprintf(f, ": 0x%04x", value);
				}
			}
		}
	} else if (ip->type == ARM_INSTR_TYPE_LS_TWO_REG_OFF) {
		int cond, p, u, w, rn, rd, store, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &p, &u, &w,
					   &rn, &rd, &store, &rm);
		if (ret < 0) abort();

		fprintf(f, "%sr%sd\tr%d, [r%d", (store ? "st" : "ld"),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
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
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			rd, rn);

		if (!p) fprintf(f, "]");

		if (off_hi || off_lo) {
			int off = (off_hi << 4) | off_lo;
			fprintf(f, ", #%s%s%x", (u ? "" : "-"),
				(off < 10 ? "" : "0x"), off);
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));

		if (rn == 15) {
			int off = ((off_hi << 4) | off_lo) * (u ? 1 : -1);
			char *targetstr = arm_addr_string(addr + 8 + off,
							  sym_map);
			if (targetstr == NULL) abort();

			fprintf(f, "\t; %s", targetstr);
			delete targetstr;
		}
	} else if (ip->type == ARM_INSTR_TYPE_L_SIGNED_REG_OFF) {
		int cond, p, u, w, rn, rd, h, rm;
		ret = arm_instr_get_params(instr, ip, 8, &cond, &p, &u, &w,
					   &rn, &rd, &h, &rm);
		if (ret < 0) abort();

		fprintf(f, "ldr%ss%s\tr%d, [r%d",
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
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
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
			(h ? "h" : "b"), rd, rn);

		if (!p) fprintf(f, "]");

		if (off_hi || off_lo) {
			int off = (off_hi << 4) | off_lo;
			fprintf(f, ", #%s%s%x", (u ? "" : "-"),
				(off < 10 ? "" : "0x"), off);
		}

		if (p) fprintf(f, "]%s", (w ? "!" : ""));

		if (rn == 15) {
			int off = ((off_hi << 4) | off_lo) * (u ? 1 : -1);
			char *targetstr = arm_addr_string(addr + 8 + off,
							  sym_map);
			if (targetstr == NULL) abort();

			fprintf(f, "\t; %s", targetstr);
			delete targetstr;
		}
	} else if (ip->type == ARM_INSTR_TYPE_DSP_ADD_SUB) {
		int cond, op, rn, rd, rm;
		ret = arm_instr_get_params(instr, ip, 5, &cond, &op, &rn, &rd,
					   &rm);
		if (ret < 0) abort();

		fprintf(f, "q%s%s%s\tr%d, r%d, r%d", ((op & 2) ? "d" : ""),
			((op & 1) ? "sub" : "add"),
			(cond != ARM_COND_AL ? arm_cond_map[cond] : ""),
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
				(cond != ARM_COND_AL ?
				 arm_cond_map[cond] : ""), rd, rm, rs, rn);
			break;
		case 1:
			fprintf(f, "s%sw%s%s\tr%d, r%d, r%d",
				(x ? "mul" : "mla"), (y ? "t" : "b"),
				(cond != ARM_COND_AL ?
				 arm_cond_map[cond] : ""));
			if (!x) fprintf(f, ", r%d", rn);
			break;
		case 2:
			fprintf(f, "smlal%s%s%s\tr%d, r%d, r%d, r%d",
				(x ? "t" : "b"), (y ? "t" : "b"),
				(cond != ARM_COND_AL ?
				 arm_cond_map[cond] : ""), rn, rd, rm, rs);
			break;
		case 3:
			fprintf(f, "smul%s%s%s\tr%d, r%d, r%d",
				(x ? "t" : "b"), (y ? "t" : "b"),
				(cond != ARM_COND_AL ?
				 arm_cond_map[cond] : ""), rd, rm, rs);
			break;
		default:
			abort();
		}
	} else {
		fprintf(f, "undefined", instr);
	}

	fprintf(f, "\n");
}
