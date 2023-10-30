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
/// \file parser.cpp
///

#include "gdef.h"
#include "parser.h"
#include "gstr.h"
#include "glog.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <stack>
#include <vector>
#include <algorithm>

struct StackItem
{
	bool enabled ;
	bool active ;
	bool done ;
} ;

struct Stack
{
	std::stack<StackItem> m_stack ;
	Stack()
	{
		m_stack.push( { true , true , false } ) ;
	}
	void push( bool value )
	{
		bool was_active = m_stack.top().active ;
		m_stack.push( { was_active , was_active&&value , value } ) ;
	}
	void set( bool value )
	{
		if( m_stack.top().enabled )
		{
			if( value && !m_stack.top().done )
			{
				m_stack.top().active = true ;
				m_stack.top().done = true ;
			}
			else
			{
				m_stack.top().active = false ;
			}
		}
	}
	void flip()
	{
		if( m_stack.top().enabled )
			m_stack.top().active = !( m_stack.top().active || m_stack.top().done ) ;
	}
	void pop()
	{
		m_stack.pop() ;
		if( m_stack.empty() )
			throw std::runtime_error( "!if/!endif nesting error" ) ;
	}
	bool active() const
	{
		return m_stack.top().active ;
	}
} ;

Parser::Parser( const Config & config , Expander & expander , Vars & vars ) :
	m_config(config) ,
	m_expander(expander) ,
	m_vars(vars) ,
	m_re_comment( R"(^\s*#.*)" ) ,
	m_re_assignment( R"(^(\S+)\s*=\s*(.*))" )
{
}

Parser::Lines Parser::parse( const std::string & makefile )
{
	m_makefile = makefile ;
	Parser::Lines lines ;
	parse( lines ) ;
	return lines ;
}

void Parser::parse( Lines & lines )
{
	G_DEBUG( "Parser::parse: parsing [" << m_makefile << "]" ) ;
	std::ifstream f( m_makefile ) ;
	if( !f.good() )
		throw std::runtime_error( "cannot open makefile [" + m_makefile + "]" ) ;

	// read the makefile into 'lines' allowing for comments and line continuations
	{
		std::string full_line ;
		std::string line ;
		unsigned line_number = 1U ;
		for( ; std::getline(f,line) ; line_number++ )
		{
			if( !line.empty() && line.at(line.size()-1U) == '\r' )
				line.erase( line.size()-1U ) ;

			if( m_config.log_makefile )
				G_LOG_S( "Parser::parse: " << line ) ;

			// continuations must be the last character, without
			// trailing whitespace

			// full-line comments are ignored before parsing continuations

			// mid-line comments hide continuation characters except
			// in action lines -- ignore for now

			if( std::regex_match( line , m_re_comment ) )
			{
			}
			else if( line.size() >= 2U && line[line.size()-1U] == '\\' && line[line.size()-2U] == '^' )
			{
				full_line.append( line.substr(0U,line.size()-2U).append(1U,'\\') ) ;
				lines.emplace_back( full_line , line_number ) ;
				full_line.clear() ;
			}
			else if( !line.empty() && line.at(line.size()-1U) == '\\' )
			{
				full_line.append( line.substr(0U,line.size()-1U) ) ;
			}
			else
			{
				full_line.append( line ) ;
				lines.emplace_back( full_line , line_number ) ;
				full_line.clear() ;
			}
		}
		if( !full_line.empty() )
			lines.emplace_back( full_line , line_number ) ;
		if( !f.eof() )
			throw std::runtime_error( "failed to read makefile [" + m_makefile + "]" ) ;
	}

	// deal with directives -- mark unwanted lines with 'ignore'
	Stack stack ;
	for( auto & line_in : lines )
	{
		if( line_in.s.find("!") == 0 )
		{
			std::string line = line_in.s.substr( 1U ) ;
            line.erase( 0U , line.find_first_not_of(" \t") ) ;
			std::string lline = G::Str::lower( line ) ;
			G::Str::replace( lline , '\t' , ' ' ) ;
			if( lline.find("if ") == 0U )
			{
				bool result = evaluate( line_in.ln , line.substr(3U) , m_expander , m_vars ) ;
				G_DEBUG( "Parser::parse: evaluate(" << line.substr(3U) << ") -> " << result ) ;
				stack.push( result ) ;
			}
			else if( lline.find("elseif ") == 0U )
			{
				bool result = evaluate( line_in.ln , line.substr(7U) , m_expander , m_vars ) ;
				stack.set( result ) ;
			}
			else if( lline.find("else") == 0U )
			{
				stack.flip() ;
			}
			else if( lline.find("endif") == 0U )
			{
				stack.pop() ;
			}
			else if( lline.find("include") == 0U )
			{
				throw std::runtime_error( "not implemented" ) ;
			}
			else
			{
				throw std::runtime_error( "unknown directive [!"+line+"]" ) ;
			}
			line_in.ignore = true ;
		}
		else if( stack.active() )
		{
			process( line_in ) ;
		}
		else
		{
			line_in.ignore = true ;
		}
	}
}

void Parser::process( Line & line )
{
	std::smatch match ;
	if( std::regex_match( line.s , match , m_re_assignment ) )
	{
		// accumulate assignments into m_vars -- expanded when used, not
		// when defined
		std::string lhs = match[1] ;
		std::string rhs = match[2] ;
		if( rhs.find( "$("+lhs+")" ) != std::string::npos )
		{
			G_DEBUG( "Parser::process: self assigment: before: [" << lhs << "]=[" << rhs << "]" ) ;
			G::Str::replaceAll( rhs , "$("+lhs+")" , m_vars.get(lhs) ) ;
			G_DEBUG( "Parser::process: self assigment: after: [" << lhs << "]=[" << rhs << "]" ) ;
		}
		else if( lhs.size() == 1U && rhs.find( "$"+lhs ) != std::string::npos )
		{
			G_DEBUG( "Parser::process: self assigment: before: [" << lhs << "]=[" << rhs << "]" ) ;
			G::Str::replaceAll( rhs , "$"+lhs , m_vars.get(lhs) ) ;
			G_DEBUG( "Parser::process: self assigment: after: [" << lhs << "]=[" << rhs << "]" ) ;
		}
		m_vars.insert( {lhs,rhs} ) ;
		line.ignore = true ;
	}
	else
	{
		// expand using current vars -- a second round of expansion for
		// just the specials (eg. $*) will be required so we do not
		// expanded them here -- but for implicit rule actions and
		// associated here-documents this first-round expansion will
		// need to be discarded and re-done using the final EOF
		// variables
		//
		line.e = m_expander.expand( line.s , m_vars , Expander::stopList() ) ;
	}
}

int Parser::evaluate( int ln , const std::string & s , const Expander & expander , const Vars & vars )
{
	// Expression::evaluate() with exceptions rethrown including context
	try
	{
		return Expression::evaluate( s , expander , vars ) ;
	}
	catch( Expression::Error & e )
	{
		std::ostringstream ss ;
		ss << m_makefile << "(" << ln << "): " << e.what() ;
		throw std::runtime_error( ss.str() ) ;
	}
}

