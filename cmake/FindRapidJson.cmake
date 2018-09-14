# @file cmake/FindRapidJson.cmake
# @author Haoran Luo
# This file is designed to find and configure Tencent's 
# rapidjson for libminecraft.
# After executing find_package(RapidJson REQUIRED), this
# module exports the following variables:
# RapidJson_FOUND - Whether RapidJson is found.
# RapidJson_INCLUDE_DIR - The root directory for RapidJson's
# headers. E.g. if the RapidJson's header is located at 
# /usr/include/rapidjson/rapidjson.hpp, then the directory
# will be /usr/include.

# Attempt to find potential include paths for rapid json.
find_path(RapidJson_INCLUDE_DIR
NAMES	"rapidjson/rapidjson.h"
PATHS	"/usr/include"
		"/usr/local/include"
		"$ENV{RapidJson_INCLUDE_DIR}")
mark_as_advanced(RapidJson_INCLUDE_DIR)

# Validate include path and set the found flags.
if(RapidJson_INCLUDE_DIR AND 
	EXISTS "${RapidJson_INCLUDE_DIR}/rapidjson/rapidjson.h")
set(RapidJson_FOUND 1)
endif()