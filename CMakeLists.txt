project(LibSplit)
cmake_minimum_required(VERSION 3.1)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DDEBUG=1)
endif()

set(CMAKE_C_STANDARD 99)

# Shared split library
add_library(split SHARED src/split.c)
set_target_properties(split PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# Static split library
add_library(split_static STATIC src/split.c)
set_target_properties(split_static PROPERTIES OUTPUT_NAME split)

# Hack as the PIC option for set_target_properies doesn't appear to work for CCE
if(CMAKE_C_COMPILER_ID MATCHES "Cray")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

# Serial application wrapper
add_executable(serial src/serial_wrapper.c)
target_link_libraries(serial split)

install(TARGETS split DESTINATION lib)
install(TARGETS split_static DESTINATION lib)
install(TARGETS serial DESTINATION bin)

file(GLOB script_files ${CMAKE_CURRENT_SOURCE_DIR}/bin/*)
install(FILES ${script_files} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
