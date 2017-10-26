/* Terminal color manipulation macros.
   Copyright (C) 2017 Free Software Foundation, Inc.

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

#ifndef GCC_COLOR_MACROS_H
#define GCC_COLOR_MACROS_H

#define COLOR_SEPARATOR		";"
#define COLOR_NONE		"00"
#define COLOR_BOLD		"01"
#define COLOR_UNDERSCORE	"04"
#define COLOR_BLINK		"05"
#define COLOR_REVERSE		"07"
#define COLOR_FG_BLACK		"30"
#define COLOR_FG_RED		"31"
#define COLOR_FG_GREEN		"32"
#define COLOR_FG_YELLOW		"33"
#define COLOR_FG_BLUE		"34"
#define COLOR_FG_MAGENTA	"35"
#define COLOR_FG_CYAN		"36"
#define COLOR_FG_WHITE		"37"
#define COLOR_BG_BLACK		"40"
#define COLOR_BG_RED		"41"
#define COLOR_BG_GREEN		"42"
#define COLOR_BG_YELLOW		"43"
#define COLOR_BG_BLUE		"44"
#define COLOR_BG_MAGENTA	"45"
#define COLOR_BG_CYAN		"46"
#define COLOR_BG_WHITE		"47"
#define SGR_START		"\33["
#define SGR_END			"m\33[K"
#define SGR_SEQ(str)		SGR_START str SGR_END
#define SGR_RESET		SGR_SEQ("")

#endif  /* GCC_COLOR_MACROS_H */
