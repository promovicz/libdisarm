/* arm.hh */

#ifndef _ARM_HH
#define _ARM_HH

#include <list>
#include <map>

#include "types.hh"
#include "image.hh"

using namespace std;


typedef enum {
	ARM_INSTR_TYPE_NONE = 0,

	/* Data processing immediate shift */
	ARM_INSTR_TYPE_DATA_IMM_SHIFT,
	/* Data processing register shift */
	ARM_INSTR_TYPE_DATA_REG_SHIFT,
	/* Data processing immediate */
	ARM_INSTR_TYPE_DATA_IMM,
	/* Undefined instruction #1 */
	ARM_INSTR_TYPE_UNDEF_1,
	/* Move immediate to status register */
	ARM_INSTR_TYPE_MOVE_IMM_STATUS,
	/* Load/store immediate offset */
	ARM_INSTR_TYPE_LS_IMM_OFF,
	/* Load/store register offset */
	ARM_INSTR_TYPE_LS_REG_OFF,
	/* Undefined instruction #2 */
	ARM_INSTR_TYPE_UNDEF_2,
	/* Undefined instruction #3 */
	ARM_INSTR_TYPE_UNDEF_3,
	/* Load/store multiple */
	ARM_INSTR_TYPE_LS_MULTI,
	/* Undefined instruction #4 */
	ARM_INSTR_TYPE_UNDEF_4,
	/* Branch and branch with link */
	ARM_INSTR_TYPE_BRANCH_LINK,
	/* Branch and branch with link and change to Thumb */
	ARM_INSTR_TYPE_BRANCH_LINK_THUMB,
	/* Coprocessor load/store and double register transfers */
	ARM_INSTR_TYPE_CP_LS,
	/* Coprocessor data processing */
	ARM_INSTR_TYPE_CP_DATA,
	/* Coprocessor register transfers */
	ARM_INSTR_TYPE_CP_REG,
	/* Software interrupt */
	ARM_INSTR_TYPE_SWI,
	/* Undefined instruction #5 */
	ARM_INSTR_TYPE_UNDEF_5,

	/* Multiply (accumulate) */
	ARM_INSTR_TYPE_MUL,
	/* Multiply (accumulate) long */
	ARM_INSTR_TYPE_MUL_LONG,
	/* Swap/swap byte */
	ARM_INSTR_TYPE_SWAP,
	/* Load/store halfword register offset */
	ARM_INSTR_TYPE_LS_HWORD_REG_OFF,
	/* Load/store halfword immediate offset */
	ARM_INSTR_TYPE_LS_HWORD_IMM_OFF,
	/* Load/store two words register offset */
	ARM_INSTR_TYPE_LS_TWO_REG_OFF,
	/* Load signed halfword/byte register offset */
	ARM_INSTR_TYPE_L_SIGNED_REG_OFF,
	/* Load/store two words immediate offset */
	ARM_INSTR_TYPE_LS_TWO_IMM_OFF,
	/* Load signed halfword/byte immediate offset */
	ARM_INSTR_TYPE_L_SIGNED_IMM_OFF,

	/* Move status register to register */
	ARM_INSTR_TYPE_MOVE_STATUS_REG,
	/* Move register to status register */
	ARM_INSTR_TYPE_MOVE_REG_STATUS,
	/* Branch/exchange instruction set */
	ARM_INSTR_TYPE_BRANCH_XCHG,
	/* Count leading zeros */
	ARM_INSTR_TYPE_CLZ,
	/* Branch and link/exchange instruction set */
	ARM_INSTR_TYPE_BRANCH_LINK_XCHG,
	/* Enhanced DSP add/subtracts */
	ARM_INSTR_TYPE_DSP_ADD_SUB,
	/* Software breakpoint */
	ARM_INSTR_TYPE_BKPT,
	/* Enhanced DSP multiplies */
	ARM_INSTR_TYPE_DSP_MUL,

	ARM_INSTR_TYPE_MAX
} arm_instr_type_t;


typedef enum {
	ARM_PARAM_TYPE_NONE = 0,
	ARM_PARAM_TYPE_UINT,
	ARM_PARAM_TYPE_COND,
	ARM_PARAM_TYPE_REG,
	ARM_PARAM_TYPE_CPREG,
	ARM_PARAM_TYPE_REGLIST,
	ARM_PARAM_TYPE_DATA_OP
} arm_param_type_t;

typedef struct {
	arm_instr_t mask;
	int shift;
	arm_param_type_t type;
	const char *name;
} arm_param_pattern_t;

typedef struct {
	arm_instr_t mask;
	arm_instr_t value;
	arm_instr_type_t type;
	const arm_param_pattern_t *param;
} arm_instr_pattern_t;



typedef enum {
	ARM_COND_EQ = 0, ARM_COND_NE,
	ARM_COND_CS,     ARM_COND_CC,
	ARM_COND_MI,     ARM_COND_PL,
	ARM_COND_VS,     ARM_COND_VC,
	ARM_COND_HI,     ARM_COND_LS,
	ARM_COND_GE,     ARM_COND_LT,
	ARM_COND_GT,     ARM_COND_LE,
	ARM_COND_AL,     ARM_COND_NV,
	ARM_COND_MAX
} arm_cond_t;

static const char *cond_map[] = {
	"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
	"hi", "ls", "ge", "lt", "gt", "le", "al", "nv"
};


typedef enum {
	ARM_DATA_SHIFT_LSL = 0, ARM_DATA_SHIFT_LSR,
	ARM_DATA_SHIFT_ASR,     ARM_DATA_SHIFT_ROR,
	ARM_DATA_SHIFT_MAX
} arm_data_shift_t;

static const char *data_shift_map[] = {
	"lsl", "lsr", "asr", "ror"
};


typedef enum {
	ARM_DATA_OPCODE_AND = 0, ARM_DATA_OPCODE_EOR,
	ARM_DATA_OPCODE_SUB,     ARM_DATA_OPCODE_RSB,
	ARM_DATA_OPCODE_ADD,     ARM_DATA_OPCODE_ADC,
	ARM_DATA_OPCODE_SBC,     ARM_DATA_OPCODE_RSC,
	ARM_DATA_OPCODE_TST,     ARM_DATA_OPCODE_TEQ,
	ARM_DATA_OPCODE_CMP,     ARM_DATA_OPCODE_CMN,
	ARM_DATA_OPCODE_ORR,     ARM_DATA_OPCODE_MOV,
	ARM_DATA_OPCODE_BIC,     ARM_DATA_OPCODE_MVN,
	ARM_DATA_OPCODE_MAX
} arm_data_opcode_t;

static const char *data_opcode_map[] = {
	"and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
	"tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn"
};


const arm_instr_pattern_t *arm_instr_get_instr_pattern(arm_instr_t instr);
const arm_param_pattern_t *arm_instr_get_param_pattern(
	const arm_instr_pattern_t *ip, uint_t param);
uint_t arm_instr_get_param(arm_instr_t instr,
				 const arm_instr_pattern_t *ip, uint_t param);
int arm_instr_get_params(arm_instr_t instr, const arm_instr_pattern_t *ip,
			 uint_t params, ...);
int arm_instr_get_cond(arm_instr_t instr, arm_cond_t *cond);
arm_addr_t arm_instr_branch_target(int offset, arm_addr_t address);
int arm_instr_is_reg_used(arm_instr_t instr, uint_t reg);
int arm_instr_is_reg_changed(arm_instr_t instr, uint_t reg);
int arm_instr_used_regs(arm_instr_t instr, uint_t *reglist);
int arm_instr_changed_regs(arm_instr_t instr, uint_t *reglist);

void arm_instr_fprint(FILE *f, arm_instr_t instr, arm_addr_t addr,
		      map<arm_addr_t, char *> *sym_map, image_t *image);
char *arm_addr_string(arm_addr_t addr, map<arm_addr_t, char *> *sym_map);


#endif /* ! _ARM_HH */
