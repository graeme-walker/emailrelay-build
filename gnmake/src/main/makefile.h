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
/// \file makefile.h
///

#ifndef G_NMAKE_MAKEFILE_H
#define G_NMAKE_MAKEFILE_H

#include "gdef.h"
#include "line.h"
#include "config.h"
#include "rules.h"
#include "vars.h"
#include "expander.h"
#include "gdatetime.h"
#include <vector>
#include <iostream>

class Makefile : private Rules
{
public:
	Makefile( const Config & , Expander & , Vars & , const Rules & ) ;

	void make( const std::string & target = {} ) ;

	void dump( std::ostream & ) const ;

private:
	using Time = G::SystemTime ;
	void expandSuffixes() ;
	void filterImplicitRules() ;
	std::pair<bool,Time> makeImp( const std::string & , int depth = 0 , const Rule * = nullptr ) ;
	std::vector<Rule> findRules( const std::string & ) ;
	Rule makeRule( const std::string & ) ;
	bool hasRuleWithActions( const std::string & ) ;
	bool runActions( const Rule & , bool , const G::StringArray & oods ) ;
	static bool trivial( const std::string & ) ;
	static bool valid( const Rule & ) ;
	static Rule norule() ;
	static Time timestamp( const std::string & ) ;
	void addDefaultImplicitRule( const std::string & , const std::string & ,
		const std::string & ) ;

private:
	Config m_config ;
	Expander & m_expander ;
	Vars & m_vars ;
} ;

#endif
