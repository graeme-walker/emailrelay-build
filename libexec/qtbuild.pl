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
# usage: qtbuild.pl [{--x86|--x86-only}]
#
# Directories, relative to cwd:
#  source   - qt5/
#  build    - qt-build-<arch>/
#  install  - qt-install-<arch>/
#
# Download qt source with:
#    $ git clone https://code.qt.io/qt/qt5.git qt5
#    $ git -C qt5 checkout 5.15
#    $ cd qt5 && perl init-repository --module-subset=qtbase,qttranslations
#
# Runs all windows commands via "vcvarsall.bat" if "x86" is
# one of the requested architectures.
#

use strict ;
use Cwd ;
use FileHandle ;
use File::Find ;
use File::Basename ;
use File::Copy ;
use File::Glob ;

my @cfg_arch = ( "x64" ) ;
if( $ARGV[0] eq "--x86" ) { push @cfg_arch , "x86" }
if( $ARGV[0] eq "--x86-only" ) { @cfg_arch = ( "x86" ) }
my $cfg_type = "release" ; # or "debug" or "debug-and-release"
die if( scalar(@cfg_arch) == 0 ) ;
my $cfg_use_vcvars = $^O ne "linux" ;
my $msvc_dir = $cfg_use_vcvars ? find_msvc() : undef ;

for my $arch ( @cfg_arch )
{
	my $source_dir = "qt5" ;
	my $build_dir = "qt-build-${arch}-${cfg_type}" ;
	my $install_dir = "qt-install-$arch" ;
	$install_dir = Cwd::realpath(".")."/$install_dir" ;

	mkdir( $build_dir ) ;

	# (see qtbase/config_help.txt)
	my @configure_args = (
		"-opensource" , "-confirm-license" ,
		"-prefix" , $install_dir ,
		"-static" , ( $^O eq "linux" ? "" : "-static-runtime" ) ,
		"-${cfg_type}" ,
		"-platform" , ( $^O eq "linux" ? "linux-g++" : "win32-msvc" ) ,
		"-ltcg" , # link-time code generation (?)
		"-no-pch" ,
		"-optimize-size" , # ?
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
	push @configure_args , map {("-skip",$_)} skips() ;
	#push @configure_args , no_networking() ;
	#push @configure_args , no_images() ;
	#push @configure_args , extras() ;

	if( $^O eq "linux" )
	{
		run( {cd=>$build_dir} , "configure($arch)" ,
			"../$source_dir/configure" , @configure_args ) ;
	}
	else
	{
		run( {arch=>$arch,cd=>$build_dir} , "configure($arch)" ,
			"..\\qt-source\\configure.bat" , @configure_args ) ;
	}

	run( {arch=>$arch,cd=>$build_dir} , "make($arch)" , $^O eq "linux" ? qw(make -j 10) : qw(nmake) ) ;
	run( {arch=>$arch,cd=>$build_dir} , "make-install($arch)" , $^O eq "linux" ? qw(make install) : qw(nmake install) ) ;
}

## ==

sub run
{
	my $opt = {} ; $opt = shift if ref($_[0]) ;
	my ( $logname , @cmd ) = @_ ;

	if( $opt->{arch} && $cfg_use_vcvars )
	{
		die "cannot do spaces" if $opt->{cd} =~ m/\s/ ; # TODO check
		my $check_dir = Cwd::getcwd() ;
		print "$logname: running: cmd=[".join(" ",@cmd)."] arch=[$$opt{arch}] cwd=[".Cwd::getcwd()."]".($opt->{cd}?" cd=[$$opt{cd}]":"")."\n" ;
		my @argv = ( "$msvc_dir/auxiliary/build/vcvarsall" , $opt->{arch} , "&&" , "cd" , ($opt->{cd}?$opt->{cd}:".") , "&&" , @cmd ) ;
		my $rc = system( @argv ) ; # must use array form
		die if Cwd::getcwd() ne $check_dir ;
		print "$logname: ERROR: cmd=[".join(" ",@cmd)."] arch=[$$opt{arch}] cwd=[".Cwd::getcwd()."]".($opt->{cd}?" wd=[$$opt{cd}]":"")."\n" if $rc ;
		$rc == 0 or die ;
	}
	else
	{
		my $cmd = join( " " , @cmd ) ;
		my $old_dir ;
		if( $opt->{cd} )
		{
			$old_dir = Cwd::getcwd() ;
			chdir( $opt->{cd} ) or die "error: $logname: cannot cd to [$opt->{cd}]\n" ;
		}
		print "$logname: running: cmd=[$cmd] cwd=[".Cwd::getcwd()."]\n" ;
		my $rc = system( $cmd ) ;
		print "$logname: running: rc=[$rc]\n" ;
		$rc == 0 or die ;
		if( $old_dir )
		{
			chdir( $old_dir ) or die "error: $logname: cannot cd back to [$old_dir]\n" ;
		}
	}
}

sub find_msvc
{
	# try using cmake (if it's on the path)
	my $msvc_dir ;
	{
		# fall back to using cmake, assuming it is on the path
		my $tmp_dir = "qtbuild.tmp" ;
		mkdir $tmp_dir ;
		print {new FileHandle( "$tmp_dir/CMakeLists.txt" , "w" )} "project(tmp)\n" if ! -e "$tmp_dir/CMakeLists.txt" ;
		system( "cmake -S $tmp_dir -B $tmp_dir >NUL: 2>&1" ) ;
		my $fh = new FileHandle( "$tmp_dir/CMakeCache.txt" ) ;
		my $cache ; { local $/ = undef ; $cache = <$fh> }
		( $msvc_dir ) = ( $cache =~ m;CMAKE_LINKER:FILEPATH=(.*/VC)/;im ) ;
	}

	# .. or a file glob
	if( ! -d $msvc_dir )
	{
		my $pf_dir = $ENV{'ProgramFiles(x86)'} ;
		( $msvc_dir ) = glob( "$ENV{SystemDrive}:/$pf_dir/microsoft visual studio/*/*/vc" ) ; # eg. .../2019/community/vc
	}

	return $msvc_dir ;
}

sub touch
{
	my ( $file ) = @_ ;
	new FileHandle( $file , "w" ) or die ;
}

sub skips
{
	return qw(
		charts
		purchasing
		quickcontrols2
		script
		svg
		tools
		virtualkeyboard
		wayland
		webchannel
		webengine
		webview
	) ;
}

sub extras
{
	# in principle it should be possible to get "--no-feature-whatever"
	# options from the qconfig-gui tool, but it's only available
	# commercially :-< -- the https://qtlite.com/ web tool looks like an
	# alternative but it does not work well in practice
	#
	# the full (?) list of features can be obtained from
	# "cd <build> && <source>/configure[.bat] -list-features 2>&1"
	#
	# configure options can go into a config file "<build>/config.opt"
	# rather than on the command-line, with "configure -redo" to
	# re-process
	#
	return grep {!m/^#/} qw(
		-no-feature-accessibility
		-no-feature-etc
		-no-feature-etc
	) ;
}

