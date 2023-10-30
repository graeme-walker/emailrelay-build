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
/// \file cwd.h
///

#include "gdef.h"

#ifndef GNMAKE_CWD_H
#define GNMAKE_CWD_H

class Cwd
{
public:
	Cwd() ;
		///< Constructor.

	~Cwd() ;
		///< Restores the current process's working
		///< directory to the same as when constructed.

	bool apply( const std::string & cd_cmd ) ;
		///< Changes the current process's working directory
		///< iff the given command is a simple "cd" command
		///< and not eg. "cd dir && cmd". Returns true
		///< if a simple cd command.

private:
	static int chdir( const std::string & ) ;

private:
	bool changed {false} ;
	std::string old ;
} ;

#endif
