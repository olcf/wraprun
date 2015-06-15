project(LibSplit)
cmake_minimum_required(VERSION 2.8.9)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DDEBUG=1)
endif()

file(GLOB SRC_FILES src/*.c src/*.cc)

add_library(split SHARED ${SRC_FILES})
set_target_properties(split PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
