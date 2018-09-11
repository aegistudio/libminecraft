# @file cmake/CMakeDetermineGperfCompiler.cmake
# @author Haoran Luo
# This file is designed to find and configure gperf executable 
# for libminecraft. Commonly in libminecraft, the gperf compiler
# ought to compile a file with .gperf extension into .gperf.o.

# Attempt to find the gperf command.
enable_language(C CXX)
find_program(CMAKE_Gperf_COMPILER NAMES gperf PATHS
	"/usr/bin"
	"/usr/local/bin")
mark_as_advanced(CMAKE_Gperf_COMPILER)
if("${CMAKE_Gperf_COMPILER}" STREQUAL "")
message(SEND_ERROR "CMake_Gperf_COMPILER is neither found or set.")
else()
message("CMake_Gperf_COMPILER: ${CMAKE_Gperf_COMPILER}")
endif()

# Configure the compiler target.
configure_file(${CMAKE_CURRENT_LIST_DIR}/CMakeGperfCompiler.cmake.in
  ${CMAKE_PLATFORM_INFO_DIR}/CMakeGperfCompiler.cmake @ONLY)
set(CMAKE_Gperf_COMPILER_ENV_VAR "Gperf_COMPILER")