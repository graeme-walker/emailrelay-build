#
## Copyright (c) 2023 Graeme Walker <graeme_walker@users.sf.net>
## 
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
## 
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
## 
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#
# Makefile
#
# Unix makefile for gnmake.
#

#CXX = x86_64-w64-mingw32-g++-posix
#CXXFLAGS = -Wall -Wextra -Wpedantic -Wshadow -DG_WINDOWS=1 -I.
#LDFLAGS = -static
#OS = win32

CXX = g++
CXXFLAGS = -I. -g -O0 -Wall -Wextra -Wpedantic -Wshadow
OS = unix

gnmake_sources = \
	config.cpp \
	cwd.cpp \
	expander.cpp \
	expression.cpp \
	main.cpp \
	makefile.cpp \
	parser.cpp \
	rules.cpp \
	vars.cpp

lib_sources = \
	garg.cpp \
	gdatetime.cpp \
	genvironment_$(OS).cpp \
	genvironment.cpp \
	gexception.cpp \
	gfile_$(OS).cpp \
	gfile.cpp \
	gformat.cpp \
	ggettext_none.cpp \
	gidentity_$(OS).cpp \
	glog.cpp \
	glogoutput_$(OS).cpp \
	glogoutput.cpp \
	glogstream.cpp \
	gpath.cpp \
	gprocess_$(OS).cpp \
	groot.cpp \
	gstr.cpp \
	gstringlist.cpp \
	gstringview.cpp \
	gtest.cpp

lib_objects = $(lib_sources:.cpp=.o)
gnmake_objects = $(gnmake_sources:.cpp=.o)

gnmake: glib.a gconfig_defs.h $(gnmake_objects)
	$(CXX) $(LDFLAGS) -o $@ $(gnmake_objects) glib.a

glib.a: gconfig_defs.h $(lib_objects)
	$(AR) -cr $@ $(lib_objects)

gconfig_defs.h:
	touch $@

clean:
	-rm gnmake $(lib_objects) $(gnmake_objects) glib.a 2>/dev/null

