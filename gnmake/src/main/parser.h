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
/// \file parser.h
///

#ifndef G_NMAKE_PARSER_H
#define G_NMAKE_PARSER_H

#include "gdef.h"
#include "config.h"
#include "expander.h"
#include "expression.h"
#include "line.h"
#include "rules.h"
#include "vars.h"
#include "gstringarray.h"
#include <string>
#include <vector>
#include <regex>

class Parser
{
public:
	using Lines = std::vector<Line> ;

	Parser( const Config & , Expander & , Vars & ) ;
		///< Constructor.

	Lines parse( const std::string & filename ) ;
		///< Parses the file and returns the parsed file lines.
		///< Assigments are interpreted and variables are expanded
		///< from Line::s to Line::e. Lines that are assigments
		///< or iffed out are marked as 'ignore'd.

private:
	void parse( Lines & ) ;
	void process( Line & ) ;
	void process( Line & , bool ) ;
	int evaluate( int , const std::string & , const Expander & , const Vars & ) ;

private:
	Config m_config ;
	Expander & m_expander ;
	Vars & m_vars ;
	std::string m_makefile ;
	std::regex m_re_comment ;
	std::regex m_re_assignment ;
	bool m_implicit_action {false} ;
} ;

#endif
