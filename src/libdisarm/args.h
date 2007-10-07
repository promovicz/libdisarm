/*
 * args.h - Instruction argument header
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

#ifndef _LIBDISARM_ARGS_H
#define _LIBDISARM_ARGS_H

#include <libdisarm/macros.h>
#include <libdisarm/types.h>

#define DA_ARG(instr,shift,mask)  (((instr)->data >> (shift)) & (mask))
#define DA_ARG_BOOL(instr,shift)  DA_ARG(instr,shift,1)
#define DA_ARG_COND(instr,shift)  DA_ARG(instr,shift,DA_COND_MASK)
#define DA_ARG_SHIFT(instr,shift)  DA_ARG(instr,shift,DA_SHIFT_MASK)
#define DA_ARG_DATA_OP(instr,shift)  DA_ARG(instr,shift,DA_DATA_OP_MASK)
#define DA_ARG_REG(instr,shift)  DA_ARG(instr,shift,DA_REG_MASK)
#define DA_ARG_CPREG(instr,shift)  DA_ARG(instr,shift,DA_CPREG_MASK)

DA_BEGIN_DECLS

da_cond_t da_instr_get_cond(const da_instr_t *instr);
da_addr_t da_instr_branch_target(da_uint_t off, da_addr_t addr);

DA_END_DECLS

#endif /* ! _LIBDISARM_ARGS_H */
