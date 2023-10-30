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
/// \file gtempfile.h
///

#ifndef G_TEMPFILE_H
#define G_TEMPFILE_H

#include "gdef.h"
#include "gpath.h"
#include "gexception.h"
#include <string>
#include <sstream>
#include <cstdio>

namespace G
{
	class TempFile ;
}

//| \class G::TempFile
/// A class for creating temporary files, with support for
/// output streaming.
///
/// Eg.
/// \code
/// G::TempFile tmp( "foo" , ".sh" ) ;
/// tmp << "echo testing " << 1 << 2.3 << "\n" ;
/// tmp.close() ;
/// system( tmp.path() ) ;
/// \endcode
///
class G::TempFile
{
public:
	G_EXCEPTION( Error , tx("tempfile error") ) ;

	explicit TempFile( const std::string & prefix = {} , const std::string & suggested_suffix = {} ) ;
		///< Constructor. Creates and opens a temporary file.

	~TempFile() ;
		///< Destructor. Deletes the file unless keep()ed.

	int fd() const ;
		///< Returns the file descriptor for writing.

	void add( const std::string & str ) ;
		///< Adds text to the file.

	void write( const char * , std::size_t ) ;
		///< Adds a block of data to the file. Throws on error.
		///< (Named to match ostream::write()).

	void rewind() ;
		///< Seeks back to the beginning of the file.

	void close() ;
		///< Closes the file.

	void keep() ;
		///< Prevents deletion on destruction.

	G::Path path() const ;
		///< Returns the full path.

	static void keep_all() ;
		///< Prevents all deletion on destruction.
		///< Used for debugging.

public:
	TempFile( const TempFile & ) = delete ;
	TempFile( TempFile && ) = delete ;
	TempFile & operator=( const TempFile & ) = delete ;
	TempFile & operator=( TempFile && ) = delete ;

private:
	void delete_() ;
	static int fdImp( FILE * ) ;
	static std::pair<std::string,FILE*> openImp( const std::string & , const std::string & ) ;

private:
	static bool m_keep_all ;
	bool m_keep{false} ;
	std::string m_path ;
	FILE * m_fp{nullptr} ;
} ;

namespace G
{
	template <typename T>
	inline
	TempFile & operator<<( TempFile & tmp , const T & t )
	{
		std::ostringstream ss ;
		ss << t ;
		tmp.add( ss.str() ) ;
		return tmp ;
	}

	template <>
	inline
	TempFile & operator<< <std::string> ( TempFile & tmp , const std::string & str )
	{
		tmp.add( str ) ;
		return tmp ;
	}
}

#endif
