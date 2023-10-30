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
/// \file rules.cpp
///

#include "gdef.h"
#include "rules.h"
#include "gstr.h"
#include "gassert.h"
#include "glog.h"
#include <regex>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <iterator>
#include <algorithm>

namespace RulesImp
{
	struct Error : std::runtime_error
	{
		explicit Error( std::string s ) : std::runtime_error(s) {}
		explicit Error( const Line & line ) : std::runtime_error("error on line "+std::to_string(line.ln)+": "+line.s) {}
		explicit Error( unsigned ln ) : std::runtime_error("error on line "+G::Str::fromInt(ln)) {}
		Error( const Line & line , std::string b ) : std::runtime_error("error on line "+std::to_string(line.ln)+": "+b) {}
	} ;
	bool addRule( Rules & , const Rule & ) ;
	void addAction( Rules & , std::size_t current , const Line & ) ;
	void addHereDocLine( Rules & , std::size_t current , const Line & ) ;
	void closeHereDoc( Rules & , std::size_t current , const std::string & ) ;
}

Rules::Rules( const std::vector<Line> & lines )
{
	std::regex re_action( R"(^\s+(.*))" ) ;
	std::regex re_implicit( R"(^\.([a-zA-Z0-9]+)\.([a-zA-Z0-9]+)\s*(:+)\s*(.*)$)" ) ;
	std::regex re_explicit( R"(^([^:]+)(:+)\s*(.*))" ) ; // after re_action -- TODO limit to one or two colons
	std::regex re_suffixes( R"(^\.SUFFIXES\s*:\s*(.*))" ) ;

	// there are multiple current rules if the lhs is multi-part --
	// the 'current' value is the index of the first of them --
	// actions are added to all current rules -- if the current
	// rule is implicit then 'current' takes a special value
	//
	static constexpr std::size_t unset = std::numeric_limits<std::size_t>::max() ;
	std::size_t current = unset ;

	bool in_heredoc = false ;
	for( const auto & line : lines )
	{
		if( line.ignore )
			continue ;

		if( in_heredoc && line.s.find("<<") == 0U )
		{
			G_ASSERT( current != unset ) ;
			RulesImp::closeHereDoc( *this , current , line.s ) ;
			in_heredoc = false ;
			continue ;
		}
		else if( in_heredoc )
		{
			G_ASSERT( current != unset ) ;
			G_DEBUG( "Rules::ctor: heredoc: [" << line.s << "]" ) ;
			RulesImp::addHereDocLine( *this , current , line ) ;
			continue ;
		}

		std::smatch m ;
		if( std::regex_match( line.s , m , re_suffixes ) )
		{
			G_DEBUG( "Rules::ctor: suffixes: [" << m[1] << "]" ) ;
			m_suffixes_set = true ;
			auto suffixes = G::Str::splitIntoTokens( m[1] , " \t" ) ;
			if( suffixes.empty() )
				m_suffixes.clear() ;
			else
				m_suffixes.insert( m_suffixes.end() , suffixes.begin() , suffixes.end() ) ;
		}
		else if( std::regex_match( line.e , m , re_implicit ) )
		{
			G_DEBUG( "Rules::ctor: implicit: [" << m[1] << "][" << m[2] << "]" ) ;

			std::string colons = m[3] ;
			std::string tail = m[4] ;

			std::string command ;
			if( !tail.empty() && tail.at(0) == ';' )
				command = tail.substr(1U) ;

			m_implicit_rules.push_back( ImplicitRule{line.ln,m[1],m[2],{},colons.size()>=2U} ) ;
			current = m_rules.size()+1U ; // sic

			if( !command.empty() )
				m_implicit_rules.back().m_actions.push_back( Action{Line{command,line.ln},HereDocType::none,{}} ) ;
		}
		else if( std::regex_match( line.s , m , re_action ) ) // first
		{
			if( current == unset )
				throw RulesImp::Error( line , "invalid indent" ) ;

			G_DEBUG( "Rules::ctor: action: [" << line.e << "]" ) ;
			in_heredoc = line.s.find("<<") != std::string::npos ;
			RulesImp::addAction( *this , current , line ) ;
		}
		else if( std::regex_match( line.s , m , re_explicit ) ) // second
		{
			// rematch the regex using line.e rather than line.s
			bool rematched = std::regex_match( line.e , m , re_explicit ) ;
			if( !rematched )
				throw RulesImp::Error( line , "invalid rule expansion" ) ; // eg. blank lhs

			std::string lhs_match = m[1] ;
			std::string sep = m[2] ;
			std::string rhs_match = m[3] ;
			G_DEBUG( "Rules::ctor: explicit: [" << lhs_match << "] " << sep << " [" << rhs_match << "]" ) ;
			if( sep.size() > 2U )
				throw RulesImp::Error( line , "invalid dependency separator" ) ;

			if( lhs_match.find_first_not_of(" \t") == std::string::npos )
				throw RulesImp::Error( line , "invalid rule expansion" ) ; // eg. blank lhs

			auto lhs_list = G::Str::splitIntoTokens( lhs_match , " \t" ) ;
			auto rhs_list = G::Str::splitIntoTokens( rhs_match , " \t" ) ;
			for( const auto & lhs : lhs_list )
			{
				RulesImp::addRule( *this , Rule{line.ln,lhs,rhs_list,{},sep.size()==2U,false} ) ;
			}
			current = m_rules.size() - lhs_list.size() ;
		}
		else if( line.s.empty() )
		{
			// a completely empty line terminates a list of actions,
			// but not a line of whitespace or a variable that
			// expands to nothing
			current = unset ;
		}
		else if( line.s.find_first_not_of(" \t") == std::string::npos )
		{
		}
		else
		{
			throw RulesImp::Error( line , "unexpected line format: ["+line.s+"]["+line.e+"]" ) ;
		}
	}
}

std::ostream & operator<<( std::ostream & stream , const Rules & r )
{
	if( !r.m_implicit_rules.empty() )
	{
		stream << "INFERENCE RULES:\n\n" ;
		for( const auto & irule : r.m_implicit_rules )
		{
			stream << "." << irule.m_lhs << "." << irule.m_rhs << ":\n" ;
			bool first = true ;
			for( const auto & action : irule.m_actions )
			{
				if( first )
					stream << "    commands:   " ;
				else
					stream << "                " ;
				stream << G::Str::unique(G::Str::trimmed(action.line.s," \t"),' ',' ') << "\n" ;
				for( const auto & hdline : action.heredoc_lines )
					stream << "  <<" << G::Str::trimmed(hdline.s," \t") << "\n" ;
				first = false ;
			}
			stream << "\n" ;
		}
	}
	if( !r.m_suffixes.empty() )
	{
		stream << ".SUFFIXES: " << G::Str::join(" ",r.m_suffixes) << "\n\n" ;
	}
	if( !r.m_rules.empty() )
	{
		stream << "TARGETS:\n\n" ;
		for( const auto & rule : r.m_rules )
		{
			stream << rule.m_lhs << ":\n" ;

			stream << "   dependents: " ;
			for( std::size_t i = 0U ; i < rule.m_rhs.size() ; i++ )
			{
				if( i && (i%5U) == 0 )
					stream << "               " ;
				stream << rule.m_rhs[i] ;
				if( (i+1U) < rule.m_rhs.size() && ((i+1U)%5U) == 0 )
					stream << "\n" ;
				else
					stream << " " ;
			}
			stream << "\n" ;

			stream << "   commands: " ;
			if( rule.m_actions.empty() )
				stream << "\n" ;
			for( std::size_t i = 0U ; i < rule.m_actions.size() ; i++ )
			{
				if( i != 0U )
					stream << "             " ;
				stream << G::Str::trimmed(rule.m_actions[i].line.s," \t") << "\n" ;
				for( const auto & hdline : rule.m_actions[i].heredoc_lines )
					stream << "                <<" << G::Str::trimmed(hdline.s," \t") << "\n" ;
			}
			stream << "\n" ;
		}
		stream << "\n" ;
	}
	stream.flush() ;
	return stream ;
}

std::ostream & operator<<( std::ostream & stream , const Rule & rule )
{
	stream << "rule: [" << rule.m_lhs << "]<-[" << G::Str::join("][",rule.m_rhs) << "]" ;
	if( !rule.m_actions.empty() )
	{
		stream << ": " ;
		for( const auto & action : rule.m_actions )
		{
			stream << "[" << G::Str::trimmed(action.line.s," \t") << "]" ;
			for( const auto & hdline : action.heredoc_lines )
				stream << "(" << G::Str::trimmed(hdline.s," \t") << ")" ;
		}
	}
	stream.flush() ;
	return stream ;
}

bool RulesImp::addRule( Rules & r , const Rule & new_rule )
{
	G_ASSERT( new_rule.m_actions.empty() ) ; // not yet populated

	// merge duplicates

	auto rule_p = std::find_if( r.m_rules.begin() , r.m_rules.end() ,
		[new_rule](const Rule &rule_){return rule_.m_lhs==new_rule.m_lhs;} ) ;

	if( rule_p == r.m_rules.end() )
	{
		r.m_rules.push_back( new_rule ) ;
		return true ; // added
	}
	else if( (*rule_p).double_colon )
	{
		r.m_rules.push_back( new_rule ) ;
		return true ; // added
	}
	else
	{
		G_DEBUG( "RulesImp::addRule: duplicate rule for [" << new_rule.m_lhs << "] (" << (*rule_p).ln << "," << new_rule.ln << ")" ) ;

		if( (*rule_p).double_colon != new_rule.double_colon )
			throw Error( "cannot have : and :: for the same target" ) ;

		// mark as locked if there are actions so future duplicates cannot add more
		if( !(*rule_p).double_colon && !(*rule_p).m_actions.empty() )
			(*rule_p).locked = true ;

		// combine rhs lists, preserving ordering and removing duplicates
		for( auto const & rhs : new_rule.m_rhs )
		{
			if( std::find((*rule_p).m_rhs.begin(),(*rule_p).m_rhs.end(),rhs) == (*rule_p).m_rhs.end() )
				(*rule_p).m_rhs.push_back( rhs ) ;
		}

		// swap the matching rule to the end of the list
		if( r.m_rules.size() > 1U )
			std::iter_swap( rule_p , std::next(r.m_rules.begin(),r.m_rules.size()-1U) ) ;
		return false ;
	}
}

void RulesImp::addAction( Rules & r , std::size_t current , const Line & action_line )
{
	G_ASSERT( current < r.m_rules.size() || current == (r.m_rules.size()+1U) ) ;
	G_ASSERT( current < r.m_rules.size() || !r.m_implicit_rules.empty() ) ;
	if( current > r.m_rules.size() )
	{
		r.m_implicit_rules.back().m_actions.push_back( Action{action_line,HereDocType::none,{}} ) ;
	}
	else
	{
		for( std::size_t i = current ; i < r.m_rules.size() ; i++ )
		{
			if( r.m_rules.at(i).locked )
			{
				std::ostringstream ss ;
				ss
					<< "duplicate rule for [" << r.m_rules.at(i).m_lhs << "] cannot have actions: "
					<< " previously on line " << r.m_rules.at(i).m_actions.at(0).line.ln ;
				throw Error( action_line , ss.str() ) ;
			}
			r.m_rules.at(i).m_actions.push_back( Action{action_line,HereDocType::none,{}} ) ;
		}
	}
}

void RulesImp::addHereDocLine( Rules & r , std::size_t current , const Line & heredoc_line )
{
	G_ASSERT( current < r.m_rules.size() || current == (r.m_rules.size()+1U) ) ;
	G_ASSERT( current < r.m_rules.size() || !r.m_implicit_rules.empty() ) ;
	if( current > r.m_rules.size() )
	{
		G_ASSERT( !r.m_implicit_rules.back().m_actions.empty() ) ;
		r.m_implicit_rules.back().m_actions.back().heredoc_lines.push_back( heredoc_line ) ;
	}
	else
	{
		for( std::size_t i = current ; i < r.m_rules.size() ; i++ )
		{
			G_ASSERT( !r.m_rules[i].m_actions.empty() ) ;
			r.m_rules[i].m_actions.back().heredoc_lines.push_back( heredoc_line ) ;
		}
	}
}

void RulesImp::closeHereDoc( Rules & r , std::size_t current , const std::string & line )
{
	G_ASSERT( current < r.m_rules.size() || current == (r.m_rules.size()+1U) ) ;
	G_ASSERT( current < r.m_rules.size() || !r.m_implicit_rules.empty() ) ;
	HereDocType heredoc_type = HereDocType::simple ;
	if( line.find("<<NOKEEP") == 0 ) heredoc_type = HereDocType::simple ;
	if( line.find("<<KEEP") == 0 ) heredoc_type = HereDocType::keep ;
	if( line.find("<<UNICODE") == 0 ) heredoc_type = HereDocType::unicode ;
	if( current > r.m_rules.size() )
	{
		G_ASSERT( !r.m_implicit_rules.back().m_actions.empty() ) ;
		r.m_implicit_rules.back().m_actions.back().heredoc_type = heredoc_type ;
	}
	else
	{
		for( std::size_t i = current ; i < r.m_rules.size() ; i++ )
		{
			G_ASSERT( !r.m_rules[i].m_actions.empty() ) ;
			r.m_rules[i].m_actions.back().heredoc_type = heredoc_type ;
		}
	}
}

std::ostream & operator<<( std::ostream & stream , const Action & a )
{
	return stream << G::Str::trimmed( a.line.s , " \t" ) ; // or maybe line.e
}

std::ostream & operator<<( std::ostream & stream , const std::vector<Action> & aa )
{
	for( const auto & a : aa )
		stream << "[" << a << "]" ;
	return stream ;
}

