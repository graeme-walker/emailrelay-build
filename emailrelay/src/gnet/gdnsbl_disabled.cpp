//
// Copyright (C) 2001-2023 Graeme Walker <graeme_walker@users.sourceforge.net>
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
/// \file gdnsbl_disabled.cpp
///

#include "gdef.h"
#include "gdnsbl.h"
#include "gexception.h"

class GNet::DnsblImp
{
} ;

GNet::Dnsbl::Dnsbl( std::function<void(bool)> callback , ExceptionSink , G::string_view ) :
	m_callback(callback)
{
}

GNet::Dnsbl::~Dnsbl()
= default ;

void GNet::Dnsbl::start( const Address & )
{
	m_callback( /*allow=*/true ) ;
}

bool GNet::Dnsbl::busy() const
{
	return false ;
}

void GNet::Dnsbl::checkConfig( const std::string & config )
{
	if( !config.empty() )
		throw G::Exception( "dnsbl has been disabled in this build" ) ;
}