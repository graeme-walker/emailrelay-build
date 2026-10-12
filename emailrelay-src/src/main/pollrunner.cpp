//
// SPDX-FileCopyrightText: 2026 Graeme Walker <graeme_walker@users.sourceforge.net>
// SPDX-License-Identifier: GPL-3.0-or-later
//
///
/// \file pollrunner.cpp
///

#include "gdef.h"
#include "pollrunner.h"
#include "unit.h"
#include "gfile.h"
#include "gstr.h"
#include "gstringtoken.h"
#include "glog.h"
#include "glogoutput.h"
#include "ggettext.h"
#include <sstream>

Main::PollRunner::Spec Main::PollRunner::parse( const std::string & spec ,
	const G::Path & base_dir , const G::Path & app_dir ,
	G::StringArray * warnings_out )
{
	using G::txt ;
	if( spec.empty() )
	{
		return makeSpec( Spec::Type::none ) ;
	}
	else if( spec == "unfail:" )
	{
		std::string_view tail = G::Str::tailView( spec , ":" ) ;
		return makeSpec( Spec::Type::unfail , 0U , parseVerbose(tail) ) ;
	}
	else if( G::Str::headMatch( spec , "retry:") )
	{
		std::string_view tail = G::Str::tailView( spec , ":" ) ;
		return makeSpec( Spec::Type::retry , parseNumber(tail) , parseVerbose(tail) ) ;
	}
	else
	{
		auto result = parsePath( spec , base_dir , app_dir ) ;
		if( warnings_out && !G::File::exists(result.path,std::nothrow) )
			warnings_out->emplace_back( std::string(txt("poll-run program does not exist: ")).append(result.path.str()) ) ;
		return result ;
	}
}

Main::PollRunner::PollRunner( GNet::EventState es , Unit & unit , GStore::MessageStore & store ,
	const Spec & spec , const G::Path & spool_dir , const G::Path & delivery_dir ) :
		m_es(es) ,
		m_unit(unit) ,
		m_store(store) ,
		m_spec(spec) ,
		m_task(*this,es,"exec error: __strerror__",G::Root::nobody()) ,
		m_cmd(spec.path,{spool_dir.str(),delivery_dir.str()})
{
}

Main::PollRunner::~PollRunner()
= default ;

void Main::PollRunner::onTaskDone( int ec , const std::string & output )
{
	logEnd( ec , output ) ;
	m_unit.requestForwarding( "poll" ) ;
}

bool Main::PollRunner::busy() const
{
	return m_task.busy() ;
}

bool Main::PollRunner::start()
{
	if( m_spec.type == Spec::Type::cmd )
	{
		// unlike filters and address verifiers stdout is not needed for
		// anything functional so capture stdout and stderr for logging
		logStart() ;
		m_task.start( m_cmd , G::Environment::minimal() ,
			G::NewProcess::Fd::devnull() , // stdin
			G::NewProcess::Fd::pipe() , // stdout
			G::NewProcess::Fd::pipe() , // stderr
			G::Path() ) ;
		return false ; // async
	}
	else if( m_spec.type == Spec::Type::unfail )
	{
		G_LOG_S_IF( m_spec.verbose , "Main::PollRunner::onTaskDone: poll runner: unfailing" ) ;
		m_store.unfailAll() ;
		return true ;
	}
	else if( m_spec.type == Spec::Type::retry )
	{
		G_LOG_S_IF( m_spec.verbose , "Main::PollRunner::onTaskDone: poll runner: retrying" ) ;
		m_store.retry( m_spec.retry_limit , m_spec.verbose ) ;
		return true ;
	}
	else
	{
		return true ;
	}
}

Main::PollRunner::Spec Main::PollRunner::parsePath( const std::string & spec , const G::Path & base_dir , const G::Path & app_dir )
{
	if( G::Str::headMatch(spec,"run:") && !G::File::exists(fullPath(spec,base_dir,app_dir),std::nothrow) )
	{
		std::string_view tail = G::Str::tailView( spec , ":" ) ;
		G::StringTokenView t( tail , "," , 1U ) ;
		G::Path path = fullPath( t() , base_dir , app_dir ) ;
		bool verbose = t.next()() == "verbose" ;
		return makeSpec( Spec::Type::cmd , 0U , verbose , path ) ;
	}
	else
	{
		return makeSpec( Spec::Type::cmd , 0U , false , fullPath(spec,base_dir,app_dir) ) ;
	}
}

G::Path Main::PollRunner::fullPath( std::string_view path_in , const G::Path & base_dir , const G::Path & app_dir )
{
	std::string path = G::sv_to_string( path_in ) ;
	if( path.find("@app") == 0U && !app_dir.empty() )
	{
		G::Str::replace( path , "@app" , app_dir.str() ) ;
	}
	else if( G::Path(path).isRelative() && !base_dir.empty() )
	{
		path = (base_dir/path).str() ;
	}
	return {path} ;
}

unsigned int Main::PollRunner::parseNumber( std::string_view s )
{
	for( G::StringTokenView t(s,",",1U) ; t ; ++t )
	{
		if( G::Str::isUInt( t() ) )
			return G::Str::toUInt( t() , 0U ) ;
	}
	return 0U ;
}

bool Main::PollRunner::parseVerbose( std::string_view s )
{
	for( G::StringTokenView t(s,",",1U) ; t ; ++t )
	{
		if( t() == "verbose" )
			return true ;
	}
	return false ;
}

void Main::PollRunner::logStart()
{
	if( m_spec.verbose )
	{
		G_LOG_S( "Main::PollRunner::logStart: poll runner: [" << m_cmd.exe().basename() << "]: running " << m_cmd.exe() ) ;
	}
}

void Main::PollRunner::logEnd( int ec , const std::string & output )
{
	if( m_spec.verbose || ( ec != 0 && G::LogOutput::Instance::atVerbose() ) )
	{
		std::ostringstream ss ;
		ss << "[" << m_cmd.exe().basename() << "]: exit " << ec
			<< (output.empty()?"":" (") << G::Str::printable(output) << (output.empty()?"":")") ;
		if( m_spec.verbose )
			G_LOG_S( "Main::PollRunner::logEnd: poll runner: " << ss.str() ) ;
		else
			G_LOG( "Main::PollRunner::logEnd: poll runner: " << ss.str() ) ;
	}
}

