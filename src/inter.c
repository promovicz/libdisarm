/* inter.c */

#include <stdio.h>
#include <stdlib.h>

#include "arm.h"
#include "inter.h"
#include "list.h"


typedef enum {
	INTER_OP_DATA = 0, INTER_OP_LS,
	INTER_OP_UNDEF,
	INTER_OP_MAX
} inter_op_type_t;


typedef enum {
	INTER_DATA_OP_AND = 0, INTER_DATA_OP_EOR,
	INTER_DATA_OP_SUB,     INTER_DATA_OP_ADD,
	INTER_DATA_OP_OR,      INTER_DATA_OP_NOT,
	INTER_DATA_OP_LSR,     INTER_DATA_OP_LSL,
	INTER_DATA_OP_MOV,
	INTER_DATA_OP_MAX
} inter_data_op_type_t;

typedef struct {
	inter_data_op_type_t op;
	unsigned int a;
	unsigned int b;
	unsigned int c;
} inter_data_op_t;


typedef enum {
	INTER_LS_OP_LOAD = 0, INTER_LS_OP_STORE,
	INTER_LS_OP_MAX
} inter_ls_op_type_t;

typedef enum {
	INTER_LS_REG = 0, INTER_LS_MEM,
	INTER_LS_IMM,     INTER_LS_FLAG,
	INTER_LS_MAX
} inter_ls_type_t;

typedef struct {
	inter_ls_op_type_t op;
	inter_ls_type_t type;
	unsigned int a;
	unsigned int b;
} inter_ls_op_t;


typedef struct {
	list_elm_t elm;
	unsigned int address;
	inter_op_type_t type;
	union {
		inter_data_op_t data;
		inter_ls_op_t ls;
	} op;
} inter_instr_t;


struct _inter_context {
	list_t instr_list;
	unsigned int next_var;
};



typedef enum {
	INTER_ARM_FLAG_NEG = 0, INTER_ARM_FLAG_ZERO,
	INTER_ARM_FLAG_CARRY,   INTER_ARM_FLAG_OVERFLOW,
	INTER_ARM_FLAG_MAX
} inter_arm_flag_t;


inter_context_t *
inter_new_context()
{
	inter_context_t *ctx = malloc(sizeof(inter_context_t));
	if (ctx == NULL) return NULL;

	list_init(&ctx->instr_list);
	ctx->next_var = 0;

	return ctx;
}

void
inter_clear_context(inter_context_t *ctx)
{
	inter_instr_t *instr;
	while (list_head(&ctx->instr_list) != NULL) {
		instr = (inter_instr_t *) list_remove_head(&ctx->instr_list);
		free(instr);
	}
}

static unsigned int
inter_alloc_var(inter_context_t *ctx)
{
	unsigned int new_var = ctx->next_var;
	ctx->next_var += 1;

	return new_var;
}

static void
inter_append_data_instr(inter_context_t *ctx, inter_data_op_type_t op,
			unsigned int a, unsigned int b, unsigned int c,
			unsigned int address)
{
	inter_instr_t *instr = malloc(sizeof(inter_instr_t));
	if (instr == NULL) abort();

	instr->address = address;
	instr->type = INTER_OP_DATA;
	instr->op.data.op = op;
	instr->op.data.a = a;
	instr->op.data.b = b;
	instr->op.data.c = c;

	list_append(&ctx->instr_list, (list_elm_t *)instr);
}

static void
inter_append_ls_instr(inter_context_t *ctx, inter_ls_op_type_t op,
		      inter_ls_type_t type, unsigned int a, unsigned int b,
		      unsigned int address)
{
	inter_instr_t *instr = malloc(sizeof(inter_instr_t));
	if (instr == NULL) abort();

	instr->address = address;
	instr->type = INTER_OP_LS;
	instr->op.ls.op = op;
	instr->op.ls.type = type;
	instr->op.ls.a = a;
	instr->op.ls.b = b;

	list_append(&ctx->instr_list, (list_elm_t *)instr);
}

static void
inter_append_undef_instr(inter_context_t *ctx, unsigned int address)
{
	inter_instr_t *instr = malloc(sizeof(inter_instr_t));
	if (instr == NULL) abort();

	instr->address = address;
	instr->type = INTER_OP_UNDEF;

	list_append(&ctx->instr_list, (list_elm_t *)instr);
}


static void
inter_append_instr_neg(inter_context_t *ctx, unsigned int a, unsigned int b,
		       unsigned int address)
{
	/* a = 0 - b */
	int tmp;
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp, 0, address);
	inter_append_data_instr(ctx, INTER_DATA_OP_SUB, a, tmp, b, address);
}

static void
inter_append_instr_asr(inter_context_t *ctx, unsigned int a, unsigned int b,
		       unsigned int c, unsigned int address)
{
	/* a = (b >> c) | (-(b >> 31) << (32 - c)) */

	/* a := b >> c */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSR, a, b, c, address);

	/* tmp1 := 31 */
	int tmp1 = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp1, 31, address);

	/* tmp1 := b >> 31 */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSR, tmp1, b, tmp1,
				address);

	/* tmp1 := -tmp1 */
	inter_append_instr_neg(ctx, tmp1, tmp1, address);

	/* tmp2 := 32 */
	int tmp2 = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp2, 32, address);

	/* tmp2 := tmp2 - c */
	inter_append_data_instr(ctx, INTER_DATA_OP_SUB, tmp2, tmp2, c,
				address);

	/* tmp1 := tmp1 << tmp2 */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSL, tmp1, tmp1, tmp2,
				address);

	/* a := a | tmp1 */
	inter_append_data_instr(ctx, INTER_DATA_OP_OR, a, a, tmp1, address);
}

static void
inter_append_instr_ror(inter_context_t *ctx, unsigned int a, unsigned int b,
		       unsigned int c, unsigned int address)
{
	/* a = (b >> c) | (b << (32 - c)) */

	/* a := b >> c */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSR, a, b, c, address);

	/* tmp := 32 */
	int tmp = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp, 32, address);

	/* tmp := tmp - c */
	inter_append_data_instr(ctx, INTER_DATA_OP_SUB, tmp, tmp, c, address);

	/* tmp := b << tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSL, tmp, b, tmp, address);

	/* a := a | tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_OR, a, a, tmp, address);
}

static void
inter_append_instr_rrx(inter_context_t *ctx, unsigned int a, unsigned int b,
		       unsigned int address)
{
	/* a = (carry << 31) | (b >> 1) */

	/* a := carry */
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_FLAG,
			      a, INTER_ARM_FLAG_CARRY, address);

	/* tmp := 31 */
	int tmp = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp, 31, address);

	/* a := a << tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSL, a, a, tmp, address);

	/* tmp := 1 */
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp, 1, address);

	/* tmp := b >> tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSR, tmp, b, tmp, address);

	/* a := a | tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_OR, a, a, tmp, address);
}

static void
inter_arm_append_data_imm_shift(inter_context_t *ctx, arm_instr_t instr,
				const arm_instr_pattern_t *ip,
				unsigned int address)
{
	int ret;

	int cond, opcode, s, rn, rd, sha, sh, rm;
	ret = arm_instr_get_params(instr, ip, 8, &cond, &opcode, &s, &rn, &rd,
				   &sha, &sh, &rm);
	if (ret < 0) abort();


	if (sh == ARM_DATA_SHIFT_LSR || sh == ARM_DATA_SHIFT_ASR) {
		sha = (sha ? sha : 32);
	}

	int rm_var = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_REG,
			      rm_var, rm, address);

	if (sha > 0) {
		int sha_var = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
				      sha_var, sha, address);

		switch (sh) {
		case ARM_DATA_SHIFT_LSL:
			inter_append_data_instr(ctx, INTER_DATA_OP_LSL,
						rm_var, rm_var, sha_var,
						address);
			break;
		case ARM_DATA_SHIFT_LSR:
			inter_append_data_instr(ctx, INTER_DATA_OP_LSR,
						rm_var, rm_var, sha_var,
						address);
			break;
		case ARM_DATA_SHIFT_ASR:
			inter_append_instr_asr(ctx, rm_var, rm_var, sha_var,
					       address);
			break;
		case ARM_DATA_SHIFT_ROR:
			inter_append_instr_ror(ctx, rm_var, rm_var, sha_var,
					       address);
			break;
		}
	} else if (sh == ARM_DATA_SHIFT_ROR) {
		inter_append_instr_rrx(ctx, rm_var, rm_var, address);
	}

	int rn_var = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_REG,
			      rn_var, rn, address);

	int rd_var = inter_alloc_var(ctx);

	int carry, tmp;

	switch (opcode) {
	case ARM_DATA_OPCODE_AND:
	case ARM_DATA_OPCODE_TST:
		inter_append_data_instr(ctx, INTER_DATA_OP_AND,
					rd_var, rn_var, rm_var, address);
		break;
	case ARM_DATA_OPCODE_EOR:
		inter_append_data_instr(ctx, INTER_DATA_OP_EOR,
					rd_var, rn_var, rm_var, address);
		break;
	case ARM_DATA_OPCODE_SUB:
	case ARM_DATA_OPCODE_CMP:
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					rd_var, rn_var, rm_var, address);
		break;
	case ARM_DATA_OPCODE_RSB:
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					rd_var, rm_var, rn_var, address);
		break;
	case ARM_DATA_OPCODE_ADD:
	case ARM_DATA_OPCODE_CMN:
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rn_var, rm_var, address);
		break;
	case ARM_DATA_OPCODE_ADC:
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rn_var, rm_var, address);
		carry = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_FLAG,
				      carry, INTER_ARM_FLAG_CARRY, address);
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rd_var, carry, address);
		break;
	case ARM_DATA_OPCODE_SBC:
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					rd_var, rn_var, rm_var, address);
		carry = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_FLAG,
				      carry, INTER_ARM_FLAG_CARRY, address);
		tmp = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
				      tmp, 1, address);
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					carry, carry, tmp, address);
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rd_var, carry, address);
		break;
	case ARM_DATA_OPCODE_RSC:
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					rd_var, rm_var, rn_var, address);
		int carry = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_FLAG,
				      carry, INTER_ARM_FLAG_CARRY, address);
		int tmp = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
				      tmp, 1, address);
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					carry, carry, tmp, address);
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rd_var, carry, address);
		break;
	case ARM_DATA_OPCODE_ORR:
	case ARM_DATA_OPCODE_TEQ:
		inter_append_data_instr(ctx, INTER_DATA_OP_OR,
					rd_var, rn_var, rm_var, address);
		break;
	case ARM_DATA_OPCODE_MOV:
		inter_append_data_instr(ctx, INTER_DATA_OP_MOV,
					rd_var, rm_var, 0, address);
		break;
	case ARM_DATA_OPCODE_BIC:
		inter_append_data_instr(ctx, INTER_DATA_OP_NOT,
					rm_var, rm_var, 0, address);
		inter_append_data_instr(ctx, INTER_DATA_OP_AND,
					rd_var, rn_var, rm_var, address);
		break;
	case ARM_DATA_OPCODE_MVN:
		inter_append_data_instr(ctx, INTER_DATA_OP_NOT,
					rm_var, rm_var, 0, address);
		inter_append_data_instr(ctx, INTER_DATA_OP_MOV,
					rd_var, rm_var, 0, address);
		break;
	}

	/* set flags ... */

	if (opcode < ARM_DATA_OPCODE_TST || opcode > ARM_DATA_OPCODE_CMN) {
		inter_append_ls_instr(ctx, INTER_LS_OP_STORE, INTER_LS_REG,
				      rd_var, rd, address);
	}
}

void
inter_arm_append(inter_context_t *ctx, arm_instr_t instr, unsigned int address)
{
	int ret;

	const arm_instr_pattern_t *ip = arm_instr_get_instr_pattern(instr);

	if (ip == NULL) {
		inter_append_undef_instr(ctx, address);
		return;
	}

	if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT) {
		inter_arm_append_data_imm_shift(ctx, instr, ip, address);
	} else {
		inter_append_undef_instr(ctx, address);
	}
}

void
inter_fprint(FILE *f, inter_context_t *ctx)
{
	list_elm_t *elm;
	list_foreach(ctx->instr_list, elm) {
		inter_instr_t *instr = (inter_instr_t *)elm;
		fprintf(f, "%08x\t", instr->address);

		static const char *inter_data_op_map[] = {
			"&", "^", "-", "+", "|", NULL, ">>", "<<", NULL
		};

		static const char *inter_arm_flag_map[] = {
			"neg", "zero", "carry", "overflow"
		};

		switch (instr->type) {
		case INTER_OP_DATA:
			if (instr->op.data.op == INTER_DATA_OP_NOT) {
				fprintf(f, "var%d := !var%d\n",
					instr->op.data.a, instr->op.data.b);
			} else if (instr->op.data.op == INTER_DATA_OP_MOV) {
				fprintf(f, "var%d := var%d\n",
					instr->op.data.a, instr->op.data.b);
			} else {
				fprintf(f, "var%d := var%d %s var%d\n",
					instr->op.data.a, instr->op.data.b,
					inter_data_op_map[instr->op.data.op],
					instr->op.data.c);
			}
				
			break;
		case INTER_OP_LS:
			if (instr->op.ls.op == INTER_LS_OP_LOAD) {
				switch (instr->op.ls.type) {
				case INTER_LS_REG:
					fprintf(f, "var%d := r%d\n",
						instr->op.ls.a,
						instr->op.ls.b);
					break;
				case INTER_LS_MEM:
					fprintf(f, "var%d := [%08x]\n",
						instr->op.ls.a,
						instr->op.ls.b);
					break;
				case INTER_LS_IMM:
					fprintf(f, "var%d := 0x%x\n",
						instr->op.ls.a,
						instr->op.ls.b);
					break;
				case INTER_LS_FLAG:
					fprintf(f, "var%d := flag(%s)\n",
						instr->op.ls.a,
						inter_arm_flag_map[
							instr->op.ls.b]);
					break;
				}
			} else {
				switch (instr->op.ls.type) {
				case INTER_LS_REG:
					fprintf(f, "r%d := var%d\n",
						instr->op.ls.b,
						instr->op.ls.a);
					break;
				case INTER_LS_MEM:
					fprintf(f, "[%08x] := var%d\n",
						instr->op.ls.b,
						instr->op.ls.a);
					break;
				case INTER_LS_FLAG:
					fprintf(f, "flag(%s) := var%d\n",
						inter_arm_flag_map[
							instr->op.ls.b],
						instr->op.ls.a);
					break;
				}
			}

			break;
		case INTER_OP_UNDEF:
		default:
			fprintf(f, "undefined\n");
			break;
		}
	}
}
