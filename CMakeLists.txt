project(LibSplit)
cmake_minimum_required(VERSION 3.1)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DDEBUG=1)
endif()

set(CMAKE_C_STANDARD 99)

file(GLOB SRC_FILES src/*.c src/*.cc)

# Shared library
add_library(split SHARED ${SRC_FILES})
set_target_properties(split PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# Static library
add_library(split_static STATIC ${SRC_FILES})
set_target_properties(split_static PROPERTIES OUTPUT_NAME split)

# Hack as the PIC option for set_target_properies doesn't appear to work for CCE
if(CMAKE_C_COMPILER_ID MATCHES "Cray")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

install(TARGETS split DESTINATION lib)
install(TARGETS split_static DESTINATION lib)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bin DESTINATION ${CMAKE_INSTALL_PREFIX})

