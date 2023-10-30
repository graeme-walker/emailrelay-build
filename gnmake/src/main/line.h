//
// Copyright (c) 2023 Graeme Walker <graeme_walker@users.sf.net>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ===
///
/// \file line.h
///

#ifndef G_NMAKE_LINE_H
#define G_NMAKE_LINE_H

#include "gdef.h"
#include <string>

struct Line
{
	Line( const std::string & s_ , unsigned int ln_ ) : ln(ln_) , s(s_) {}
	unsigned ln ;
	std::string s ;
	std::string e ;
	bool ignore {false} ;
} ;

#endif
