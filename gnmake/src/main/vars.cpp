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
/// \file vars.cpp
///

#include "gdef.h"
#include "vars.h"
#include "gstr.h"
#include "glog.h"
#include "genvironment.h"
#include <algorithm>

void Vars::insert( const std::string & key , const std::string & value )
{
	insert( std::make_pair( key , value ) ) ;
}

std::pair<Vars::List::iterator,bool> Vars::insert( const List::value_type & pair )
{
	List::iterator p = std::find_if( m_list.begin() , m_list.end() ,
		[=](const List::value_type & pair_){ return pair_.first == pair.first ; } ) ;

	G_DEBUG( "Vars::insert: [" << pair.first << "] = [" << pair.second << "]" ) ;

	if( p == m_list.end() )
	{
		onUpdate( pair.first , pair.second ) ;
		m_list.push_back( pair ) ;
		return std::make_pair( m_list.begin() + (m_list.size()-1U) , true ) ;
	}
	else
	{
		(*p).second = pair.second ;
		return std::make_pair( p , false ) ;
	}
}

std::string Vars::get( const std::string & key , const std::string & default_ ) const
{
	auto p = find( key ) ;
	return p == end() ? default_ : (*p).second ;
}

void Vars::onUpdate( const std::string & key , const std::string & new_value )
{
	std::string ekey = G::Str::upper( key ) ;
	std::string null( 1U , '\001' ) ;
	std::string evalue = G::Environment::get( ekey , null ) ;
	if( evalue != null && evalue != new_value )
		G::Environment::put( ekey , new_value ) ;
}

Vars::List::const_iterator Vars::find( const std::string & key ) const
{
	return std::find_if( begin() , end() ,
		[=](const List::value_type & pair_){ return pair_.first == key ; } ) ;
}


Vars::List::const_iterator Vars::begin() const
{
	return m_list.begin() ;
}

Vars::List::const_iterator Vars::end() const
{
	return m_list.end() ;
}

std::size_t Vars::size() const
{
	return m_list.size() ;
}

bool Vars::empty() const
{
	return m_list.empty() ;
}

std::size_t Vars::count( const std::string & key ) const
{
	return find(key) == end() ? 0U : 1U ;
}

void Vars::reserve( std::size_t n )
{
	m_list.reserve( n ) ;
}

void Vars::setDefault( const std::string & key , const std::string & value )
{
	if( find(key) == end() )
		insert( key , value ) ;
}

