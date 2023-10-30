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
/// \file expander.h
///

#ifndef G_NMAKE_EXPANDER_H
#define G_NMAKE_EXPANDER_H

#include "gdef.h"
#include "config.h"
#include "vars.h"
#include "gstringview.h"
#include <regex>

class Expander
{
public:
	explicit Expander( const Config & ) ;
		///< Constructor.

	explicit Expander( bool force_env , bool debug ) ;
		///< Constructor for testing purposes.

	std::string expand( std::string , const Vars & ,
		G::StringArray stop_list = stopList() , bool with_env = true ) const ;
			///< Expands sub-strings like $X, $(FOO) and $(FOO:a=b)
			///< in the string using the given variable values and
			///< (optionally) all environment variables. If 'force-env'
			///< applies (and 'with-env') then environment variables
			///< take precedence and 'with-env' is ignored.
			///<
			///< All single-character variables are interpreted
			///< as if bracketed, so $X is treated like $(X).
			///<
			///< Variables in the stop-list are not expanded. This
			///< includes substitutions, so if "FOO" is in the
			///< stop-list then sub-strings like "$(FOO:a=b)" will
			///< not be expanded either.

	static G::StringArray stopList() ;
		///< Returns the canonical stop-list, including "*", "*F"
		///< "*B", "*D", "@", etc.

private:
	static std::string lookup( const std::string & , const Vars & ) ;
	std::string expandImp( std::string , const Vars & ,
		G::StringArray stop_list , bool with_env , int ) const ;

private:
	bool m_force_env ;
	int m_debug ;
	std::regex m_re ;
} ;

#endif
