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
/// \file config.h
///

#ifndef G_NMAKE_CONFIG_H
#define G_NMAKE_CONFIG_H

#include "gdef.h"
#include "gstringmap.h"
#include "gstringarray.h"
#include <iostream>
#include <string>

struct Config
{
	explicit Config( const std::string & prefix_ ) : prefix(prefix_) {}
	Config() {}
	std::string prefix ;
	bool dump {false} ;
	bool quiet {false} ;
	bool nologo {false} ;
	bool force_env {false} ;
	bool verbose {false} ;
	int debug {0} ;
	bool log_start {false} ; // see main()
	bool log_makefile {false} ; // see Parser::parse()
	bool log_running {false} ;
	bool ignore {false} ;
	bool dry_run {false} ;
	bool all {false} ;
	bool silent {false} ;
	bool keep {false} ;
	bool strict {false} ; // wrt. "foo:" vs "foo::" rules
} ;
std::ostream & operator<<( std::ostream & , const Config & ) ;

#endif
