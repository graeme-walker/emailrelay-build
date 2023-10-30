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
/// \file gtempfile_win32.cpp
///

#include "gdef.h"
#include "gtempfile.h"
#include "gpath.h"
#include <cstdio> // _tempnam()
#include <utility>

std::pair<std::string,FILE*> G::TempFile::openImp( const std::string & prefix , const std::string & )
{
	// (see also tmpfile_s() and tmpnam_s<>())
	char * p = _tempnam( nullptr , prefix.c_str() ) ; // uses %TMP%
	if( p == nullptr )
		throw Error( "_tempnam" ) ;

	std::string name = Path(std::string(p)).str() ;
	std::free( p ) ;

	FILE * fp = _fsopen( name.c_str() , "w" , _SH_DENYNO ) ;
	if( fp == nullptr )
		throw Error( name , "fopen(_tempnam())" ) ;

	return { name , fp } ;
}

int G::TempFile::fdImp( FILE * fp )
{
	return _fileno( fp ) ; // may be a macro
}

