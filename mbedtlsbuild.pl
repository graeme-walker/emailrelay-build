#!/usr/bin/env perl
#
# Copyright (C) 2001-2023 Graeme Walker <graeme_walker@users.sourceforge.net>
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# ===
#
# mbedtlsbuild.pl
#
# Does a static build of mbedtls on Windows using only cl and link.
#
# usage: mbedtlsbuild.pl [--config <config>] [<src-dir> [<build-dir>]]
#
# Eg:
#	mbedtlssbuild.pl --config release mbedtls mbedtls-x64
#
# Does something similar to:
#	mkdir <build>
#	cd <build>
#	cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -S .. -B .
#	nmake lib
#
# with "/MT" instead of /"MD".
#

use strict ;
use FileHandle ;
use Getopt::Long ;

my %opt = () ;
GetOptions( \%opt , "config=s" , "arch=s" , "quiet|q" ) or die "mbedtlsbuild.pl: usage error" ;

my $cfg_arch = $opt{arch} || current_arch() ;
my $cfg_config = $opt{config} || "release" ;
my $cfg_src_dir = $ARGV[0] || "mbedtls" ;
my $cfg_build_dir = $ARGV[1] || "mbedtls-${cfg_arch}" ;
my $cfg_quiet = $opt{quiet} ;
print "source: $cfg_src_dir\n" ;
print "build: $cfg_build_dir\n" ;

my $CFLAGS =
	( $cfg_config =~ m/debug/i ) ?
		"/DWIN32 /D_WINDOWS /D_DEBUG /W3 /WX /MTd /Zi /Ob0 /Od /Fd$cfg_build_dir/library/$cfg_config/mbedtls.pdb" :
		"/DWIN32 /D_WINDOWS /DNDEBUG /W2 /WX /MT /O2 /Ob2" ;

-f "$cfg_src_dir/library/aes.c" or die "error: mbedtlsbuild.pl: invalid source directory\n" ;
map { -d $_ || mkdir $_ or die } ( $cfg_build_dir , "$cfg_build_dir/library" , "$cfg_build_dir/library/$cfg_config" ) ;

sub read_objects
{
	my ( $key ) = @_ ;
	my $fh = new FileHandle( "$cfg_src_dir/library/Makefile" ) or die ;
	my $x ; { local $/ = undef ; $x = <$fh> ; }
	my ( $obj ) = ( $x =~ m/OBJS_${key}\s*=\s*([^#]*)/m ) ;
	return $obj
		=~ s/[\n\t\\]/ /rg
		=~ s/\s+/ /rg ;
}

sub objects
{
	my ( $lib ) = @_ ;
	return split( " " , read_objects(uc($lib)) ) ;
}

sub sources
{
	my ( $lib ) = @_ ;
	return map { $_ =~ s/\.o/.c/g ; $_ } split(" ",read_objects(uc($lib))) ;
}

for my $lib ( "crypto" , "x509" , "tls" )
{
	my @obj_paths = () ;
	for my $csrc ( sources( uc($lib) ) )
	{
		my $obj = $csrc =~ s/.c$/.obj/r ;
		push @obj_paths , "$cfg_build_dir/library/$cfg_config/$obj" ;
		my $cmd = "cl $CFLAGS -nologo " .
			"-I$cfg_src_dir/include " .
			"-I$cfg_src_dir/library " .
			"-Fo$cfg_build_dir/library/$cfg_config/$obj " .
			"/c $cfg_src_dir/library/$csrc" ;
		print "$cmd\n" unless $cfg_quiet ;
		system( $cmd ) ;
	}
	print "link /lib /OUT:$cfg_build_dir/library/$cfg_config/mbed$lib.lib\n" unless $cfg_quiet ;
	system( "link /lib /nologo /OUT:$cfg_build_dir/library/$cfg_config/mbed$lib.lib " . join(" ",@obj_paths) ) ;
}

sub current_arch
{
	if( $^O eq "linux" )
	{
		return "x64" ;
	}
	else
	{
		my $arch = $ENV{Platform} ;
		die if( $arch ne "x64" && $arch ne "x86" ) ;
		return $arch ;
	}
}

