/*
 * args.c - Instruction argument functions
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

#include <assert.h>

#include "args.h"
#include "macros.h"
#include "types.h"


/* Return true if instruction has a cond field. */
static int
da_instr_has_cond(const da_instr_t *instr)
{
	switch (instr->group) {
	case DA_GROUP_UNDEF_1:
	case DA_GROUP_UNDEF_2:
	case DA_GROUP_UNDEF_3:
	case DA_GROUP_UNDEF_4:
	case DA_GROUP_UNDEF_5:
	case DA_GROUP_BLX_IMM:
		return 0;
	default:
		return 1;
	}
}

DA_API da_cond_t
da_instr_get_cond(const da_instr_t *instr)
{
	if (da_instr_has_cond(instr)) return DA_ARG_COND(instr, 28);
	else return DA_COND_AL;
}

/* Return target address for branch instruction offset. */
DA_API da_addr_t
da_instr_branch_target(da_uint_t offset, da_addr_t addr)
{
	/* Sign-extend offset */
	struct { signed int ext : 24; } off;
	off.ext = offset;
	
	return (off.ext << 2) + addr + 8;
}
