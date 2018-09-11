# @file cmake/CMakeGperfInformation.cmake
# @author Haoran Luo
# This file is designed to specify compiler flags for gperf compiler.
# Under Unix or Cygwin, the compile command will always be:
# <GPERF> <gperfFile> | <CC> -c -xc++ <INCLUDES> <FLAGS> -o <gperfObject> -

# Suppress some impossible behaviors.
set(CMAKE_Gperf_CREATE_SHARED_LIBRARY "Gperf_NO_CREATE_SHARED_LIBRARY")
set(CMAKE_Gperf_CREATE_SHARED_MODULE "Gperf_NO_CREATE_SHARED_MODULE")
set(CMAKE_Gperf_LINK_EXECUTABLE "Gperf_NO_LINK_EXECUTABLE")
set(CMAKE_INCLUDE_FLAG_Gperf "-I")
set(CMAKE_INCLUDE_FLAG_Gperf_SEP )

if("${CMAKE_Gperf_COMPILE_OBJECT}" STREQUAL "") 
set(CMAKE_Gperf_COMPILE_OBJECT 
"<CMAKE_Gperf_COMPILER> <SOURCE> | <CMAKE_CXX_COMPILER> -xc++ -c <DEFINES> <INCLUDES> ${CMAKE_CXX_FLAGS} -o <OBJECT> -")
endif()
set(CMAKE_Gperf_INFORMATION_LOADED 1)