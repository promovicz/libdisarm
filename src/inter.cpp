/* inter.cpp */

#include <cstdio>
#include <cstdlib>

#include <list>

#include "arm.hh"
#include "inter.hh"
#include "types.hh"

using namespace std;


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
	uint_t a;
	uint_t b;
	uint_t c;
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
	uint_t a;
	uint_t b;
} inter_ls_op_t;


typedef struct {
	uint_t addr;
	inter_op_type_t type;
	union {
		inter_data_op_t data;
		inter_ls_op_t ls;
	} op;
} inter_instr_t;


struct _inter_ctx {
	list<inter_instr_t *> *instr_list;
	uint_t next_var;
};



typedef enum {
	INTER_ARM_FLAG_NEG = 0, INTER_ARM_FLAG_ZERO,
	INTER_ARM_FLAG_CARRY,   INTER_ARM_FLAG_OVERFLOW,
	INTER_ARM_FLAG_MAX
} inter_arm_flag_t;


inter_ctx_t *
inter_new_ctx()
{
	inter_ctx_t *ctx = new inter_ctx_t;
	if (ctx == NULL) return NULL;

	ctx->instr_list = new list<inter_instr_t *>();
	ctx->next_var = 0;

	return ctx;
}

void
inter_clear_ctx(inter_ctx_t *ctx)
{
	while (!ctx->instr_list->empty()) {
		inter_instr_t *instr = ctx->instr_list->front();
		ctx->instr_list->pop_front();
		delete instr;
	}
}

static uint_t
inter_alloc_var(inter_ctx_t *ctx)
{
	uint_t new_var = ctx->next_var;
	ctx->next_var += 1;

	return new_var;
}

static void
inter_append_data_instr(inter_ctx_t *ctx, inter_data_op_type_t op,
			uint_t a, uint_t b, uint_t c, arm_addr_t addr)
{
	inter_instr_t *instr = new inter_instr_t;
	if (instr == NULL) abort();

	instr->addr = addr;
	instr->type = INTER_OP_DATA;
	instr->op.data.op = op;
	instr->op.data.a = a;
	instr->op.data.b = b;
	instr->op.data.c = c;

	ctx->instr_list->push_back(instr);
}

static void
inter_append_ls_instr(inter_ctx_t *ctx, inter_ls_op_type_t op,
		      inter_ls_type_t type, uint_t a, uint_t b,
		      arm_addr_t addr)
{
	inter_instr_t *instr = new inter_instr_t;
	if (instr == NULL) abort();

	instr->addr = addr;
	instr->type = INTER_OP_LS;
	instr->op.ls.op = op;
	instr->op.ls.type = type;
	instr->op.ls.a = a;
	instr->op.ls.b = b;

	ctx->instr_list->push_back(instr);
}

static void
inter_append_undef_instr(inter_ctx_t *ctx, arm_addr_t addr)
{
	inter_instr_t *instr = new inter_instr_t;
	if (instr == NULL) abort();

	instr->addr = addr;
	instr->type = INTER_OP_UNDEF;

	ctx->instr_list->push_back(instr);
}


static void
inter_append_instr_neg(inter_ctx_t *ctx, uint_t a, uint_t b, arm_addr_t addr)
{
	/* a = 0 - b */
	int tmp;
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp, 0, addr);
	inter_append_data_instr(ctx, INTER_DATA_OP_SUB, a, tmp, b, addr);
}

static void
inter_append_instr_asr(inter_ctx_t *ctx, uint_t a, uint_t b, uint_t c,
		       arm_addr_t addr)
{
	/* a = (b >> c) | (-(b >> 31) << (32 - c)) */

	/* a := b >> c */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSR, a, b, c, addr);

	/* tmp1 := 31 */
	int tmp1 = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp1, 31, addr);

	/* tmp1 := b >> 31 */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSR, tmp1, b, tmp1,
				addr);

	/* tmp1 := -tmp1 */
	inter_append_instr_neg(ctx, tmp1, tmp1, addr);

	/* tmp2 := 32 */
	int tmp2 = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp2, 32, addr);

	/* tmp2 := tmp2 - c */
	inter_append_data_instr(ctx, INTER_DATA_OP_SUB, tmp2, tmp2, c, addr);

	/* tmp1 := tmp1 << tmp2 */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSL, tmp1, tmp1, tmp2,
				addr);

	/* a := a | tmp1 */
	inter_append_data_instr(ctx, INTER_DATA_OP_OR, a, a, tmp1, addr);
}

static void
inter_append_instr_ror(inter_ctx_t *ctx, uint_t a, uint_t b, uint_t c,
		       arm_addr_t addr)
{
	/* a = (b >> c) | (b << (32 - c)) */

	/* a := b >> c */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSR, a, b, c, addr);

	/* tmp := 32 */
	int tmp = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp, 32, addr);

	/* tmp := tmp - c */
	inter_append_data_instr(ctx, INTER_DATA_OP_SUB, tmp, tmp, c, addr);

	/* tmp := b << tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSL, tmp, b, tmp, addr);

	/* a := a | tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_OR, a, a, tmp, addr);
}

static void
inter_append_instr_rrx(inter_ctx_t *ctx, uint_t a, uint_t b, arm_addr_t addr)
{
	/* a = (carry << 31) | (b >> 1) */

	/* a := carry */
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_FLAG,
			      a, INTER_ARM_FLAG_CARRY, addr);

	/* tmp := 31 */
	int tmp = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp, 31, addr);

	/* a := a << tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSL, a, a, tmp, addr);

	/* tmp := 1 */
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
			      tmp, 1, addr);

	/* tmp := b >> tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_LSR, tmp, b, tmp, addr);

	/* a := a | tmp */
	inter_append_data_instr(ctx, INTER_DATA_OP_OR, a, a, tmp, addr);
}

static void
inter_arm_append_data_imm_shift(inter_ctx_t *ctx, arm_instr_t instr,
				const arm_instr_pattern_t *ip, arm_addr_t addr)
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
			      rm_var, rm, addr);

	if (sha > 0) {
		int sha_var = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
				      sha_var, sha, addr);

		switch (sh) {
		case ARM_DATA_SHIFT_LSL:
			inter_append_data_instr(ctx, INTER_DATA_OP_LSL,
						rm_var, rm_var, sha_var,
						addr);
			break;
		case ARM_DATA_SHIFT_LSR:
			inter_append_data_instr(ctx, INTER_DATA_OP_LSR,
						rm_var, rm_var, sha_var,
						addr);
			break;
		case ARM_DATA_SHIFT_ASR:
			inter_append_instr_asr(ctx, rm_var, rm_var, sha_var,
					       addr);
			break;
		case ARM_DATA_SHIFT_ROR:
			inter_append_instr_ror(ctx, rm_var, rm_var, sha_var,
					       addr);
			break;
		}
	} else if (sh == ARM_DATA_SHIFT_ROR) {
		inter_append_instr_rrx(ctx, rm_var, rm_var, addr);
	}

	int rn_var = inter_alloc_var(ctx);
	inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_REG,
			      rn_var, rn, addr);

	int rd_var = inter_alloc_var(ctx);

	int carry, tmp;

	switch (opcode) {
	case ARM_DATA_OPCODE_AND:
	case ARM_DATA_OPCODE_TST:
		inter_append_data_instr(ctx, INTER_DATA_OP_AND,
					rd_var, rn_var, rm_var, addr);
		break;
	case ARM_DATA_OPCODE_EOR:
		inter_append_data_instr(ctx, INTER_DATA_OP_EOR,
					rd_var, rn_var, rm_var, addr);
		break;
	case ARM_DATA_OPCODE_SUB:
	case ARM_DATA_OPCODE_CMP:
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					rd_var, rn_var, rm_var, addr);
		break;
	case ARM_DATA_OPCODE_RSB:
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					rd_var, rm_var, rn_var, addr);
		break;
	case ARM_DATA_OPCODE_ADD:
	case ARM_DATA_OPCODE_CMN:
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rn_var, rm_var, addr);
		break;
	case ARM_DATA_OPCODE_ADC:
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rn_var, rm_var, addr);
		carry = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_FLAG,
				      carry, INTER_ARM_FLAG_CARRY, addr);
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rd_var, carry, addr);
		break;
	case ARM_DATA_OPCODE_SBC:
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					rd_var, rn_var, rm_var, addr);
		carry = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_FLAG,
				      carry, INTER_ARM_FLAG_CARRY, addr);
		tmp = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
				      tmp, 1, addr);
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					carry, carry, tmp, addr);
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rd_var, carry, addr);
		break;
	case ARM_DATA_OPCODE_RSC:
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					rd_var, rm_var, rn_var, addr);
		carry = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_FLAG,
				      carry, INTER_ARM_FLAG_CARRY, addr);
		tmp = inter_alloc_var(ctx);
		inter_append_ls_instr(ctx, INTER_LS_OP_LOAD, INTER_LS_IMM,
				      tmp, 1, addr);
		inter_append_data_instr(ctx, INTER_DATA_OP_SUB,
					carry, carry, tmp, addr);
		inter_append_data_instr(ctx, INTER_DATA_OP_ADD,
					rd_var, rd_var, carry, addr);
		break;
	case ARM_DATA_OPCODE_ORR:
	case ARM_DATA_OPCODE_TEQ:
		inter_append_data_instr(ctx, INTER_DATA_OP_OR,
					rd_var, rn_var, rm_var, addr);
		break;
	case ARM_DATA_OPCODE_MOV:
		inter_append_data_instr(ctx, INTER_DATA_OP_MOV,
					rd_var, rm_var, 0, addr);
		break;
	case ARM_DATA_OPCODE_BIC:
		inter_append_data_instr(ctx, INTER_DATA_OP_NOT,
					rm_var, rm_var, 0, addr);
		inter_append_data_instr(ctx, INTER_DATA_OP_AND,
					rd_var, rn_var, rm_var, addr);
		break;
	case ARM_DATA_OPCODE_MVN:
		inter_append_data_instr(ctx, INTER_DATA_OP_NOT,
					rm_var, rm_var, 0, addr);
		inter_append_data_instr(ctx, INTER_DATA_OP_MOV,
					rd_var, rm_var, 0, addr);
		break;
	}

	/* set flags ... */

	if (opcode < ARM_DATA_OPCODE_TST || opcode > ARM_DATA_OPCODE_CMN) {
		inter_append_ls_instr(ctx, INTER_LS_OP_STORE, INTER_LS_REG,
				      rd_var, rd, addr);
	}
}

void
inter_arm_append(inter_ctx_t *ctx, arm_instr_t instr, arm_addr_t addr)
{
	int ret;

	const arm_instr_pattern_t *ip = arm_instr_get_instr_pattern(instr);

	if (ip == NULL) {
		inter_append_undef_instr(ctx, addr);
		return;
	}

	if (ip->type == ARM_INSTR_TYPE_DATA_IMM_SHIFT) {
		inter_arm_append_data_imm_shift(ctx, instr, ip, addr);
	} else {
		inter_append_undef_instr(ctx, addr);
	}
}

void
inter_fprint(FILE *f, inter_ctx_t *ctx)
{
	list<inter_instr_t *>::iterator instr_iter;
	for (instr_iter = ctx->instr_list->begin();
	     instr_iter != ctx->instr_list->end(); instr_iter++) {
		inter_instr_t *instr = *instr_iter;
		fprintf(f, "%08x\t", instr->addr);

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
