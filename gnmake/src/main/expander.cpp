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
/// \file expander.cpp
///

#include "gdef.h"
#include "expander.h"
#include "genvironment.h"
#include "gassert.h"
#include "gstr.h"
#include "glog.h"
#include "gassert.h"
#include <stdexcept>

Expander::Expander( bool force_env , bool debug ) :
	m_force_env(force_env) ,
	m_debug(debug?2:0) ,
	m_re( R"(\$([^\(]|\([^\)]*\)))" )
{
}

Expander::Expander( const Config & config ) :
	Expander(config.force_env,config.debug)
{
	m_debug = config.debug ;
}

std::string Expander::expand( std::string from , const Vars & vars ,
	G::StringArray stop_list , bool with_env ) const
{
	if( from.empty() || vars.empty() ) return from ;
	std::string to = expandImp( from , vars , stop_list , with_env , 1 ) ;
	if( ( from != to && m_debug >= 2 ) || m_debug >= 3 )
	{
		G_DEBUG( "Expander::expand: from [" << from << "]" ) ;
		G_DEBUG( "Expander::expand: to [" << to << "]" ) ;
	}
	return to ;
}

std::string Expander::expandImp( std::string s , const Vars & vars ,
	G::StringArray stop_list , bool with_env , int depth ) const
{
	if( s.find('$') == std::string::npos )
		return s ;

	if( depth > 100 )
		throw std::runtime_error( "expansion loop" ) ;

	using Iterator = std::regex_iterator<std::string::const_iterator> ;

	std::string result ;

	Iterator p( s.begin() , s.end() , m_re , {} ) ;
	Iterator end ;
	Iterator final ;
	bool done = false ;
	for( ; p != end ; ++p )
	{
		G_ASSERT( !(std::string((*p)[1])).empty() ) ;

		std::string prefix = (*p).prefix() ;
		std::string varname = (*p)[1] ;

		if( m_debug >= 4 ) { G_DEBUG( "Expander::expandImp: " << std::string(depth*2,' ') << "expanding [" << varname << "]" ) ; }

		// sanity checks
		if( varname == "()" )
			throw std::runtime_error( "macro name is missing" ) ;
		if( varname != "$" && varname.find('$') != std::string::npos )
			throw std::runtime_error( "illegal dollar character in macro name [" + varname + "] when expanding [" + s + "]" ) ;

		// strip round brackets: $(FOO) -> FOO
		if( varname.size() > 1U )
		{
			G_ASSERT( varname[0] == '(' && varname.size() > 2U && varname.at(varname.size()-1U) == ')' ) ;
			varname = varname.substr( 1U , varname.size()-2U ) ;
		}

		// parse out substitutions
		std::size_t cpos = varname.find( ':' ) ;
		std::size_t eqpos = varname.find( '=' ) ;
		std::string from ;
		std::string to ;
		if( cpos != std::string::npos &&
			eqpos != std::string::npos &&
			eqpos > cpos )
		{
			from = varname.substr( cpos+1U , eqpos-cpos-1U ) ;
			to = varname.substr( eqpos+1U ) ;
			varname = varname.substr( 0U , cpos ) ; // last
		}
		if( m_debug >= 4 ) { G_DEBUG( "Expander::expandImp: " << std::string(depth*2,' ') << "[" << varname << "] with [" << from << "] -> [" << to << "]" ) ; }

		// apply the stop-list
		if( std::find(stop_list.begin(),stop_list.end(),varname) != stop_list.end() )
		{
			if( m_debug >= 4 ) { G_DEBUG( "Expander::expandImp: " << std::string(depth*2,' ') << "[" << varname << "] in the stop-list" ) ; }
			result.append( prefix ) ;
			result.append( 1U , '$' ) ;
			result.append( (*p)[1] ) ;
		}
		else if( varname == "$" ) // ie. "$$" -> "$"
		{
			result.append( prefix ) ;
			result.append( 1U , '$' ) ;
		}
		else
		{
			// lookup
			std::string value ;
			if( with_env && m_force_env )
			{
				std::string default_ = lookup( varname , vars ) ;
				value = G::Environment::get( varname , default_ ) ;
			}
			else
			{
				if( with_env )
					value = G::Environment::get( varname , {} ) ;
				if( vars.find(varname) != vars.end() )
					value = lookup( varname , vars ) ;
			}
			if( m_debug >= 4 ) { G_DEBUG( "Expander::expandImp: " << std::string(depth*2,' ') << "[" << varname << "] -> [" << value << "]" ) ; }

			// recursion
			value = expandImp( value , vars , stop_list , with_env , depth+1 ) ;
			if( m_debug >= 4 ) { G_DEBUG( "Expander::expandImp: " << std::string(depth*2,' ') << "[" << varname << "] -> [" << value << "]" ) ; }

			// apply substitutions
			if( !from.empty() )
			{
				G::Str::replaceAll( value , from , to ) ;
			}
			if( m_debug >= 4 ) { G_DEBUG( "Expander::expandImp: " << std::string(depth*2,' ') << "[" << varname << "] -> [" << value << "]" ) ; }

			result.append( prefix ) ;
			result.append( value ) ;
			if( m_debug >= 4 ) { G_DEBUG( "Expander::expandImp: " << std::string(depth*2,' ') << "partial result: [" << result << "]" ) ; }
		}
		done = true ;
		final = p ;
	}
	if( done )
	{
		result.append( (*final).suffix() ) ;
		return result ;
	}
	else
	{
		return s ;
	}
}

std::string Expander::lookup( const std::string & s , const Vars & vars )
{
	auto p = vars.find( s ) ;
	if( p == vars.end() )
		return {} ;
	return (*p).second ;
}

G::StringArray Expander::stopList()
{
	// TODO also $$@ and $**
	return {
		"?" , "?F" , "?B" , "?D" ,
		"*" , "*F" , "*B" , "*D" ,
		"<" , "<F" , "<B" , "<D" ,
		"@" , "@F" , "@B" , "@D" } ;
}

