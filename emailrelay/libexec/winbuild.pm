#!/usr/bin/perl
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
# winbuild.pm
#
# Helper functions for winbuild.pl.
#
# Synopsis:
#
#  require "winbuild.pm" ;
#  winbuild::default_touchfile(...) ;
#  winbuild::find_cmake(...) ;
#  winbuild::find_qt_x86(...) ;
#  winbuild::find_qt_x64(...) ;
#  winbuild::find_mbedtls(...) ;
#  winbuild::find_runtime(...) ;
#  winbuild::spit_out_batch_files(...) ;
#  winbuild::clean_cmake_files(...) ;
#  winbuild::clean_cmake_cache_files(...) ;
#  winbuild::deltree(...) ;
#  winbuild::decode(...) ;
#  winbuild::create_touchfile(...) ;
#  winbuild::read_makefiles(...) ;
#  winbuild::read_makefiles_imp(...) ;
#  winbuild::cmake_cache_value(...) ;
#  winbuild::fixup(...) ;
#  winbuild::touch(...) ;
#  winbuild::file_copy(...) ;
#  winbuild::sha256(...) ;
#

use strict ;
use Cwd ;
use Carp ;
use FileHandle ;
use File::Basename ;
use File::Find ;
use File::Path ;
use File::Glob ;
use lib dirname($0) , dirname($0)."/bin" ;
use AutoMakeParser ;
$AutoMakeParser::debug = 0 ;

package winbuild ;

sub _find_cmake
{
	return (
		_find_basic( "find-cmake" , "cmake.exe" , _path_dirs() ) ||
		_find_match( "find-cmake" , "cmake*/bin/cmake.exe" , undef ,
			"$ENV{SystemDrive}" ,
			"$ENV{ProgramFiles}" ) ) ;
}

sub find_cmake
{
	return ( -x "/usr/bin/cmake" ? "/usr/bin/cmake" : undef ) if $^O eq "linux" ;
	return _find_cmake() ;
}

sub find_qt_x86
{
	my @dirs = (
		File::Basename::dirname($0)."/.." ,
		"$ENV{HOMEDRIVE}$ENV{HOMEPATH}/qt" ,
		"$ENV{SystemDrive}/qt" ,
	) ;
	return (
		_find_basic( "find-qt(x86)" , "qt-install-x86" , "." , ".." ) || # see qtbuild.pl
		_find_match( "find-qt(x86)" , "6*/msvc*/lib/cmake/qt6" , qr;/msvc\d\d\d\d/; , @dirs ) ||
		_find_match( "find-qt(x86)" , "5*/msvc*/lib/cmake/qt5" , qr;/msvc\d\d\d\d/; , @dirs ) ) ;
}

sub find_qt_x64
{
	my @dirs = (
		File::Basename::dirname($0)."/.." ,
		"$ENV{HOMEDRIVE}$ENV{HOMEPATH}/qt" ,
		"$ENV{SystemDrive}/qt" ,
	) ;
	return (
		_find_basic( "find-qt(x64)" , "qt-install-x64" , "." , ".." ) || # see qtbuild.pl
		_find_match( "find-qt(x64)" , "6*/msvc*_64/lib/cmake/qt6" , undef , @dirs ) ||
		_find_match( "find-qt(x64)" , "5*/msvc*_64/lib/cmake/qt5" , undef , @dirs ) ) ;
}

sub _find_mbedtls
{
	return (
		_find_match( "find-mbedtls" , "mbedtls*" , undef ,
			File::Basename::dirname($0) ,
			File::Basename::dirname($0)."/.." ,
			"$ENV{HOMEDRIVE}$ENV{HOMEPATH}" ,
			"$ENV{SystemDrive}" ) ) ;
}

sub find_mbedtls
{
	return ( -d "mbedtls" ? "mbedtls" : undef ) if $^O eq "linux" ;
	return _find_mbedtls() ;
}

sub find_runtime
{
	my ( $msvc_base , $arch , @names ) = @_ ;
	my $search_base = "$msvc_base/redist" ;
	my %runtime = () ;
	for my $name ( @names )
	{
		my @paths = grep { m:/$arch/:i } _find_all_under( $name , $search_base ) ;
		if( @paths )
		{
			# pick the shortest, as a heuristic
			my @p = sort { length($a) <=> length($b) } @paths ;
			my $path = $p[0] ;
			print "runtime: [$name] for [$arch] is [$path]\n" ;

			$runtime{$name} = { path => $path , name => $name } ;
		}
		else
		{
			print "runtime: [$name] not found under [$search_base]\n" ;
		}
	}
	return \%runtime ;
}

sub default_touchfile
{
	my ( $script ) = @_ ;
	$script =~ s/\.pl$// ;
	return "$script.ok" ;
}

sub _path_dirs
{
	my $path = $ENV{PATH} ;
	my $sep = ( $path =~ m/;/ ) ? ";" : ":" ;
	return split( $sep , $path ) ;
}

sub _sanepath
{
	my ( $path ) = @_ ;
	$path =~ s:\\:/:g ;
	return $path ;
}

sub _find_basic
{
	my ( $logname , $fname , @dirs ) = @_ ;
	my $result ;
	for my $dir ( map {_sanepath($_)} @dirs )
	{
		my $path = "$dir/$fname" ;
		if( -e $path ) { $result = Cwd::realpath($path) ; last }
		print "$logname: not $path\n" ;
	}
	print "$logname=[$result]\n" if $result ;
	return $result ;
}

sub _find_under
{
	my ( $logname , $fname , @dirs ) = @_ ;
	my $result ;
	for my $dir ( map {_sanepath($_)} @dirs )
	{
		next if !$dir ;
		my @find_list = () ;
		File::Find::find( sub { push @find_list , $File::Find::name if lc($_) eq $fname } , $dir ) ;
		if( @find_list ) { $result = Cwd::realpath($find_list[0]) ; last }
		print "$logname: not under $dir\n" ;
	}
	print "$logname=[$result]\n" if $result ;
	return $result ;
}

sub _find_all_under
{
	my ( $fname , $dir ) = @_ ;
	my @result = () ;
	File::Find::find( sub { push @result , $File::Find::name if lc($_) eq $fname } , $dir ) ;
	return @result ;
}

sub _find_match
{
	my ( $logname , $glob , $re , @dirs ) = @_ ;
	$re = qr;.; if !defined($re) ;
	my $result ;
	for my $dir ( map {_sanepath($_)} @dirs )
	{
		my @glob_match = () ;
		push @glob_match , grep { -e $_ && $_ =~ m/$re/ } File::Glob::bsd_glob( "$dir/$glob" ) ;
		if( @glob_match ) { $result = Cwd::realpath($glob_match[0]) ; last }
		print "$logname: no match for $dir/$glob\n" ;
	}
	print "$logname=[$result]\n" if $result ;
	return $result ;
}

sub spit_out_batch_files
{
	my ( @parts ) = @_ ;
	for my $part ( @parts )
	{
		my $fname = "winbuild-$part.bat" ;
		if( ! -f $fname )
		{
			my $fh = new FileHandle( $fname , "w" ) or next ;
			print $fh "runperl winbuild.pl winbuild.ok $part\n" ;
			$fh->close() ;
		}
	}
}

sub clean_cmake_files
{
	my ( $base_dir ) = @_ ;
	$base_dir ||= "." ;
	my @list = () ;
	File::Find::find( sub { push @list , $File::Find::name if $_ eq "CMakeLists.txt" } , $base_dir ) ;
	unlink grep {!m/mbedtls/i} grep {!m/qt/i} @list ;
}

sub clean_cmake_cache_files
{
	my ( $base_dir , $opt ) = @_ ;
	my $verbose = $opt->{verbose} || 0 ;
	$base_dir ||= "." ;
	my @tree_list = () ;
	my @file_list = () ;
	File::Find::find( sub { push @tree_list , $File::Find::name if $_ eq "CMakeFiles" } , $base_dir ) ;
	File::Find::find( sub { push @file_list , $File::Find::name if $_ eq "CMakeCache.txt" } , $base_dir ) ;
	map { print "cmake: cleaning [$base_dir/$_]\n" if $verbose } grep {!m/mbedtls/i} ( @tree_list , @file_list ) ;
	map { deltree($_) } grep {!m/mbedtls/i} @tree_list ;
	map { unlink $_ or die } grep {!m/mbedtls/i} @file_list ;
}

sub deltree
{
	my ( $dir ) = @_ ;
	my $e ;
	File::Path::remove_tree( $dir , {safe=>1,verbose=>0,error=>\$e} ) ;
	if( $e && scalar(@$e) )
	{
		for my $x ( @$e )
		{
			my ( $f , $m ) = ( %$x ) ;
			print "warning: " . ($f?"[$f]: ":"") . $m , "\n" ;
		}
	}
}

sub decode
{
	# re-creates a binary file (eg. ".qm") from an ascii-encoded file (eg. ".qm_in")
	my ( $path_in , $path_out ) = @_ ;
	my $dir = File::Basename::dirname($path_out) ;
	-d $dir or mkdir $dir or die "winbuild: error: decode: cannot create \".qm\" output directory [$dir]\n" ;
	my $fh_in = new FileHandle( $path_in ) or die "winbuild: error: decode: cannot open \".qm_in\" file [$path_in]\n" ;
	my @nn = () ; { local $/ = undef ; my $content = <$fh_in> ; @nn = split( " " , $content ) }
	die "winbuild: error: decode: no \".qm_in\" data\n" if scalar(@nn) == 0 ;
	open( my $fh_out , ">:raw" , $path_out ) ;
	$fh_out or die "winbuild: error: decode: cannot create \".qm\" file [$path_out]\n" ;
	for my $nn ( @nn )
	{
		if( $nn < 0 ) { $nn = $nn + 256 ; }
		die "winbuild: error: decode: invalid encoding ($nn)\n" if ( $nn < 0 || $nn > 255 ) ;
		printf $fh_out "%c" , $nn ;
	}
	close( $fh_out ) or die ;
}

sub create_touchfile
{
	my ( $touchfile ) = @_ ;
	my $fh = new FileHandle( $touchfile , "w" ) or die ;
	$fh->close() or die ;
}

sub read_makefiles
{
	my ( $switches , $vars ) = @_ ;
	my $verbose = 1 ;
	return AutoMakeParser::readall( "." , $switches , $vars , $verbose ) ;
}

sub cmake_cache_value
{
	my ( $arch , $re ) = @_ ;
	my $fh = new FileHandle( "$arch/CMakeCache.txt" , "r" ) or Carp::confess "error: cannot open cmake cache file [$arch/CMakeCache.txt] cwd=[".Cwd::getcwd()."]\n" ;
	my $value ;
	while(<$fh>)
	{
		chomp( my $line = $_ ) ;
		my ( $x ) = ( $line =~ m/$re/i ) ;
		if( $x )
		{
			$value = $x ;
			last ;
		}
	}
	return $value ;
}

sub msvc_base
{
	my ( $arch ) = @_ ;
	my $msvc_linker = undef ;
	if( defined($arch) )
	{
		$msvc_linker = _cmake_cache_value_msvc_linker( $arch ) ;
	}
	else
	{
		$msvc_linker = _cmake_cache_value_msvc_linker( "x64" ) ;
		$msvc_linker = _cmake_cache_value_msvc_linker( "x86" ) if( !defined($msvc_linker) ) ;
	}
	my $dir = File::Basename::dirname( $msvc_linker ) ;
	my ( $base ) = ( $dir =~ m:(.*/vc)/.*:i ) ; # could to better
	return Cwd::realpath( $base ) ;
}

sub _cmake_cache_value_msvc_linker
{
	my ( $arch ) = @_ ;
	my $msvc_linker = cmake_cache_value( $arch , qr/^CMAKE_LINKER:[A-Z]+=(.*)/ ) ;
	$msvc_linker or die "error: install: cannot read linker path from CMakeCache.txt\n" ;
	return $msvc_linker ;
}

sub fixup
{
	my ( $base , $fnames , $fixes ) = @_ ;
	for my $fname ( @$fnames )
	{
		my $fh_in = new FileHandle( "$base/$fname" , "r" ) or die "error install: cannot read [$base/$fname]\n" ;
		my $fh_out = new FileHandle( "$base/$fname.$$.tmp" , "w" ) or die ;
		while(<$fh_in>)
		{
			my $line = $_ ;
			for my $from ( keys %$fixes )
			{
				my $to = $fixes->{$from} ;
				$line =~ s/\Q$from\E/$to/g ;
			}
			print $fh_out $line ;
		}
		$fh_in->close() or die ;
		$fh_out->close() or die ;
		rename( "$base/$fname.$$.tmp" , "$base/$fname" ) or die ;
	}
}

sub touch
{
	my ( $path ) = @_ ;
	if( ! -f $path )
	{
		my $fh = new FileHandle( $path , "w" ) or die ;
		$fh->close() or die ;
	}
}

sub file_copy
{
	my ( $src , $dst ) = @_ ;

	if( $dst =~ m:/$: )
	{
		$dst =~ s:/$:: ;
		File::Path::make_path( $dst ) ;
		-d $dst or die "error: failed to create target directory [$dst]" ;
	}
	elsif( ! -d File::Basename::dirname($dst) )
	{
		File::Path::make_path( File::Basename::dirname($dst) ) ;
	}

	my $to_crlf = undef ;
	for my $ext ( "txt" , "js" , "pl" , "pm" )
	{
		if( -d $dst )
		{
			$to_crlf = 1 if ( $src =~ m/\.${ext}$/ ) ;
		}
		else
		{
			$to_crlf = 1 if( $dst =~ m/\.${ext}$/ ) ;
		}
	}

	if( $to_crlf )
	{
		if( -d $dst ) { $dst = "$dst/".File::Basename::basename($src) }
		my $fh_in = new FileHandle( $src , "r" ) ;
		my $fh_out = new FileHandle( $dst , "w" ) ;
		( $fh_in && $fh_out ) or die "error: failed to copy [$src] to [$dst]\n" ;
		while(<$fh_in>)
		{
			chomp( my $line = $_ ) ;
			print $fh_out $line , "\r\n" ;
		}
		$fh_out->close() or die "error: failed to copy [$src] to [$dst]\n" ;
	}
	else
	{
		File::Copy::copy( $src , $dst ) or die "error: failed to copy [$src] to [$dst]\n" ;
	}
}

sub sha256
{
	my ( $file , $default_ ) = @_ ;
	my $result = $default_ ;
	my $fh = new FileHandle( "certutil.exe -hashfile $file SHA256 |" ) ;
	while(<$fh>)
	{
		chomp( my $line = $_ ) ;
		if( $line =~ m/^[0-9a-f][0-9a-f] [0-9a-f][0-9a-f] /i )
		{
			$line =~ s/\s//g ;
			$result = $line ;
			last ;
		}
	}
	return $result ;
}

package main ;

1 ;