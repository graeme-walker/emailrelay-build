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
/// \file gtempfile_unix.cpp
///

#include "gdef.h"
#include "gtempfile.h"
#include "gstr.h"
#include <iterator>
#include <algorithm>
#include <cstdlib> // mkstemps()
#include <vector>

std::pair<std::string,FILE*> G::TempFile::openImp( const std::string & prefix , const std::string & suffix )
{
	// (mkstemps() needs a writable buffer containing its XXXXXX substitution variable)

	if( prefix.find("XXXXXX") != std::string::npos || suffix.find("XXXXXX") != std::string::npos )
		throw Error( "invalid parameter" ) ;

	std::string pattern = "/tmp/" + G::Str::replaced(prefix,'/','_') + "XXXXXX" + G::Str::replaced(suffix,'/','_') ;
	std::vector<char> buffer( pattern.length()+1U , '\0' ) ;
	std::copy( pattern.begin() , pattern.end() , buffer.begin() ) ;

	int fd = ::mkstemps( &buffer[0] , static_cast<int>(suffix.length()) ) ;
	if( fd < 0 )
		throw Error("mkstemp") ;

	std::string name ;
	std::copy( buffer.begin() , buffer.end() , std::back_inserter(name) ) ;
	name.resize( pattern.length() ) ; // remove trailing nul

	FILE * fp = fdopen( fd , "w" ) ;
	if( fp == nullptr )
		throw Error(name,"fdopen") ;

	return { name , fp } ;
}

int G::TempFile::fdImp( FILE * fp )
{
	return ::fileno( fp ) ; // not std::fileno
}

