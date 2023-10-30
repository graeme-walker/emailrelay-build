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
/// \file vars.h
///

#ifndef G_NMAKE_VARS_H
#define G_NMAKE_VARS_H

#include "gdef.h"
#include "gstringarray.h"
#include <vector>
#include <utility>

struct Vars /// a map
{
	using List = std::vector<std::pair<std::string,std::string>> ;

	std::pair<List::iterator,bool> insert( const List::value_type & ) ;
	void insert( const std::string & , const std::string & ) ;
	void setDefault( const std::string & key , const std::string & value ) ;

	List::const_iterator find( const std::string & ) const ;
	List::const_iterator begin() const ;
	List::const_iterator end() const ;
	List::const_iterator cbegin() const ;
	List::const_iterator cend() const ;

	std::string get( const std::string & key , const std::string & default_ = {} ) const ;

	bool empty() const ;
	std::size_t size() const ;
	std::size_t count( const std::string & ) const ;
	void reserve( std::size_t ) ;

private:
	void onUpdate( const std::string & , const std::string & ) ;

private:
	List m_list ;
} ;

#endif
