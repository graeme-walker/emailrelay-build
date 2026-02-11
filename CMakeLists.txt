#
# CMakeLists.txt to run winbuildall.bat in a cmake development environment.
#
# Eg: 
#    cmake --fresh -A x64 -B x64 -S . 
#    cmake --build x64 --config release
#
# Eg: 
#    cmake --fresh -A Win32 -B x86-debug -S . 
#    cmake --build x86-debug --config debug
#
# Install as "CMakeLists.txt" alongside "winbuildall.bat" in the top-level 
# directory that also contains source trees for perl, emailrelay, openssl, 
# mbedtls, and qt.
#
cmake_minimum_required(VERSION 3.24)
project(winbuildall)
set(batchfile $<PATH:ABSOLUTE_PATH,NORMALIZE,winbuildall.bat,${CMAKE_SOURCE_DIR}>)
set(config $<LOWER_CASE:$<CONFIG>>)
set(arch $<PATH:GET_STEM,LAST_ONLY,${CMAKE_CURRENT_BINARY_DIR}>)
set(cmake ${CMAKE_COMMAND})
add_custom_target(winbuildall ALL COMMAND ${batchfile} ${config} ${arch} ${CMAKE_COMMAND})
