project(LibSplit)
cmake_minimum_required(VERSION 3.1)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DDEBUG=1)
endif()

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

install(TARGETS split DESTINATION lib)
install(TARGETS split_static DESTINATION lib)
install(TARGETS serial DESTINATION bin)

file(GLOB script_files ${CMAKE_CURRENT_SOURCE_DIR}/bin/*)
install(FILES ${script_files}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ 
        WORLD_EXECUTE DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/python2.7/site-packages/wraprun"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ GROUP_WRITE WORLD_READ
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/python2.7/site-packages/wraprun")
