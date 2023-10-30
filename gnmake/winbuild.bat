cl /nologo /MTd /EHsc /Isrc\lib -Isrc\main /D_DEBUG=1 /c gnmake.cpp
link /nologo /SUBSYSTEM:CONSOLE /OUT:gnmake.exe gnmake.obj advapi32.lib kernel32.lib
