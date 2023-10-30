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
/// \file gtempfile.cpp
///

#include "gdef.h"
#include "gtempfile.h"
#include "gprocess.h"
#include "gpath.h"
#include <iterator>
#include <algorithm>
#include <cstdlib> // mkstemps()
#include <cstdio> // _tempnam()
#include <vector>

bool G::TempFile::m_keep_all = false ;

G::TempFile::TempFile( const std::string & prefix , const std::string & suffix )
{
	std::pair<std::string,FILE*> pair = openImp( prefix , suffix ) ;
	m_path = pair.first ;
	m_fp = pair.second ;
}

G::TempFile::~TempFile()
{
	close() ;
	delete_() ;
}

int G::TempFile::fd() const
{
	return fdImp( m_fp ) ;
}

void G::TempFile::add( const std::string & str )
{
	std::fputs( str.c_str() , m_fp ) ;
	if( ferror(m_fp) )
		throw Error(m_path,"fputs") ;
}

void G::TempFile::write( const char * p , std::size_t n )
{
	std::size_t rc = std::fwrite( p , n , 1U , m_fp ) ;
	if( rc != 1U || ferror(m_fp) )
		throw Error(m_path,"fwrite") ;
}

void G::TempFile::rewind()
{
	std::rewind( m_fp ) ;
}

void G::TempFile::close()
{
	if( m_fp != nullptr )
	{
		FILE * fp = m_fp ;
		m_fp = nullptr ;
		if( std::fclose( fp ) != 0 )
			throw Error(m_path,"fclose") ;
	}
}

void G::TempFile::keep()
{
	m_keep = true ;
}

void G::TempFile::keep_all()
{
	m_keep_all = true ;
}

void G::TempFile::delete_()
{
	if( !m_keep && !m_keep_all )
		std::remove( m_path.c_str() ) ;
}

G::Path G::TempFile::path() const
{
	return m_path ;
}

