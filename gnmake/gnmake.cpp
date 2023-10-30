//
// gnmake
//

//
// This source file is for doing a 'unity build' of 'gnmake' by
// invoking the compiler once, straight from the command-line:
//
// unix:
//   g++ -o gnmake -pthread -g -O0 -Wall -Wextra -Wpedantic -Wshadow -D_DEBUG=1 -DG_UNIX=1 -Isrc gnmake.cpp
//
// windows:
//   cl /nologo /MTd /EHsc /Isrc /D_DEBUG=1 /c gnmake.cpp
//   link /nologo /SUBSYSTEM:CONSOLE /OUT:gnmake.exe gnmake.obj advapi32.lib kernel32.lib
//
// mingw for 64-bit windows:
//   x86_64-w64-mingw32-g++-posix -static -o gnmake.exe -Wall -Wextra -Wpedantic -Wshadow -DG_WINDOWS=1 -Isrc gnmake.cpp
//

#include "garg.cpp"
#include "gdatetime.cpp"
#include "genvironment.cpp"
#include "gexception.cpp"
#include "gfile.cpp"
#include "gformat.cpp"
#include "ggettext_none.cpp"
#include "glog.cpp"
#include "glogoutput.cpp"
#include "glogstream.cpp"
#include "gpath.cpp"
#include "groot.cpp"
#include "gstr.cpp"
#include "gstringlist.cpp"
#include "gstringview.cpp"
#include "gtempfile.cpp"
#include "gtest.cpp"

#include "config.cpp"
#include "cwd.cpp"
#include "expander.cpp"
#include "expression.cpp"
#include "main.cpp"
#include "makefile.cpp"
#include "parser.cpp"
#include "rules.cpp"
#include "vars.cpp"

#ifdef G_UNIX
#include "gcleanup_unix.cpp"
#include "genvironment_unix.cpp"
#include "gfile_unix.cpp"
#include "gidentity_unix.cpp"
#include "glogoutput_unix.cpp"
#include "gprocess_unix.cpp"
#include "gtempfile_unix.cpp"
#else
#include "gcleanup_win32.cpp"
#include "genvironment_win32.cpp"
#include "gfile_win32.cpp"
#include "gidentity_win32.cpp"
#include "glogoutput_win32.cpp"
#include "gprocess_win32.cpp"
#include "gtempfile_win32.cpp"
#endif

