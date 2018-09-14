cmake_minimum_required(VERSION 3.5)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# LibMinecraft is a minecraft protocol library based on C++.
project(libminecraft C CXX Gperf)

# Require ZLIB for compressed packet's compression and decompression.
find_package(ZLIB REQUIRED)
if(${ZLIB_FOUND})
message("ZLIB_INCLUDE_DIR: ${ZLIB_INCLUDE_DIR}")
message("ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")
else()
message(SEND_ERROR "ZLIB is required to be configured for libminecraft.")
endif()

# Require RapidJson for parsing chat or user provided nbt-json.
find_package(RapidJson REQUIRED)
if(${RapidJson_FOUND})
message("RapidJson_INCLUDE_DIR: ${RapidJson_INCLUDE_DIR}")
else()
message(SEND_ERROR "RapidJson is required to be configured for libminecraft.")
endif()

# Configure the include directories.
include_directories(include src ${ZLIB_INCLUDE_DIR} ${RapidJson_INCLUDE_DIR})

# Configure the library targets.
set(LIBMC_SRC src/connection.cpp src/writable.cpp src/stream.cpp 
		src/iobase.cpp src/nbt.cpp src/chat.cpp
		src/chattoken.gperf src/chatcolor.gperf src/keybind.gperf)
if(UNIX AND NOT APPLE)
list(APPEND LIBMC_SRC src/multiplexer_linux.cpp)
endif()
add_library(minecraft ${LIBMC_SRC})
target_link_libraries(minecraft ${ZLIB_LIBRARIES})