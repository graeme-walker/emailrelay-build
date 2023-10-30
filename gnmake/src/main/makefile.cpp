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
/// \file makefile.cpp
///

#include "gdef.h"
#include "makefile.h"
#include "cwd.h"
#include "gprocess.h"
#include "gfile.h"
#include "gscope.h"
#include "gtempfile.h"
#include "gassert.h"
#include "glog.h"
#include <algorithm>
#include <cstdlib> // std::system()
#include <sstream>
#include <limits>

namespace MakefileImp
{
	struct Error : std::runtime_error
	{
		explicit Error( std::string s ) : std::runtime_error(s) {}
	} ;
	struct CommandError : std::runtime_error
	{
		std::string str( const Rule & rule , int rc , const Action & action , const std::string & cmd , const std::string cwd )
		{
			std::ostringstream ss ;
			ss
				<< "command failed with exit code " << rc
				<< " when running [" << cmd << "]"
				<< " in [" << cwd << "]"
				<< " from line " << action.line.ln ;
			if( !action.heredoc_lines.empty() )
			{
				ss << " with here-document " ;
				for( auto hdl : action.heredoc_lines )
					ss << "(" << G::Str::trimmed(G::Str::unique(G::Str::replaced(hdl.e,'\t',' '),' ',' ')," ") << ")" ;
			}
			ss
				<< " to make [" << rule.m_lhs << "]" ;
			if( !rule.m_rhs.empty() )
				ss << " having dependents [" << G::Str::join("][",rule.m_rhs) << "]" ;
			return ss.str() ;
		}
		CommandError( const Rule & rule , int rc , const Action & action , const std::string & cmd , const std::string cwd ) :
			std::runtime_error( str(rule,rc,action,cmd,cwd) )
		{
		}
	} ;
}

Makefile::Makefile( const Config & config , Expander & expander , Vars & vars , const Rules & rules ) :
	Rules(rules) ,
	m_config(config) ,
	m_expander(expander) ,
	m_vars(vars)
{
	if( m_suffixes_set )
	{
		// TODO suffixes line should be expanded with variables in scope, not EOF variables
		for( auto & s : m_suffixes )
			s = m_expander.expand( s , m_vars , {} , false ) ;
	}
	else
	{
		m_suffixes = { ".obj" , ".asm" , ".c" , ".cc" , ".cpp" , ".cxx" , ".f" , ".f90" , ".for" , ".rc" } ;
	}

	addDefaultImplicitRule( "asm" , "obj" , "$(AS) $(AFLAGS) /c $<" ) ;
	addDefaultImplicitRule( "asm" , "exe" , "$(AS) $(AFLAGS) $<" ) ;
	addDefaultImplicitRule( "c" , "obj" , "$(CC) $(CFLAGS) /c $<" ) ;
	addDefaultImplicitRule( "c" , "exe" , "$(CC) $(CFLAGS) $<" ) ;
	addDefaultImplicitRule( "cc" , "obj" , "$(CC) $(CFLAGS) /c $<" ) ;
	addDefaultImplicitRule( "cc" , "exe" , "$(CC) $(CFLAGS) $<" ) ;
	addDefaultImplicitRule( "cpp" , "obj" , "$(CPP) $(CPPFLAGS) /c $<" ) ;
	addDefaultImplicitRule( "cpp" , "exe" , "$(CPP) $(CPPFLAGS) $<" ) ;
	addDefaultImplicitRule( "cxx" , "obj" , "$(CXX) $(CXXFLAGS) /c $<" ) ;
	addDefaultImplicitRule( "cxx" , "exe" , "$(CXX) $(CXXFLAGS) $<" ) ;

	filterImplicitRules() ;
}

void Makefile::filterImplicitRules()
{
	// keep implicit rules only if the source extension
	// is in .SUFFIXES -- build the replacement list in
	// suffix order
	//
	std::vector<ImplicitRule> new_list ;
	for( const auto & dot_ext : m_suffixes )
	{
		std::string ext = dot_ext.substr( 1U ) ;
		for( auto p = m_implicit_rules.begin() ; p != m_implicit_rules.end() ; )
		{
			if( (*p).m_lhs == ext )
			{
				new_list.push_back( *p ) ;
				p = m_implicit_rules.erase( p ) ;
			}
			else
			{
				++p ;
			}
		}
	}
	for( const auto & r : m_implicit_rules )
	{
		G_WARNING( "Makefile::filterImplicitRules: removing [." << r.m_lhs << "." << r.m_rhs << "]" ) ;
	}
	m_implicit_rules.swap( new_list ) ;
}

void Makefile::dump( std::ostream & stream ) const
{
	if( !m_vars.empty() )
	{
		stream << "MACROS:\n\n" ;
		for( const auto & pair : m_vars )
		{
			std::string value = pair.second ;
			G::Str::replace( value , '\t' , ' ' ) ;
			value = G::Str::trimmed( G::Str::unique(value,' ',' ') , " " ) ;
			stream
				<< std::string(25U-std::min(std::size_t(24U),pair.first.size()),' ')
				<< pair.first
				<< " = "
				<< value << "\n" ;
		}
		std::cout << "\n" ;
	}
	stream << *this << std::endl ; // base class
}

void Makefile::addDefaultImplicitRule( const std::string & lhs , const std::string & rhs ,
	const std::string & command )
{
	auto p = std::find_if( Rules::m_implicit_rules.begin() , Rules::m_implicit_rules.end() ,
		[=](const ImplicitRule &irule_){return irule_.m_lhs==lhs && irule_.m_rhs==rhs;} ) ;
	if( p == m_implicit_rules.end() )
	{
		Action action { Line(command,0) , HereDocType::none , {} } ;
		m_implicit_rules.push_back( ImplicitRule{0,lhs,rhs,{action},false} ) ;
	}
}

void Makefile::make( const std::string & target )
{
	if( target.empty() && Rules::m_rules.empty() )
	{
		throw MakefileImp::Error( "no default target" ) ;
	}
	else if( target.empty() )
	{
		unsigned ln = Rules::m_rules.at(0).ln ;
		for( const auto & rule : Rules::m_rules )
		{
			if( rule.ln == ln )
			{
				G_ASSERT( !rule.m_lhs.empty() ) ;
				makeImp( rule.m_lhs ) ;
			}
		}
	}
	else
	{
		makeImp( target ) ;
	}
}

std::pair<bool,Makefile::Time> Makefile::makeImp( const std::string & target , int depth , const Rule * parent )
{
	Time future( 9999999999 ) ;
	if( depth > 30 ) // TODO make tweakable or detect properly
		throw MakefileImp::Error( "dependency loop for [" + target + "]" ) ;

	bool built = false ;

	G_LOG( "Makefile::makeImp: " << std::string(depth,' ') << "making [" << target << "]" ) ;

	// find an explicit rule -- or more than one if double-colons
	//
	std::vector<Rule> rules = findRules( target ) ;

	// no explicit rule -- make one from an implicit rule
	if( rules.empty() )
	{
		G_DEBUG( "Makefile::makeImp: no explicit rules" ) ;
		Rule r = makeRule( target ) ; // from implicit
		if( valid(r) )
		{
			G_ASSERT( r.ln == 0 ) ;
			rules.push_back( r ) ;
			G_DEBUG( "Makefile::makeImp: made a rule: " << r ) ;
		}
	}
	else
	{
		// a rule with no actions can be an implicit rule with extra dependents
		std::size_t i = 1U ; GDEF_IGNORE_VARIABLE(i) ;
		std::size_t n = rules.size() ; GDEF_IGNORE_VARIABLE(n) ;
		for( auto & rule : rules )
		{
			G_DEBUG( "Makefile::makeImp: rule " << i << "/" << n << ": " << rule ) ;
			if( rule.m_actions.empty() && !rule.double_colon )
			{
				G_DEBUG( "Makefile::makeImp: rule " << i << "/" << n << " has no actions" ) ;
				Rule r = makeRule( target ) ; // from implicit if implicit rule defined and its source file exists
				if( valid(r) )
				{
					G_ASSERT( r.m_rhs.size() == 1U ) ;
					rule.m_rhs.insert( rule.m_rhs.begin() , r.m_rhs.at(0) ) ; // using .at(0) to expand $<
					rule.m_actions = r.m_actions ;
					rule.ln = 0 ; // => implicit-stle expansion
					G_DEBUG( "Makefile::makeImp: made a rule: " << r ) ;
				}
			}
		}
	}

	// if no rule then just check for existance
	if( rules.empty() )
	{
		Time t = timestamp( target ) ;
		if( t == Time::zero() )
		{
			std::ostringstream ss ;
			ss << "don't know how to make [" << target << "]" ;
			if( parent )
				ss << " as a dependency of [" << parent->m_lhs << "]" ;
			if( parent && parent->ln )
				ss << " (line " << parent->ln << ")" ;
			throw MakefileImp::Error( ss.str() ) ;
		}
		G_LOG( "Makefile::makeImp: " << std::string(depth,' ') << "making [" << target << "]: no rule but file exists: " << t.s() ) ;
		return std::make_pair( false , t ) ;
	}

	Time tmax = Time::zero() ;
	for( auto & rule : rules )
	{
		// do $* and $@ expansion on rule lhs and rhs
		{
			Vars svars ;
			svars.reserve( 10U ) ;
			svars.insert( "@" , rule.m_lhs ) ;
			svars.insert( "@B" , G::Path(rule.m_lhs).withoutExtension().basename() ) ;
			svars.insert( "@F" , G::Path(rule.m_lhs).basename() ) ;
			svars.insert( "@D" , G::Path(rule.m_lhs).dirname().str() ) ;
			svars.insert( "*" , G::Path(rule.m_lhs).withoutExtension().str() ) ;
			svars.insert( "*B" , G::Path(rule.m_lhs).withoutExtension().basename() ) ;
			svars.insert( "*F" , G::Path(rule.m_lhs).withoutExtension().basename() ) ;
			svars.insert( "*D" , G::Path(rule.m_lhs).dirname().str() ) ;
			rule.m_lhs = m_expander.expand( rule.m_lhs , svars , {} , false ) ;
			for( auto & rhs : rule.m_rhs )
				rhs = m_expander.expand( rhs , svars , {} , false ) ;
		}

		G_ASSERT( target == rule.m_lhs ) ;
		G_LOG( "Makefile::makeImp: " << std::string(depth,' ') << "making [" << target << "]: dependents: " << G::Str::join(" ",rule.m_rhs) ) ;

		// make dependents
		G::StringArray oods ;
		if( !rule.m_rhs.empty() )
		{
			for( const auto & rhs : rule.m_rhs )
			{
				if( rhs.empty() )
					continue ; // expanded to nothing above -- never gets here?

				std::pair<bool,Time> pair = makeImp( rhs , depth+1 , &rule ) ;
				bool built_rhs = pair.first ;
				Time t = pair.second ;

				if( t == Time::zero() || t > timestamp(rule.m_lhs) || built_rhs ) // TODO check
				{
					built = true ;
					oods.push_back( rhs ) ;
				}

				tmax = std::max( tmax , t ) ;
			}
		}

		//G_LOG( "Makefile::makeImp: " << std::string(depth,' ') << "making [" << target << "]: no dependents" ) ;
		//G_LOG( "Makefile::makeImp: " << std::string(depth,' ') << "making [" << target << "]: latest dependent " << tmax.s() ) ;

		// make if out-of-date
		Time t = timestamp( target ) ;
		if( t <= tmax )
		{
			built = true ;
			if( rule.m_actions.empty() )
			{
				G_LOG( "Makefile::makeImp: " << std::string(depth,' ') << "making [" << target << "]: no actions" ) ;
			}
			else
			{
				G_LOG( "Makefile::makeImp: " << std::string(depth,' ') << "making [" << target << "]: actions: " << rule.m_actions ) ;
				runActions( rule , rule.ln == 0U , oods ) ;
			}
			Time t_new = timestamp( target ) ;
			if( t_new > tmax )
				tmax = t_new ;
		}
		else
		{
			G_LOG( "Makefile::makeImp: " << std::string(depth,' ') << "making [" << target << "]: already up-to-date: " << t.s() << " >= " << tmax.s() ) ;
		}
	}
	return std::make_pair( built , tmax == Time::zero() ? future : tmax ) ;
}

bool Makefile::runActions( const Rule & rule , bool implicit , const G::StringArray & oods )
{
	G_ASSERT( valid(rule) ) ;
	if( rule.m_actions.empty() )
	{
		G_DEBUG( "Makefile::runActions: no actions to make [" << rule.m_lhs << "]" ) ;
		return false ;
	}

	Vars svars ;
	if( implicit )
	{
		G::Path lhs( rule.m_lhs ) ;
		G::Path rhs( rule.m_rhs.at(0) ) ;
		svars.insert( "*" , rhs.withoutExtension().str() ) ;
		svars.insert( "*B" , rhs.withoutExtension().basename() ) ;
		svars.insert( "*F" , rhs.withoutExtension().basename() ) ;
		svars.insert( "*D" , rhs.dirname().str() ) ;
		svars.insert( "@" , lhs.str() ) ;
		svars.insert( "@B" , lhs.withoutExtension().basename() ) ;
		svars.insert( "@F" , lhs.basename() ) ;
		svars.insert( "@D" , lhs.dirname().str() ) ;
		svars.insert( "<" , rhs.str() ) ;
		svars.insert( "<B" , rhs.withoutExtension().basename() ) ;
		svars.insert( "<F" , rhs.basename() ) ;
		svars.insert( "<D" , rhs.dirname().str() ) ;
		// TODO $? etc ?
	}
	else
	{
		G::Path lhs( rule.m_lhs ) ;
		std::string oods_s = G::Str::join(" ",oods) ;
		std::string oods_d ; for( const auto & o : oods ) oods_d.append(oods_d.empty()?0U:1U,' ').append(G::Path(o).simple()?std::string(1U,'.'):G::Path(o).dirname().str()) ;
		svars.insert( "*" , lhs.withoutExtension().str() ) ;
		svars.insert( "*B" , lhs.withoutExtension().basename() ) ;
		svars.insert( "*F" , lhs.withoutExtension().basename() ) ;
		svars.insert( "@" , lhs.str() ) ;
		svars.insert( "@B" , lhs.withoutExtension().basename() ) ;
		svars.insert( "@F" , lhs.basename() ) ;
		svars.insert( "?" , oods_s ) ;
		svars.insert( "?B" , oods_s ) ; // TODO check
		svars.insert( "?F" , oods_s ) ; // TODO check
		svars.insert( "?D" , oods_d ) ; // TODO check
		// TODO also $$@ and $**
	}

	Cwd cwd ;
	std::size_t action_i = 0U ;
	std::size_t action_n = rule.m_actions.size() ;
	for( const auto & action : rule.m_actions )
	{
		action_i++ ;

		// expand $* etc
		std::string cmd ;
		if( implicit )
		{
			cmd = m_expander.expand( action.line.s , m_vars ) ; // (ignore line.e)
			cmd = m_expander.expand( cmd , svars , {} , false ) ;
		}
		else
		{
			cmd = m_expander.expand( action.line.e , svars , {} , false ) ;
		}
		G_ASSERT( !cmd.empty() ) ;

		// heredoc
		std::unique_ptr<G::TempFile> hdf ;
		std::string heredoc_filename ;
		if( !action.heredoc_lines.empty() )
		{
			hdf = std::make_unique<G::TempFile>( "gnmake" , "rsp" ) ;
			heredoc_filename = hdf->path().str() ;
			for( const auto & hdl : action.heredoc_lines )
			{
				std::string line ;
				if( implicit )
				{
					line = m_expander.expand( hdl.s , m_vars ) ;
					line = m_expander.expand( line , svars , {} , false ) ;
				}
				else
				{
					line = m_expander.expand( hdl.e , svars , {} , false ) ;
				}
				(*hdf) << line << "\r\n" ;
			}
			hdf->close() ;
			if( m_config.keep || action.heredoc_type == HereDocType::keep )
				hdf->keep() ;

			G::Str::replaceAll( cmd , "<<" , heredoc_filename ) ;
		}

		// sanitise and parse the command
		bool ignore_rc = false ;
		bool silent = false ;
		{
			// check for leading dash etc
			for( auto p = cmd.begin() ; p != cmd.end() ; )
			{
				if( *p == ' ' || *p == '\t' )
					p = cmd.erase( p ) ;
				else if( *p == '-' )
					ignore_rc = true , p = cmd.erase( p ) ;
				else if( *p == '@' )
					silent = true , ++p ; // no erase
				else
					break ;
			}

			// deal with quotes -- we need to strip quotes if the command
			// is ["\dir\exe" foo], but [@ "dir\exe" foo] runs okay without
			// modification!
			//
			if( cmd.size() > 2U && cmd[0] == '"' )
			{
				auto qpos = cmd.find( '"' , 1U ) ;
				if( qpos != std::string::npos && cmd.find(' ') > qpos )
				{
					G_LOG( "Makefile::runActions: stripping quotes from [" << cmd << "]" ) ;
					cmd.erase( qpos , 1U ) ;
					cmd.erase( 0U , 1U ) ;
				}
			}
		}

		// run the command
		if( cmd.find_first_not_of(" \t") == std::string::npos )
		{
		}
		else if( m_config.dry_run )
		{
			std::cout << std::string(8U,' ') << cmd << "\n" ;
		}
		else
		{
			if( m_config.log_running )
			{
				std::ostringstream ss ;
				ss << "running [" << cmd << "] " << (trivial(cmd)?"(not) ":"") << "to make [" << rule.m_lhs << "]" ;
				if( action_n )
					ss << " (" << action_i << "/" << action_n << ")" ;
				G_LOG_S( "Makefile::runActions: " << ss.str() ) ;
			}

			if( !m_config.silent && !silent )
				std::cout << std::string(8U,' ') << cmd << "\n" ;
			if( !trivial(cmd) && !cwd.apply(cmd) )
			{
				std::cout.flush() ;
				std::cerr.flush() ;
				int rc = std::system( G::Str::trimmed(cmd," \t").c_str() ) ;
				if( rc != 0 && ignore_rc )
					G_LOG( "Makefile::runActions: exit code " + std::to_string(rc) << " ignored" ) ;
				else if( rc != 0 )
					throw MakefileImp::CommandError( rule , rc , action , cmd , G::Process::cwd() ) ;
				std::cout.flush() ;
				std::cerr.flush() ;
			}
		}
	}
	return true ;
}

bool Makefile::trivial( const std::string & cmd )
{
	return cmd == "@" || cmd == "@rem" ;
}

std::vector<Rule> Makefile::findRules( const std::string & key )
{
	std::vector<Rule> result ;
	for( auto rule_p = m_rules.begin() ; rule_p != m_rules.end() ; ++rule_p )
	{
		if( (*rule_p).m_lhs == key )
			result.push_back( *rule_p ) ;
	}
	return result ;
}

bool Makefile::hasRuleWithActions( const std::string & key )
{
	std::vector<Rule> result ;
	for( auto rule_p = m_rules.begin() ; rule_p != m_rules.end() ; ++rule_p )
	{
		if( (*rule_p).m_lhs == key && !(*rule_p).m_actions.empty() )
			return true ;
	}
	return false ;
}

Rule Makefile::makeRule( const std::string & target )
{
	G::Path path( target ) ;

	std::string ext = path.extension() ; // eg. "obj"
	std::string prefix = path.withoutExtension().str() ; // eg. "dir/foo"
	if( ext.empty() )
		return norule() ;

	// (implicit rules are already in SUFFIXES order)

	for( const auto & irule : Rules::m_implicit_rules )
	{
		if( ext == irule.m_rhs )
		{
			std::string src_path = prefix + "." + irule.m_lhs ; // eg. "dir/foo.c"
			if( G::File::exists(src_path) ||
				hasRuleWithActions(src_path) ) // not documented but needed for perl DynaLoader
			{
				G_DEBUG( "Makefile::makeRule: implicit rule [" << irule.m_lhs << "]->"
					<< "[" << irule.m_rhs << "] matches: building from [" << src_path << "]" ) ;
				return Rule{0U,target,{src_path},irule.m_actions,false,false} ;
			}
			else
			{
				G_DEBUG( "Makefile::makeRule: implicit rule [" << irule.m_lhs << "]->"
					<< "[" << irule.m_rhs << "] matches but no [" << src_path << "]" ) ;
			}
		}
	}
	return norule() ;
}

Makefile::Time Makefile::timestamp( const std::string & filename )
{
	G::File::Stat stat = G::File::stat( filename ) ;
	return stat.error ? Time::zero() : Time(stat.mtime_s,stat.mtime_us) ;
}

Rule Makefile::norule()
{
	return Rule{std::numeric_limits<unsigned int>::max(),{},{},{},false,false} ;
}

bool Makefile::valid( const Rule & rule )
{
	return rule.ln != std::numeric_limits<unsigned int>::max() ;
}

