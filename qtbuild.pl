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
# qtbuild.pl
#
# Builds Qt static libraries.
#
# (The pre-built Qt libraries available to download are import libraries
# with Qt DLLs and they dynamically link to the c/c++ run-time. To build
# a GUI application that can be distributed without any DLLs a static
# build of Qt libraries is needed. Older versions of Qt were awkward to
# build statically because they did not have the "-static-runtime"
# option on the configure script, so they needed an edit of the
# "mkspecs\common\msvc-desktop.conf" file to add "/MT".)
#
# Runs the Qt 'configure' batch file with options chosen to limit the
# size of the build and then runs 'make' or 'nmake'.
#
# usage: qtbuild.pl [--config {debug|release}]
#
# Download qt source with:
#    $ git clone https://code.qt.io/qt/qt5.git qt5
#    $ git -C qt5 checkout 5.15
#    $ cd qt5 && perl init-repository --module-subset=qtbase
#
# Directories, relative to cwd:
#  source   - qt5/
#  build    - qt-build-<arch>/
#  install  - qt-<arch>/
#
# Delete qt-build-<arch> and qt-<arch> directories for a clean
# build ("rmdir /q /s" on windows).
#

use strict ;
use Cwd ;
use FileHandle ;
use File::Find ;
use File::Basename ;
use File::Copy ;
use File::Glob ;
use lib( File::Basename::dirname($0) ) ;

my $cfg_config = $ARGV[0] eq "--config" ? $ARGV[1] : "release" ;
die "usage error" if( $cfg_config ne "debug" && $cfg_config ne "release" ) ;
my $cfg_arch = "x64" ;
if( $^O ne "linux" )
{
	$cfg_arch = $ENV{Platform} ;
	die "error: not running in a windows vcvarsall session" if !$cfg_arch ;
}
my $cfg_static = 1 ;

my $source_dir = "qt5" ;
die "error: no $source_dir sub-directory here\n" if ! -d $source_dir ;

for my $arch ( $cfg_arch )
{
	my $build_dir = "qt-build-$arch" ;
	my $install_dir = "qt-$arch" ;
	$install_dir = Cwd::realpath(".")."/$install_dir" ;

	mkdir( $build_dir ) ;

	# configure
	#
	if( -e "$build_dir/config.status".($^O eq "linux"?"":".bat") )
	{
		print "qtbuild: info: already configured\n" ;
	}
	else
	{
		# set configure options -- see qtbase/config_help.txt --
		# these can go on the command-line, or into a config
		# file "<build>/config.opt" with "configure -redo" to
		# re-process
		my @configure_args = grep {m/./} (
			"-opensource" , "-confirm-license" ,
			"-prefix" , $install_dir ,
			( $cfg_static ? "-static" : "" ) ,
			( $cfg_static && $^O ne "linux" ? "-static-runtime" : "" ) ,
			"-${cfg_config}" ,
			"-platform" , ( $^O eq "linux" ? "linux-g++" : "win32-msvc" ) ,
			"-no-openssl" ,
			"-no-opengl" ,
			"-no-dbus" ,
			"-no-gif" ,
			"-no-libpng" ,
			"-no-libjpeg" ,
			#"-no-sqlite" ,
			"-nomake" , "examples" ,
			"-nomake" , "tests" ,
			"-nomake" , "tools" ,
			"-make" , "libs" ,
		) ;
		if( $cfg_config ne "debug" )
		{
			push @configure_args , (
				#"-ltcg" ,
				"-no-pch" ,
				"-optimize-size"
			) ;
		}
		push @configure_args , map {("-skip",$_)} skips() ;
		push @configure_args , no_features() ;

		# fix for missing qglobal.h error -- see "-e" test in "qtbase/configure"
		touch( "$source_dir/qtbase/.git" ) ;

		if( $^O eq "linux" )
		{
			run( $build_dir , "configure($arch)" ,
				"../$source_dir/configure" , @configure_args ) ;
		}
		else
		{
			run( $build_dir , "configure($arch)" ,
				"..\\\\$source_dir\\\\configure.bat" , @configure_args ) ;
		}
	}

	# build
	#
	run( $build_dir , "make($arch)" , $^O eq "linux" ? qw(make -j 10) : qw(nmake) ) ;

	# install
	#
	run( $build_dir , "make-install($arch)" , $^O eq "linux" ? qw(make install) : qw(nmake install) ) ;
}

## ==

sub run
{
	my ( $cd , $prefix , @cmd ) = @_ ;

	$prefix = "qtbuild: $prefix" ;
	my $old_dir ;
	if( $cd )
	{
		$old_dir = Cwd::getcwd() ;
		chdir( $cd ) or die "$prefix: error: cannot cd to [$cd]" ;
	}
	print "$prefix: running: cmd=[".join(" ",@cmd)."] cwd=[".Cwd::getcwd()."]\n" ;
	my $rc = system( @cmd ) ;
	print "$prefix: rc=[$rc]\n" ;
	if( $old_dir )
	{
		chdir( $old_dir ) or die "$prefix: error: cannot cd back to [$old_dir]" ;
	}
	die "$prefix: error: command failed\n" if $rc != 0 ;
}

sub touch
{
	my ( $path ) = @_ ;
	return 1 if -e $path ;
	my $fh = new FileHandle( $path , "w" ) ;
	return defined($fh) && $fh->close() ;
}

sub skips
{
	# skip any unwanted submodules cloned by init-repository
	# (see .git/.gitmodules without the "qt" prefixes) -- we might
	# want to clone qttools, qttranslations etc. for other reasons
	# but we do not want to build them with this script
	return grep {!m/^#/} qw(
		translations
		tools
	) ;
}

sub no_features
{
	# in principle it should be possible to get "--no-feature-whatever"
	# options from the qconfig-gui tool, but it's only available
	# commercially -- the https://qtlite.com/ web tool looks like an
	# alternative but it does not work well in practice
	#
	# the full list of features can be obtained from
	# "cd <build> && <source>/configure[.bat] -list-features 2>&1"
	#
	return grep {!m/^#/} qw(
		#-no-feature-accessibility
		#-no-feature-etc
		#-no-feature-etc
	) ;
}

