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
# winbuild.pl
#
# Parses automake files throughout the emailrelay source tree to generate
# windows-specific cmake files, then uses cmake to generate mbedtls and
# emailrelay makefiles, and finally uses "cmake --build" to build the mbedtls
# libraries and emailrelay executables.
#
# usage: winbuild.pl [--no-mbedtls] [--no-gui] [--static-gui] [<subtask> [<subtask> ...]]
#
# Spits out batch files (like "winbuild-whatever.bat") for doing sub-tasks,
# including "winbuild-install.bat". See "winbuild.bat".
#
# By default the emailrelay cmake files specify static linkage of the run-time
# library ("/MT"), with the exception of the emailrelay GUI which is built with
# "/MD". Use "--static-gui" to have the emailrelay GUI cmake file also specify
# "/MT".
#
# Requires "cmake" to be on the path or somewhere obvious (see find_cmake() in
# "winbuild.pm").
#
# Looks for mbedtls source code in a sibling or child directory or other likely
# places (see winbuild::find_mbedtls(), disabled with "--no-mbedtls").
#
# Looks for Qt libraries in various places (see winbuild::find_qt_x64(),
# disabled with "--no-gui"). For a fully static GUI build the Qt libraries
# will have to have been built from source (ie. "configure -static -static-runtime")
# and then "--static-gui" passed to this script. See also "qtbuild.pl".
#
# The "install" sub-task, which is not run by default, assembles binaries
# and their dependencies in a directory tree ready for zipping and
# distribution. Some dependencies are assembled by the Qt dependency
# tool, "windeployqt", although this only works for a non-static GUI build.
#
# On linux the "install_winxp" sub-task can be used after a mingw cross
# build to assemble an installation zip file.
#

use strict ;
use Cwd ;
use FileHandle ;
use File::Find ;
use File::Basename ;
use File::Copy ;
use File::Glob ;
use lib dirname($0) , dirname($0)."/libexec" ;
require "winbuild.pm" ;

# configuration
my %cfg_options = () ;
my %cfg_paths = () ;
my @cfg_run_parts = () ;
{
	my $fh = new FileHandle( "winbuild.cfg" ) ;
	while(<$fh>)
	{
		chomp( my $line = $_ ) ;
		$line =~ s/#.*// ;
		$line =~ s/\s*$// ;
		my ( $key , $value ) = ( $line =~ m/(\S+)\s+(.*)/ ) ;
		$cfg_options{x86} = 1 if( $line =~ m/^x86/ ) ;
		$cfg_options{debug} = 1 if( $line =~ m/^debug/ ) ;
		$cfg_options{no_mbedtls} = 1 if( $line =~ m/^no[_-]mbedtls/ ) ;
		$cfg_options{no_gui} = 1 if( $line =~ m/^no[_-]gui/ ) ;
		$cfg_options{static_gui} = 1 if( $line =~ m/^static[_-]gui/ ) ;
		$cfg_paths{cmake} = $value if $key eq "cmake" ;
		$cfg_paths{mbedtls} = $value if $key eq "mbedtls" ;
		$cfg_paths{qt_x86} = $value if $key eq "qt-x86" ;
		$cfg_paths{qt_x64} = $value if $key eq "qt-x64" ;
	}
}
for my $arg ( @ARGV )
{
	if( $arg eq "--x86" ) { $cfg_options{x86} = 2 }
	elsif( $arg eq "--debug" ) { $cfg_options{debug} = 2 }
	elsif( $arg eq "--no-mbedtls" ) { $cfg_options{no_mbedtls} = 2 }
	elsif( $arg eq "--no-gui" ) { $cfg_options{no_gui} = 2 }
	elsif( $arg eq "--static-gui" ) { $cfg_options{static_gui} = 2 }
	elsif( $arg eq "--verbose" ) { $cfg_options{verbose} = 2 }
	else { push @cfg_run_parts , $arg }
}
if( $cfg_options{no_gui} == 1 ) { print "winbuild: info: winbuild.cfg: gui build disabled\n" }
if( $cfg_options{static_gui} == 1 ) { print "winbuild: info: winbuild.cfg: gui static build\n" }
if( $cfg_options{no_mbedtls} == 1 ) { print "winbuild: info: winbuild.cfg: mbedtls disabled\n" }
if( $cfg_options{debug} == 1 ) { print "winbuild: info: winbuild.cfg: debug build enabled\n" }
if( ! -e "winbuild.cfg" && scalar(%cfg_options) != 0 )
{
	my $fh = new FileHandle( "winbuild.cfg" , "w" ) or die "winbuild: error: cannot create winbuild.cfg\n" ;
	map { print $fh $_ , "\n" } keys %cfg_options ;
	$fh->close() or die ;
}
my $cfg_with_gui = !exists $cfg_options{no_gui} ;
my $cfg_static_gui = exists $cfg_options{static_gui} ;
my $cfg_with_mbedtls = !exists $cfg_options{no_mbedtls} ;
my $cfg_verbose = exists $cfg_options{verbose} ;
my $cfg_opt_debug = exists $cfg_options{debug} ;
my $cfg_opt_x86 = exists $cfg_options{x86} ;
my $cfg_opt_x64 = 1 ;
my $cfg_path_cmake = $cfg_paths{cmake} ;
my $cfg_path_mbedtls = $cfg_paths{mbedtls} ;
my $cfg_path_qt_x64 = $cfg_paths{qt_x64} ;
my $cfg_path_qt_x86 = $cfg_paths{qt_x86} ;
my $cfg_add_runtime = 1 ;
my $cfg_add_gui_runtime = $cfg_with_gui && !$cfg_static_gui ; # windeployqt
die unless ($cfg_opt_x64 || $cfg_opt_x86) ;
if( scalar(@cfg_run_parts) == 0 )
{
	@cfg_run_parts = $^O eq "linux" ? qw( install_winxp ) : ( $cfg_with_mbedtls ?
		qw( batchfiles generate mbedtls cmake build ) :
		qw( batchfiles generate cmake build ) ) ;
}

# find stuff ...

if( !$cfg_path_cmake ) { $cfg_path_cmake = winbuild::find_cmake() }
if( !$cfg_path_mbedtls && $cfg_with_mbedtls ) { $cfg_path_mbedtls = winbuild::find_mbedtls() }
if( !$cfg_path_qt_x64 && $cfg_with_gui ) { $cfg_path_qt_x64 = winbuild::find_qt_x64() }
if( !$cfg_path_qt_x86 && $cfg_with_gui ) { $cfg_path_qt_x86 = winbuild::find_qt_x86() }

my $missing_cmake = !$cfg_path_cmake ;
my $missing_qt = ( $cfg_with_gui && $cfg_opt_x86 && !$cfg_path_qt_x86 ) || ( $cfg_with_gui && $cfg_opt_x64 && !$cfg_path_qt_x64 ) ;
my $missing_mbedtls = ( $cfg_with_mbedtls && !$cfg_path_mbedtls ) ;
warn "error: cannot find cmake.exe: please download from cmake.org\n" if $missing_cmake ;
warn "error: cannot find qt libraries: please download from wwww.qt.io or set no_gui in winbuild.cfg\n" if $missing_qt ;
warn "error: cannot find mbedtls source: please download from tls.mbed.org or set no_mbedtls in winbuild.cfg\n" if $missing_mbedtls ;
if( $missing_cmake || $missing_qt || $missing_mbedtls )
{
	warn "error: missing prerequisites: please install the missing components " ,
		"or " . (-f "winbuild.cfg"?"edit the":"use a") . " winbuild.cfg configuration file" , "\n" ;
	die "winbuild: error: missing prerequisites\n" ;
}

# qt info
my $qt_version = ( ( $cfg_opt_x86 && ($cfg_path_qt_x86 =~ m;qt6;i) ) || ( $cfg_opt_x64 && ($cfg_path_qt_x64 =~ m;qt6;i) ) ) ? 6 : 5 ;
my $qt_libs = {
	5 => "Qt5::Widgets Qt5::Gui Qt5::Core" ,
	6 => "Qt6::Widgets" ,
} ;
my $qt_info = {
	v => $qt_version ,
	x86 => $cfg_path_qt_x86 ,
	x64 => $cfg_path_qt_x64 ,
	libs => $qt_libs->{$qt_version} ,
} ;

# cmake command-line options
my $cmake_args = {
	x64 => [
		# try these in turn...
		[ "-G" , "Visual Studio 17 2022" , "-A" , "x64" ] ,
		[ "-G" , "Visual Studio 16 2019" , "-A" , "x64" ] ,
		[ "-A" , "x64" ] ,
	] ,
	x86 => [
		# try these in turn...
		[ "-G" , "Visual Studio 17 2022" , "-A" , "Win32" ] ,
		[ "-G" , "Visual Studio 16 2019" , "-A" , "Win32" ] ,
		[ "-A" , "Win32" ] ,
		[]
	] ,
} ;

# project version
chomp( my $version = eval { FileHandle->new("VERSION")->gets() } || "2.5.1" ) ;
my $project = "emailrelay" ;
my $install_x64 = "$project-$version-w64" ;
my $install_x86 = "$project-$version-w32" ;
my $install_winxp = "$project-$version-winxp" ;

# makefile conditionals
my %switches = (
	GCONFIG_BSD => 0 ,
	GCONFIG_DNSBL => 1 ,
	GCONFIG_EPOLL => 0 ,
	GCONFIG_GETTEXT => 0 ,
	GCONFIG_GUI => $cfg_with_gui ,
	GCONFIG_ICONV => 0 ,
	GCONFIG_INSTALL_HOOK => 0 ,
	GCONFIG_INTERFACE_NAMES => 1 ,
	GCONFIG_MAC => 0 ,
	GCONFIG_PAM => 0 ,
	GCONFIG_POP => 1 ,
	GCONFIG_ADMIN => 1 ,
	GCONFIG_TESTING => 1 ,
	GCONFIG_TLS_USE_MBEDTLS => ($cfg_with_mbedtls?1:0) ,
	GCONFIG_TLS_USE_OPENSSL => 0 ,
	GCONFIG_TLS_USE_BOTH => 0 ,
	GCONFIG_TLS_USE_NONE => ($cfg_with_mbedtls?0:1) ,
	GCONFIG_UDS => 0 ,
	GCONFIG_WINDOWS => 1 ,
) ;

# makefile expansion variables -- many are required but not relevant
my %vars = (
	top_srcdir => "." ,
	top_builddir => "." ,
	sbindir => "." ,
	mandir => "." ,
	localedir => "." ,
	e_spooldir => "c:/emailrelay" , # passed as -D but not used -- see src/gstore/gfilestore_win32.cpp
	e_docdir => "c:/emailrelay" ,
	e_initdir => "c:/emailrelay" ,
	e_bsdinitdir => "c:/emailrelay" ,
	e_rundir => "c:/emailrelay" ,
	e_icondir => "c:/emailrelay" ,
	e_trdir => "c:/emailrelay" ,
	e_examplesdir => "c:/emailrelay" ,
	e_libdir => "c:/emailrelay" ,
	e_pamdir => "c:/emailrelay" ,
	e_sysconfdir => "c:/emailrelay" ,
	GCONFIG_WINDRES => "windres" ,
	GCONFIG_WINDMC => "mc" ,
	GCONFIG_QT_LIBS => "" ,
	GCONFIG_QT_CFLAGS => "" ,
	GCONFIG_QT_MOC => "" ,
	GCONFIG_TLS_LIBS => "" ,
	GCONFIG_STATIC_START => "" ,
	GCONFIG_STATIC_END => "" ,
	VERSION => $version ,
	RPM_ARCH => "x86" ,
	RPM_ROOT => "rpm" ,
) ;

# run stuff ...

for my $part ( @cfg_run_parts )
{
	if( $part eq "batchfiles" )
	{
		winbuild::spit_out_batch_files( qw(
			find generate cmake build
			debug-build debug-test test
			mbedtls clean vclean install ) ) ;
	}
	elsif( $part eq "generate" )
	{
		run_generate( $project , \%switches , \%vars , $qt_info ) ;
	}
	elsif( $part eq "mbedtls" )
	{
		run_mbedtls_cmake( "x64" ) if $cfg_opt_x64 ;
		run_mbedtls_cmake( "x86" ) if $cfg_opt_x86 ;
		run_mbedtls_build( "x64" , "Debug" ) if ( $cfg_opt_x64 && $cfg_opt_debug ) ;
		run_mbedtls_build( "x64" , "Release" ) if $cfg_opt_x64 ;
		run_mbedtls_build( "x86" , "Debug" ) if ( $cfg_opt_x86 && $cfg_opt_debug ) ;
		run_mbedtls_build( "x86" , "Release" ) if $cfg_opt_x86 ;
	}
	elsif( $part eq "cmake" )
	{
		run_cmake( "x64" ) if $cfg_opt_x64 ;
		run_cmake( "x86" ) if $cfg_opt_x86 ;
	}
	elsif( $part eq "build" )
	{
		run_build( "x64" , "Release" ) if $cfg_opt_x64 ;
		run_build( "x64" , "Debug" ) if ( $cfg_opt_x64 && $cfg_opt_debug ) ;
		run_build( "x86" , "Release" ) if $cfg_opt_x86 ;
		run_build( "x86" , "Debug" ) if ( $cfg_opt_x86 && $cfg_opt_debug ) ;
		print_checksums( "x64" ) if $cfg_opt_x64 ;
		print_checksums( "x86" ) if $cfg_opt_x86 ;
	}
	elsif( $part eq "debug-build" )
	{
		run_build( "x64" , "Debug" ) ;
	}
	elsif( $part eq "clean" )
	{
		clean_test_files() ;
		run_build( "x64" , "Debug" , "clean" ) if ( $cfg_opt_x64 && $cfg_opt_debug ) ;
		run_build( "x64" , "Release" , "clean" ) if $cfg_opt_x64 ;
		run_build( "x86" , "Debug" , "clean" ) if ( $cfg_opt_x86 && $cfg_opt_debug ) ;
		run_build( "x86" , "Release" , "clean" ) if $cfg_opt_x86 ;
	}
	elsif( $part eq "vclean" )
	{
		clean_test_files() ;
		run_build( "x64" , "Debug" , "clean" ) if ( $cfg_opt_x64 && $cfg_opt_debug ) ;
		run_build( "x64" , "Release" , "clean" ) if $cfg_opt_x64 ;
		run_build( "x86" , "Debug" , "clean" ) if ( $cfg_opt_x86 && $cfg_opt_debug ) ;
		run_build( "x86" , "Release" , "clean" ) if $cfg_opt_x86 ;
		winbuild::clean_cmake_files() ;
		winbuild::clean_cmake_cache_files() ;
		winbuild::deltree( $install_x64 ) if $cfg_opt_x64 ;
		winbuild::deltree( $install_x86 ) if $cfg_opt_x86 ;
		winbuild::deltree( "$cfg_path_mbedtls/x64" ) if( $cfg_with_mbedtls && $cfg_opt_x64 ) ;
		winbuild::deltree( "$cfg_path_mbedtls/x86" ) if( $cfg_with_mbedtls && $cfg_opt_x86 ) ;
	}
	elsif( $part eq "install" )
	{
		install( $install_x64 , "x64" , $qt_info , $cfg_with_gui , $cfg_with_mbedtls ,
			$cfg_add_runtime , $cfg_add_gui_runtime ) if $cfg_opt_x64 ;

		install( $install_x86 , "x86" , $qt_info , $cfg_with_gui , $cfg_with_mbedtls ,
			$cfg_add_runtime , $cfg_add_gui_runtime ) if $cfg_opt_x86 ;
	}
	elsif( $part eq "install_winxp" )
	{
		run_install_winxp() ;
	}
	elsif( $part eq "debug-test" )
	{
		run_tests( "x64/src/main/Debug" , "x64/test/Debug" ) ;
	}
	elsif( $part eq "test" )
	{
		run_tests( "x64/src/main/Release" , "x64/test/Release" ) ;
	}
	else
	{
		die "winbuild: usage error\n" ;
	}
}

# signal success to the batch file if we have not died
winbuild::create_touchfile( winbuild::default_touchfile($0) ) ;

# show a helpful message
if( (grep {$_ eq "build"} @cfg_run_parts) && !(grep {$_ eq "install"} @cfg_run_parts) )
{
	print "winbuild: finished -- try winbuild-install.bat for packaging\n"
}

# ==

sub create_cmake_file
{
	my ( $project , $m , $switches , $qt_info ) = @_ ;

	my $path = join( "/" , dirname($m->path()) , "CMakeLists.txt" ) ;

	print "winbuild: cmake-file=[$path]\n" ;
	my $fh = new FileHandle( $path , "w" ) or die ;

	print $fh "# $path -- generated by $0\n" ;
	if( $project ) # top-level CMakeLists.txt file
	{
		print $fh "cmake_minimum_required(VERSION 3.1.0)\n" ;
		print $fh "project($project)\n" ;
		if( $cfg_with_gui && $qt_info->{v} == 5 )
		{
			# see https://doc.qt.io/qt-5/cmake-get-started.html
			print $fh "find_package(Qt5 COMPONENTS Widgets REQUIRED)\n" ;
		}
		elsif( $cfg_with_gui )
		{
			# see https://doc.qt.io/qt-6/cmake-get-started.html
			print $fh "find_package(Qt6 REQUIRED COMPONENTS Widgets)\n" ;
		}
		if( $cfg_with_mbedtls )
		{
			print $fh "find_package(MbedTLS REQUIRED)\n" ;
		}
		print $fh "find_program(CMAKE_MC_COMPILER mc)\n" ;
	}

	if( $m->path() =~ m/gui/ )
	{
		print $fh "set(CMAKE_AUTOMOC ON)\n" ;
		print $fh "set(CMAKE_INCLUDE_CURRENT_DIR ON)\n" ;
	}

	# force static or dynamic linking of the c++ runtime by switching
	# between /MD and /MT -- by default use static linking for the
	# main executablers but keep the gui dynamically linked so that
	# it can use a binary Qt distribution -- note that the gui build
	# is self-contained by virtue of "glibsources.cpp"
	#
	{
		my $dynamic_runtime = !$cfg_static_gui && ( $m->path() =~ m/gui/ ) ;
		print $fh '# choose dynamic or static linking of the c++ runtime' , "\n" ;
		print $fh 'set(CompilerFlags' , "\n" ;
		print $fh '    CMAKE_CXX_FLAGS' , "\n" ;
		print $fh '    CMAKE_CXX_FLAGS_DEBUG' , "\n" ;
		print $fh '    CMAKE_CXX_FLAGS_RELEASE' , "\n" ;
		print $fh '    CMAKE_C_FLAGS' , "\n" ;
		print $fh '    CMAKE_C_FLAGS_DEBUG' , "\n" ;
		print $fh '    CMAKE_C_FLAGS_RELEASE' , "\n" ;
		print $fh ')' , "\n" ;
		print $fh 'foreach(CompilerFlag ${CompilerFlags})' , "\n" ;
		if( $dynamic_runtime )
		{
			print $fh '    string(REPLACE "/MT" "/MD" ${CompilerFlag} "${${CompilerFlag}}")' , "\n" ;
		}
		else
		{
			print $fh '    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")' , "\n" ;
		}
		print $fh 'endforeach()' , "\n" ;
	}

	print $fh "\n" ;
	for my $subdir ( $m->subdirs() )
	{
		print $fh "add_subdirectory($subdir)\n" ;
	}

	my $definitions = join( " " , "G_WINDOWS=1" , grep {!m/G_LIB_SMALL/} $m->definitions() ) ;
	my $includes = join( " " , "." , ".." , $m->includes($m->base()) , '"${MBEDTLS_INCLUDE_DIRS}"' ) ;

	my @libraries = $m->libraries() ;
	for my $library ( @libraries )
	{
		my $sources = join( " " , $m->sources( $library ) ) ;
		if( $sources )
		{
			( my $library_key = $library ) =~ s/\.a$// ; $library_key =~ s/^lib// ;
			if( $library_key !~ m/extra$/ )
			{
				print $fh "\n" ;
				print $fh "add_library($library_key $sources)\n" ;
				print $fh "target_include_directories($library_key PUBLIC $includes)\n" ;
				print $fh "target_compile_definitions($library_key PUBLIC $definitions)\n" ;
			}
		}
	}

	my @programs = $m->programs() ;
	for my $program ( @programs )
	{
		my $sources = join( " " , $m->sources( $program ) ) ;
		my $our_libs = join( " " , $m->our_libs( $program ) ) ;
		my $sys_libs = join( " " , $m->sys_libs( $program ) ) ;

		my $tls_libs = "" ;
		if( ( $our_libs =~ m/gssl/ || $our_libs =~ m/keygen/ ) && $cfg_with_mbedtls )
		{
			$tls_libs =
				'optimized ${MBEDTLS_LIBRARY} debug ${MBEDTLS_LIBRARY_DEBUG} ' .
				'optimized ${MBEDX509_LIBRARY} debug ${MBEDX509_LIBRARY_DEBUG} ' .
				'optimized ${MBEDCRYPTO_LIBRARY} debug ${MBEDCRYPTO_LIBRARY_DEBUG}' ;
		}

		my $qt_libs = ( $m->path() =~ m/gui/ ) ? $qt_info->{libs} : "" ;
		my $win32 = ( $m->path() =~ m/gui/ || $program eq "emailrelay" ) ? "WIN32 " : "" ;

		my $resources = "" ;
		my $resource_includes = "" ;
		if( $program eq "emailrelay" )
		{
			$resources = "messages.mc emailrelay.rc emailrelay.exe.manifest" ;
			$resource_includes = "icon" ;
		}
		if( $program eq "emailrelay-service" )
		{
			$resources = "emailrelay-service.exe.manifest" ;
		}
		if( $program =~ m/emailrelay.gui/ )
		{
			$resources = "messages.mc emailrelay-gui.rc $program.exe.manifest" ;
			$resource_includes = "../main/icon" ;
		}

		my $program_sources = join(" ",split(' ',"$win32 $resources $sources")) ;
		my $program_includes = join(" ",split(' ',"$resource_includes $includes")) ;
		my $program_libs = join(" ",split(' ',"$our_libs $qt_libs $tls_libs $sys_libs")) ;

		( my $program_key = $program ) =~ s/\.real$// ;
		print $fh "\n" ;
		print $fh "add_executable($program_key $program_sources)\n" ;
		print $fh "target_include_directories($program_key PUBLIC $program_includes)\n" ;
		print $fh "target_compile_definitions($program_key PUBLIC $definitions)\n" ;
		print $fh "target_link_libraries($program_key $program_libs)\n" ;
		if( $resources =~ /messages.mc/ )
		{
			print $fh 'add_custom_command(TARGET '."$program_key".' PRE_BUILD COMMAND "${CMAKE_MC_COMPILER}" "${CMAKE_CURRENT_SOURCE_DIR}/messages.mc" VERBATIM)' , "\n" ;
		}
		if( $resources =~ m/manifest/ )
		{
			# the uac stanza is in manifest file, so stop the linker from adding it too
			print $fh "set_target_properties($program_key PROPERTIES LINK_FLAGS \"/MANIFESTUAC:NO\")\n" ;
		}
	}

	$fh->close() or die ;
}

sub create_cmake_files
{
	my ( $project , $switches , $vars , $qt_info ) = @_ ;

	my @makefiles = winbuild::read_makefiles( $switches , $vars ) ;
	my $first = 1 ;
	for my $m ( @makefiles )
	{
		create_cmake_file( $first?$project:undef , $m , $switches , $qt_info ) ;
		$first = 0 ;
	}
}

sub clean_test_files
{
	my @list = () ;
	File::Find::find( sub { push @list , $File::Find::name if $_ =~ m/^.tmp/ } , "." ) ;
	unlink grep { -f $_ } @list ;
	rmdir grep { -d $_ } @list ;
}

sub run_generate
{
	my ( $project , $switches , $vars , $qt_info ) = @_ ;
	create_cmake_files( $project , $switches , $vars , $qt_info ) ;
	create_gconfig_header() ;
}

sub run_cmake
{
	my ( $arch ) = @_ ;
	$arch ||= "x64" ;

	my @arch_args = @{$cmake_args->{$arch}} ;
	my $rc ;
	my $i = 0 ;
	for my $arch_args ( @arch_args )
	{
		#winbuild::clean_cmake_cache_files( $arch , {verbose=>0} ) ;

		my @args = @$arch_args ;
		if( $cfg_with_mbedtls )
		{
			my $mbedtls_realpath = Cwd::realpath($cfg_path_mbedtls) ; # must be full paths
			my $mbedtls_include_dir = "$mbedtls_realpath/include" ;
			my $mbedtls_lib_dir = "$mbedtls_realpath/$arch/library" ;
			unshift @args , "-DCMAKE_INCLUDE_PATH:FILEPATH=$mbedtls_include_dir" ;
			unshift @args , "-DMBEDTLS_LIBRARY:FILEPATH=$mbedtls_lib_dir/release/mbedtls.lib" ;
			unshift @args , "-DMBEDX509_LIBRARY:FILEPATH=$mbedtls_lib_dir/release/mbedx509.lib" ;
			unshift @args , "-DMBEDCRYPTO_LIBRARY:FILEPATH=$mbedtls_lib_dir/release/mbedcrypto.lib" ;
			unshift @args , "-DMBEDTLS_LIBRARY_DEBUG:FILEPATH=$mbedtls_lib_dir/debug/mbedtls.lib" ;
			unshift @args , "-DMBEDX509_LIBRARY_DEBUG:FILEPATH=$mbedtls_lib_dir/debug/mbedx509.lib" ;
			unshift @args , "-DMBEDCRYPTO_LIBRARY_DEBUG:FILEPATH=$mbedtls_lib_dir/debug/mbedcrypto.lib" ;
		}
		if( $cfg_with_gui )
		{
			my $qt_dir = defined($qt_info) ? Cwd::realpath( $qt_info->{$arch} ) : "." ;
			unshift @args , "-DCMAKE_PREFIX_PATH:FILEPATH=$qt_dir/lib/cmake" ; # so find_package() will find <qt>/lib/cmake/qt5/Qt5Config.cmake
			if( $qt_info->{v} == 5 )
			{
				unshift @args , "-DQt5_DIR:FILEPATH=$qt_dir" ;
			}
			else
			{
				unshift @args , "-DQt6_DIR:FILEPATH=$qt_dir" ;
				unshift @args , "-DQt6CoreTools_DIR:FILEPATH=".Cwd::realpath("$qt_dir/../qt6coretools") ;
				unshift @args , "-DQt6GuiTools_DIR:FILEPATH=".Cwd::realpath("$qt_dir/../qt6guitools") ;
			}
		}
		unshift @args , "-DCMAKE_MC_COMPILER:FILEPATH=mc" ;
		push @args , ( "-S" , "." ) ;
		push @args , ( "-B" , $arch ) ;

		print "winbuild: cmake($arch): running: [",join("][",$cfg_path_cmake,@args),"]\n" ;
		$rc = system( $cfg_path_cmake , @args ) ;
		last if $rc == 0 ;

		$i++ ;
		my $final = ( $i == scalar(@arch_args) ) ;
		print "winbuild: cmake($arch): cannot use that cmake generator: trying another\n" if !$final ;
	}
	print "winbuild: cmake-exit=[$rc]\n" ;
	die "winbuild: error: cmake failed: check error messages above and maybe tweak cmake_args in winbuild.pl\n" unless $rc == 0 ;
}

sub run_build
{
	my ( $arch , $confname , $target ) = @_ ;

	my @args = (
		"--build" , $arch , # build directory (cf. "-B")
		"--config" , $confname ) ;
	push @args , ( "--target" , $target ) if $target ;
	push @args , "-v" if $cfg_verbose ;

	print "winbuild: build($arch,$confname): running: [",join("][",$cfg_path_cmake,@args),"]\n" ;
	my $rc = system( $cfg_path_cmake , @args ) ;
	print "winbuild: build($arch,$confname): exit=[$rc]\n" ;
	die unless $rc == 0 ;
}

sub print_checksums
{
	my ( $arch , $confname ) = @_ ;
	$confname ||= "release" ;

	my @artifacts = (
		"$arch/src/main/$confname/emailrelay.exe" ,
		"$arch/src/main/$confname/emailrelay-textmode.exe" ,
		"$arch/src/main/$confname/emailrelay-submit.exe" ,
		"$arch/src/main/$confname/emailrelay-passwd.exe" ,
		"$arch/src/main/$confname/emailrelay-keygen.exe" ,
		"$arch/src/main/$confname/emailrelay-service.exe" ,
	) ;
	push @artifacts , "$arch/src/gui/$confname/emailrelay-gui.exe" if $cfg_with_gui ;

	for my $exe ( @artifacts )
	{
		print "winbuild: build($arch,$confname): " ,
			File::Basename::basename($exe) , " " ,
			winbuild::sha256($exe,"<error>") , "\n" if -f $exe ;
	}
}

sub create_gconfig_header
{
	winbuild::touch( "src/gconfig_defs.h" ) ;
}

sub run_mbedtls_cmake
{
	my ( $arch ) = @_ ;

	# run cmake to generate .sln file
	if( -f "$cfg_path_mbedtls/$arch/mbed tls.sln" )
	{
		print "winbuild: mbedtls-cmake($arch): already got [$arch/mbed tls.sln]: not running cmake\n" ;
	}
	else
	{
		my @runtime_args = () ;
		{
			# MSVC_STATIC_RUNTIME switch not in mbedtls 2.28.x
			my $x = "" ;
			my $fh = new FileHandle( "$cfg_path_mbedtls/$arch/library/CMakeLists.txt" ) ;
			{
				local $/ = undef ;
				$x = <$fh> ;
			}
			if( $x =~ m/MSVC_STATIC_RUNTIME/ )
			{
				@runtime_args = ( "-DMSVC_STATIC_RUNTIME=On" ) ;
			}
			else
			{
				# (use dashes here because of cmake bug)
				@runtime_args = (
					"-DCMAKE_C_FLAGS_DEBUG=-MTd -Ob0 -Od -RTC1" ,
					"-DCMAKE_C_FLAGS_RELEASE=-MT -O2 -Ob1 -DNDEBUG" ) ;
			}
		}

		my @arch_args = @{$cmake_args->{$arch}} ;
		my $rc ;
		for my $arch_args ( @arch_args )
		{
			my @args = (
				@$arch_args ,
				@runtime_args ,
				# (the ENABLE_...Off switches make the build a lot quicker but
				# also mean that the following mbedtls directories can be
				# deleted: docs doxygen visualc programs tests scripts)
				"-DENABLE_TESTING=Off" ,
				"-DENABLE_PROGRAMS=Off" ,
				"-B" , "$cfg_path_mbedtls/$arch" ,
				"-S" , $cfg_path_mbedtls ,
			) ;
			print "winbuild: mbedtls-cmake($arch): running: [",join("][",$cfg_path_cmake,@args),"]\n" ;
			$rc = system( $cfg_path_cmake , @args ) ;
			print "winbuild: mbedtls-cmake($arch): exit=[$rc]\n" ;
			last if( $rc == 0 ) ;
		}
		die unless $rc == 0 ;
	}
}

sub run_mbedtls_build
{
	my ( $arch , $confname ) = @_ ;

	my @args = (
		"--build" , "$cfg_path_mbedtls/$arch" ,
		"--target" , "mbedtls" ,
		"--target" , "mbedcrypto" ,
		"--target" , "mbedx509" ,
		"--config" , $confname ,
	) ;

	print "winbuild: mbedtls-build($arch,$confname): running: [",join("][",$cfg_path_cmake,@args),"]\n" ;
	my $rc = system( $cfg_path_cmake , @args ) ;
	print "winbuild: mbedtls-build($arch,$confname): exit=[$rc]\n" ;
	die unless $rc == 0 ;
}

sub install
{
	my ( $install , $arch , $qt_info , $with_gui , $with_mbedtls , $add_runtime , $add_gui_runtime ) = @_ ;

	my $msvc_base = winbuild::msvc_base( $arch ) ;
	$msvc_base or die "winbuild: error: install: cannot determine the msvc base directory\n" ;
	print "winbuild: msvc-base=[$msvc_base]\n" ;

	my $runtime = $add_runtime ? winbuild::find_runtime( $msvc_base , $arch , "vcruntime140.dll" , "msvcp140.dll" ) : undef ;
	if( $add_runtime && scalar(keys %$runtime) != 2 )
	{
		die "winbuild: error: install: cannot find msvc [$arch] runtime dlls under [$msvc_base]\n" ;
	}

	# copy the core files -- the main programs are always statically linked so they
	# can go into a "programs" sub-directory -- the gui/setup executable may be
	# dynamically linked in which case it should have run-time dlls copied
	# alongside
	#
	install_core( "$arch/src/main/Release" , $install ) ;

	if( $with_gui )
	{
		install_copy( "$arch/src/gui/Release/emailrelay-gui.exe" , "$install/emailrelay-setup.exe" ) ;

		install_copy( "$arch/src/main/Release/emailrelay-keygen.exe" , "$install/programs/" ) if $with_mbedtls ;

		install_payload_cfg( "$install/payload/payload.cfg" ) ;
		install_core( "$arch/src/main/Release" , "$install/payload/files" , 1 ) ;

		install_copy( "$arch/src/gui/Release/emailrelay-gui.exe" , "$install/payload/files/gui/" ) ;
		install_copy( "$arch/src/main/Release/emailrelay-keygen.exe" , "$install/payload/files/programs/" ) if $with_mbedtls ;

		# optionally use windeployqt to install compiler runtime DLLs, compiler runtime
		# installer, Qt library DLLs, Qt plugin DLLs, and Qt qtbase translations -- this
		# is only possible if the GUI is dynamically linked -- if statically linked
		# then the plugins are automatically linked in at build-time and windeployqt
		# fails -- unfortunately that means that qtbase translations then need to
		# be copied by hand
		#
		if( $add_gui_runtime ) # ie. windeployqt -- only works if dynamically linked
		{
			install_gui_dependencies( $msvc_base , $arch , $qt_info ,
				"$install/emailrelay-setup.exe" ,
				"$install/payload/files/gui/emailrelay-gui.exe" ) ;
		}

		winbuild::decode( "src/gui/emailrelay.no.qm_in" , "$install/translations/emailrelay.no.qm" ) ;
		winbuild::decode( "src/gui/emailrelay.no.qm_in" , "$install/payload/files/translations/emailrelay.no.qm" ) ;
	}

	if( $add_runtime )
	{
		install_runtime( $runtime , $arch , $install ) ;
		install_runtime( $runtime , $arch , "$install/payload/files/gui" ) if $with_gui ;
	}

	print "winbuild: $arch distribution in [$install]\n" ;
}

sub run_install_winxp
{
	install_core( "src/main" , $install_winxp , 0 , "." ) ;
	{
		my $fh = new FileHandle( "$install_winxp/emailrelay-start.bat" , "w" ) or die ;
		print $fh "start \"emailrelay\" emailrelay.exe \@app/config.txt\r\n" ;
		$fh->close() or die ;
	}
	{
		my $fh = new FileHandle( "$install_winxp/config.txt" , "w" ) or die ;
		print $fh "#\r\n# config.txt\r\n#\r\n\r\n" ;
		print $fh "# Use emailrelay-start.bat to run emailrelay with this config.\r\n" ;
		print $fh "# For demo purposes this only does forwarding once on startup.\r\n" ;
		print $fh "# Change 'forward' to 'forward-on-disconnect' once you\r\n" ;
		print $fh "# have set a valid 'forward-to' address.\r\n" ;
		for my $item ( qw(
			show=window,tray
			log
			verbose
			log-time
			log-file=@app/log-%d.txt
			syslog
			close-stderr
			spool-dir=@app
			port=25
			interface=0.0.0.0
			forward
			forward-to=127.0.0.1:25
			pop
			pop-port=110
			pop-auth=@app/popauth.txt
			) )
		{
			print $fh "$item\r\n" ;
		}
		print $fh "\r\n" ;
		$fh->close() or die ;
	}
	{
		my $fh = new FileHandle( "$install_winxp/popauth.txt" , "w" ) or die ;
		print $fh "#\r\n# popauth.txt\r\n#\r\n" ;
		print $fh "server plain postmaster postmaster\r\n" ;
		$fh->close() or die ;
	}
	{
		my $fh = new FileHandle( "$install_winxp/emailrelay-service.cfg" , "w" ) or die ;
		print $fh "dir-config=\"\@app\"\r\n" ;
		$fh->close() or die ;
	}
	{
		my $fh = new FileHandle( "$install_winxp/emailrelay-submit-test.bat" , "w" ) or die ;
		my $cmd = "\@echo off\r\n" ;
		$cmd .= "emailrelay-submit.exe -N -n -s \@app --from postmaster " ;
		$cmd .= "-C U3ViamVjdDogdGVzdA== " ; # subject
		$cmd .= "-C = " ;
		$cmd .= "-C VGVzdCBtZXNzYWdl " ; # body
		$cmd .= "-d -F -t " ;
		$cmd .= "postmaster" ; # to
		print $fh "$cmd\r\n" ;
		print $fh "pause\r\n" ;
		$fh->close() or die ;
	}
	system( "zip -q -r $install_winxp.zip $install_winxp" ) == 0 or die ;
	print "winbuild: winxp mingw distribution in [$install_winxp]\n" ;
}

sub install_runtime
{
	my ( $runtime , $arch , $dst_dir ) = @_ ;
	for my $key ( keys %$runtime )
	{
		my $path = $runtime->{$key}->{path} ;
		my $name = $runtime->{$key}->{name} ;
		my $src = $path ;
		my $dst = "$dst_dir/$name" ;
		if( ! -f $dst )
		{
			File::Copy::copy( $src , $dst ) or die "winbuild: error: install: failed to copy [$src] to [$dst]\n" ;
		}
	}
}

sub install_gui_dependencies
{
	my ( $msvc_base , $arch , $qt_info , @exes ) = @_ ;

	$ENV{VCINSTALLDIR} = $msvc_base ; # used by windeployqt to copy runtime files
	my $qt_bin = "$$qt_info{$arch}/bin" ;
	if( ! -d $qt_bin ) { die "winbuild: error: install: no qt bin directory\n" }
	my $tool = "$qt_bin/windeployqt.exe" ;
	if( ! -x $tool ) { die "winbuild: error: install: no windeployqt executable\n" }

	for my $exe ( @exes )
	{
		my $rc = system( $tool , $exe ) ;
		$rc == 0 or die "winbuild: error: install: failed running [$tool] [$exe]" ;
	}
}

sub install_payload_cfg
{
	my ( $file ) = @_ ;
	File::Path::make_path( File::Basename::dirname($file) ) ;
	my $fh = new FileHandle( $file , "w" ) or die "winbuild: error: install: cannot create [$file]\n" ;
	print $fh "files/programs/=\%dir-install\%/\n" ;
	print $fh "files/examples/=\%dir-install\%/examples/\n" ;
	print $fh "files/doc/=\%dir-install\%/doc/\n" ;
	print $fh "files/txt/=\%dir-install\%/\n" ;
	print $fh "files/gui/=\%dir-install\%/\n" ;
	$fh->close() or die ;
}

sub install_core
{
	my ( $src_main_bin_dir , $root , $is_payload , $programs ) = @_ ;

	$programs ||= "programs" ;
	my $txt = $is_payload ? "txt" : "." ;
	my %copy = qw(
		README __txt__/readme.txt
		COPYING __txt__/copying.txt
		AUTHORS __txt__/authors.txt
		LICENSE __txt__/license.txt
		NEWS __txt__/news.txt
		ChangeLog __txt__/changelog.txt
		doc/doxygen-missing.html doc/doxygen/index.html
		__src_main_bin_dir__/emailrelay-service.exe __programs__/
		__src_main_bin_dir__/emailrelay.exe __programs__/
		__src_main_bin_dir__/emailrelay-submit.exe __programs__/
		__src_main_bin_dir__/emailrelay-passwd.exe __programs__/
		__src_main_bin_dir__/emailrelay-textmode.exe __programs__/
		bin/emailrelay-service-install.js __programs__/
		bin/emailrelay-bcc-check.pl examples/
		bin/emailrelay-edit-content.js examples/
		bin/emailrelay-edit-envelope.js examples/
		bin/emailrelay-resubmit.js examples/
		bin/emailrelay-set-from.pl examples/
		bin/emailrelay-set-from.js examples/
		doc/authentication.png doc/
		doc/forwardto.png doc/
		doc/whatisit.png doc/
		doc/serverclient.png doc/
		doc/*.html doc/
		doc/developer.txt doc/
		doc/reference.txt doc/
		doc/userguide.txt doc/
		doc/windows.txt doc/
		doc/windows.txt __txt__/readme-windows.txt
		doc/doxygen-missing.html doc/doxygen/index.html
	) ;
	while( my ($src,$dst) = each %copy )
	{
		$dst = "" if $dst eq "." ;
		$src =~ s:__src_main_bin_dir__:$src_main_bin_dir:g ;
		$dst =~ s:__programs__:$programs:g ;
		$dst =~ s:__txt__:$txt:g ;
		map { install_copy( $_ , "$root/$dst" ) } File::Glob::bsd_glob( $src ) ;
	}
	# fix up inter-document references in readme.txt and license.txt
	winbuild::fixup( $root ,
		[ "$txt/readme.txt" , "$txt/license.txt" ] ,
		{
			README => 'readme.txt' ,
			COPYING => 'copying.txt' ,
			AUTHORS => 'authors.txt' ,
			INSTALL => 'install.txt' ,
			ChangeLog => 'changelog.txt' ,
		} ) ;
}

sub install_copy
{
	my ( $src , $dst ) = @_ ;
	winbuild::file_copy( $src , $dst ) ;
}

sub install_mkdir
{
	my ( $dir ) = @_ ;
	return if -d $dir ;
	mkdir( $dir ) or die "winbuild: error: install: failed to mkdir [$dir]\n" ;
}

sub run_tests
{
	my ( $main_bin_dir , $test_bin_dir ) = @_ ;
	my $dash_v = "" ; # or "-v"
	my $script = "test/emailrelay_test.pl" ;
	system( "perl -Itest \"$script\" $dash_v -d \"$main_bin_dir\" -x \"$test_bin_dir\" -c \"test/certificates\"" ) ;
}

sub run_find
{
	my $fh = new FileHandle( "winbuild.cfg" , "w" ) or die "winbuild: error: cannot create winbuild.cfg\n" ;
	print $fh "cmake " , $cfg_path_cmake , "\n" ;
	print $fh "mbedtls " , $cfg_path_mbedtls , "\n" ;
	print $fh "qt-x64 " , $cfg_path_qt_x64 , "\n" ;
	print $fh "qt-x86 " , $cfg_path_qt_x86 , "\n" ;
	$fh->close() or die ;
}

