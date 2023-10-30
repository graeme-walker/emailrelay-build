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
/// \file config.cpp
///

#include "gdef.h"
#include "config.h"
#include <iostream>

std::ostream & operator<<( std::ostream & s , const Config & c )
{
	return s
		<< (c.dump?"dump ":"")
		<< (c.quiet?"quiet ":"")
		<< (c.nologo?"nologo ":"")
		<< (c.force_env?"force_env ":"")
		<< (c.verbose?"verbose":"")
		<< (c.debug?"debug":"")
		<< (c.ignore?"ignore ":"")
		<< (c.dry_run?"dry_run ":"")
		<< (c.all?"all ":"")
		<< (c.silent?"silent ":"")
		<< (c.keep?"keep ":"") ;
}

