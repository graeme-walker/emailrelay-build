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
/// \file rules.h
///

#ifndef G_NMAKE_RULES_H
#define G_NMAKE_RULES_H

#include "gdef.h"
#include "line.h"
#include "gstringview.h"
#include "gstringarray.h"
#include <vector>
#include <iostream>

enum class HereDocType
{
	none ,
	simple ,
	keep ,
	nokeep ,
	unicode
} ;

struct Action
{
	Line line ;
	HereDocType heredoc_type ;
	std::vector<Line> heredoc_lines ;
} ;
std::ostream & operator<<( std::ostream & , const Action & ) ;
std::ostream & operator<<( std::ostream & , const std::vector<Action> & ) ;

struct ImplicitRule
{
	unsigned ln ;
	std::string m_lhs ; // "c"
	std::string m_rhs ; // "obj"
	std::vector<Action> m_actions ;
	bool batch_mode ; // two colons
} ;

struct Rule
{
	unsigned ln ; // zero if synthesised from ImplicitRule
	std::string m_lhs ;
	G::StringArray m_rhs ;
	std::vector<Action> m_actions ;
	bool double_colon ;
	bool locked ;
} ;
std::ostream & operator<<( std::ostream & , const Rule & ) ;

struct Rules
{
	explicit Rules( const std::vector<Line> & ) ;
	G::StringArray m_suffixes ;
	bool m_suffixes_set {false} ;
	std::vector<ImplicitRule> m_implicit_rules ;
	std::vector<Rule> m_rules ;
} ;
std::ostream & operator<<( std::ostream & , const Rules & ) ;

#endif
