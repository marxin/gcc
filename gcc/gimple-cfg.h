/* Gimple IR definitions related to CFG.

   Copyright (C) 2007-2018 Free Software Foundation, Inc.
   Contributed by Martin Liska <mliska@suse.cz>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef GCC_GIMPLE_CFG_H
#define GCC_GIMPLE_CFG_H

/* Return the edge that belongs to label numbered INDEX
   of a switch statement.  */

static inline edge
gimple_switch_edge (gswitch *gs, unsigned index)
{
  tree label = CASE_LABEL (gimple_switch_label (gs, index));
  return find_edge (gimple_bb (gs), label_to_block (label));
}

/* Return the default edge of a switch statement.  */

static inline edge
gimple_switch_default_edge (gswitch *gs)
{
  tree label = CASE_LABEL (gimple_switch_label (gs, 0));
  return find_edge (gimple_bb (gs), label_to_block (label));
}

#endif  /* GCC_GIMPLE_CFG_H */
