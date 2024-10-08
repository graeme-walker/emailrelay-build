//
// Copyright (C) 2001-2024 Graeme Walker <graeme_walker@users.sourceforge.net>
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
/// \file ghostname_unix.cpp
///

#include "gdef.h"
#include "ghostname.h"
#include "gstr.h"
#include "genvironment.h"
#include <sys/utsname.h>

std::string G::hostname()
{
	struct utsname info {} ;
	int rc = ::uname( &info ) ;
	if( rc == -1 )
		return {} ;

	info.nodename[sizeof(info.nodename)-1U] = '\0' ;
	std::string_view name = G::Str::headView( info.nodename , {".",1U} , false ) ;

	if( name.empty() ) // pathologically "uname -n" can be empty
		return Environment::get( "HOSTNAME" , std::string() ) ;

	return Str::printable( name ) ;
}

