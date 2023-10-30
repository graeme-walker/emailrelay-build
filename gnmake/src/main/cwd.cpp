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
/// \file cwd.cpp
///

#include "gdef.h"
#include "cwd.h"
#include "gprocess.h"
#include "gstr.h"
#include "glog.h"

#ifdef G_WINDOWS
#include <direct.h>
int Cwd::chdir( const std::string & p ) { return _chdir( p.c_str() ) ; }
#else
#include <unistd.h>
int Cwd::chdir( const std::string & p ) { return ::chdir( p.c_str() ) ; }
#endif

Cwd::Cwd() =
default ;

Cwd::~Cwd()
{
	if( changed )
		chdir( old ) ;
}

bool Cwd::apply( const std::string & cd_cmd )
{
	auto pos = cd_cmd.find_first_not_of( " \t@-" ) ;
	if( pos != std::string::npos && (pos+2U) < cd_cmd.size() &&
		( cd_cmd.at(pos) == 'c' || cd_cmd.at(pos) == 'C' ) &&
		( cd_cmd.at(pos+1U) == 'd' || cd_cmd.at(pos+1U) == 'D' ) &&
		cd_cmd.find("&&") == std::string::npos )
	{
		std::string dir = G::Str::trimmed( cd_cmd.substr(pos+2U) , " \t" ) ;
		if( dir.size() >= 2U && dir[0] == '"' && dir[dir.size()-1U] == '"' )
			dir = dir.substr( 1U , dir.size()-2U ) ;
		if( !changed )
			old = G::Process::cwd() ;
		if( chdir( dir ) == 0 )
			changed = true ;
		else
			G_WARNING( "cannot change directory" ) ;
		return true ;
	}
	else
	{
		return false ; // not a simple 'cd' command
	}
}

