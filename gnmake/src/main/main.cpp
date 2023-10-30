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
/// \file main.cpp
///

#include "gdef.h"
#include "config.h"
#include "parser.h"
#include "makefile.h"
#include "garg.h"
#include "gstr.h"
#include "gstringlist.h"
#include "gstringmap.h"
#include "gprocess.h"
#include "glogoutput.h"
#include "genvironment.h"
#include "glog.h"
#include <string>
#include <stdexcept>
#include <cctype>

const char * disclaimer =
        "This program comes with ABSOLUTELY NO WARRANTY." "\n"
        "This is free software, and you are welcome to " "\n"
        "redistribute it under certain conditions. For " "\n"
        "more information refer to the file named COPYING." ;

const char * copyright = "Copyright (c) 2023 Graeme Walker" ;

struct UsageError : std::runtime_error
{
	UsageError( const std::string & s ) : std::runtime_error( s ) {}
} ;

struct Exit
{
	int rc ;
	Exit( int rc_ ) : rc(rc_) {}
} ;

int parseOption( const std::string & arg_in , Config & config , std::string & makefile_path , std::string & makeflags )
{
	std::string arg = G::Str::lower( arg_in ) ;
	if( arg == "/?" || arg == "/help" || arg == "-h" || arg == "--help" )
	{
		std::cout
			<< "usage: " << config.prefix << " [<options> [[<macros> [[<targets>]]]" << "\n"
			<< "\n"
			<< "options:\n"
			<< " /f <filename>       filename\n"
			<< " /?                  show this help\n"
			<< " /a                  build all evaluated targets\n"
			//<< " /b                  build if time stamps are equal\n"
			<< " /c                  suppress output messages\n"
			//<< " /d                  display build information\n"
			<< " /e                  override env-var macros\n"
			//<< " /errorreport:<type> report errors\n"
			//<< " /g                  display '!include' filenames\n"
			<< " /help               show this help\n"
			<< " /i                  ignore exit codes from commands\n"
			//<< " /k                  build unrelated targets on error\n"
			<< " /n                  display commands but do not execute\n"
			<< " /nologo             suppress copyright message\n"
			<< " /p                  display NMAKE information\n"
			//<< " /q                  check time stamps but do not build\n"
			//<< " /r                  ignore predefined rules/macros\n"
			<< " /s                  suppress executed-commands display\n"
			//<< " /t                  change time stamps but do not build\n"
			//<< " /u                  dump inline files\n"
			//<< " /y                  disable batch-mode\n"
			<< "\n"
			<< disclaimer << "\n"
			<< "\n"
			<< copyright << "\n" ;
		throw Exit( 1 ) ;
	}
	if( arg == "--verbose" )
	{
		config.verbose = true ;
	}
	else if( arg == "--log-start" )
	{
		config.log_start = true ;
	}
	else if( arg == "--log-makefile" )
	{
		config.log_makefile = true ;
	}
	else if( arg == "--log-running" )
	{
		config.log_running = true ;
	}
	else if( arg == "--debug" )
	{
		config.verbose = true ;
		config.debug++ ;
	}
	else if( arg == "--keep" )
	{
		config.keep = true ;
	}
	else if( arg == "/nologo" || arg == "-nologo" )
	{
		config.nologo = true ;
	}
	else if( arg == "/f" || arg == "-f" )
	{
		return 2 ; // next command-line argument is option parameter
	}
	else if( ( arg[0] == '/' || arg[0] == '-' ) && arg[1] == 'f' )
	{
		makefile_path = arg_in.substr( 2U ) ;
		return 1 ;
	}
	else if( arg[0] == '/' || ( !G::is_windows() && arg[0] == '-' ) )
	{
		for( char c : arg.substr(1U) )
		{
			if( c == 'p' )
			{
				config.dump = true ;
				makeflags.append( 1U , 'P' ) ;
			}
			else if( c == 'c' )
			{
				config.quiet = true ;
				makeflags.append( 1U , 'C' ) ;
			}
			else if( c == 'e' )
			{
				config.force_env = true ; // environment overrides makefile
				makeflags.append( 1U , 'E' ) ;
			}
			else if( c == 'i' )
			{
				config.ignore = true ;
				makeflags.append( 1U , 'I' ) ;
			}
			else if( c == 'n' )
			{
				config.dry_run = true ;
				makeflags.append( 1U , 'N' ) ;
			}
			else if( c == 'a' )
			{
				config.all = true ;
				makeflags.append( 1U , 'A' ) ;
			}
			else if( c == 's' )
			{
				config.silent = true ;
				makeflags.append( 1U , 'S' ) ;
			}
			else
			{
				throw UsageError( "unknown option character ["+std::string(1U,c)+"] in ["+arg+"]" ) ;
			}
		}
	}
	else if( arg[0] == '/' || ( !G::is_windows() && arg[0] == '-' ) )
	{
		throw UsageError( "unknown option ["+arg+"]" ) ;
	}
	else
	{
		return 0 ; // not an option
	}
	return 1 ;
}

bool parseMacro( const std::string & arg , Vars & vars )
{
	std::size_t pos = arg.find( '=' ) ;
	if( pos == std::string::npos )
		return false ;
	auto var = std::make_pair( G::Str::head(arg,pos) , G::Str::tail(arg,pos) ) ;
	if( var.first.empty() )
		throw UsageError( "invalid macro ["+arg+"]" ) ;
	vars.insert( var ) ;
	return true ;
}

void parseTarget( const std::string & arg , G::StringArray & targets )
{
	targets.push_back( arg ) ;
}

void runCore( const Config & , Vars , const G::StringArray & targets , const std::string & makefile_path ) ;

int run( G::Arg args )
{
	std::string more = G::Environment::get( "GNMAKE_OPTIONS" , "" ) ;
	if( !more.empty() )
	{
		G::Str::replaceAll( more , "_" , "-" ) ;
		G::StringArray args_ = args.array() ;
		G::StringArray more_args = G::Str::splitIntoTokens( more , " ," ) ;
		args_.insert( std::next(args_.begin()) , more_args.begin() , more_args.end() ) ;
		args = G::Arg( args_ ) ;
	}

	Config config( args.prefix() ) ;
	std::string makefile_path = "Makefile" ;

	Vars vars ;
	std::string makeflags ;
	G::StringArray targets ;
	bool in_options = true ;
	for( auto arg_p = args.cbegin() ; arg_p != args.cend() ; )
	{
		std::string arg = *arg_p ;
		if( arg.empty() ) continue ;
		if( in_options )
		{
			int n = parseOption( arg , config , makefile_path , makeflags ) ;
			if( n == 2 )
				makefile_path = *std::next(arg_p) ;
			std::advance( arg_p , n ) ;
			if( n <= 0 )
				in_options = false ;
		}
		else
		{
			if( arg == "-nologo" ) // ffs
				;
			else if( arg.find('=') != std::string::npos )
				parseMacro( arg , vars ) ;
			else
				parseTarget( arg , targets ) ;
			++arg_p ;
		}
	}

	G::LogOutput log_output( args.prefix() ,
		G::LogOutput::Config()
			.set_stdout()
			.set_output_enabled(config.verbose||config.log_start||config.log_makefile)
			.set_summary_info(config.verbose||config.log_start||config.log_makefile)
			.set_verbose_info(config.verbose)
			.set_more_verbose_info(config.verbose)
			.set_debug(config.debug)
			.set_strip(false)
			.set_with_level() ) ;

	vars.insert( "MAKEFLAGS" , makeflags ) ;
	vars.insert( "MAKEDIR" , G::Process::cwd() ) ;
	vars.setDefault( "MAKE" , args.v0() ) ;
	vars.setDefault( "AS" , "ml64" ) ;
	vars.setDefault( "CC" , "cl" ) ;
	vars.setDefault( "CPP" , "cl" ) ;
	vars.setDefault( "CXX" , "cl" ) ;
	vars.setDefault( "LINK" , "link" ) ;
	vars.setDefault( "RC" , "rc" ) ;

	if( config.log_start )
	{
		G_LOG_S( "main: start: cwd=[" << G::Process::cwd() << "]" ) ;
		G_LOG_S( "main: start: pid=[" << G::Process::Id() << "]" ) ;
		G_LOG_S( "main: start: args: [" << G::Str::join("][",args.array(1U)) << "]" ) ;
		G_LOG_S( "main: start: makefile: [" << makefile_path << "]" ) ;
	}

	try
	{
		runCore( config , vars , targets , makefile_path ) ;

		if( config.log_start )
			G_LOG_S( "main: end: pid=[" << G::Process::Id() << "]: exit 0" ) ;
		return 0 ;
	}
	catch( std::exception & e )
	{
		G_ERROR( "main: exception: " << e.what() ) ;
		if( config.log_start )
			G_LOG_S( "main: end: pid=[" << G::Process::Id() << "]: exit 2" ) ;
		throw ;
	}
	catch(...)
	{
		if( config.log_start )
			G_LOG_S( "main: end: pid=[" << G::Process::Id() << "]: exit 2" ) ;
		throw ;
	}
}

void runCore( const Config & config , Vars vars , const G::StringArray & targets , const std::string & makefile_path )
{
	Expander expander( config ) ;
	Parser parser( config , expander , vars ) ;
	Parser::Lines lines = parser.parse( makefile_path ) ;
	Rules rules( lines ) ;
	Makefile makefile( config , expander , vars , rules ) ;
	if( config.dump )
		makefile.dump( std::cout ) ;

	if( targets.empty() )
	{
		makefile.make() ;
	}
	else
	{
		for( const auto & target : targets )
			makefile.make( target ) ;
	}
}

int main( int argc , char * argv [] )
{
	try
	{
		G::Arg args( argc , argv ) ;
		return run( args ) ;
	}
	catch( Exit & e )
	{
		return e.rc ;
	}
	catch( UsageError & e )
	{
		std::cerr << "usage error: " << e.what() << "\n" ;
	}
	catch( std::exception & e )
	{
		std::cerr << G::Arg::prefix(argv) << ": error: " << e.what() << "\n" ;
	}
	return 2 ;
}

