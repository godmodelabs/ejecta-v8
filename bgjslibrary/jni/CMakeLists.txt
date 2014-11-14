cmake_minimum_required (VERSION 2.6)
project (bgjsunix)

#v8 libraries
#add_library(v8_snapshot  SHARED IMPORTED )
# You can define two import-locations: one for debug and one for release.
#set_target_properties( v8_snapshot PROPERTIES IMPORTED_LOCATION (${PROJECT_SOURCE_DIR}/../libs/osx-x64/))

include_directories ("${PROJECT_SOURCE_DIR}/bgjs" "${PROJECT_SOURCE_DIR}/bgjs/modules")
include_directories ("${PROJECT_SOURCE_DIR}/include")
add_subdirectory (bgjs)


add_executable(bgjsunix test_unix.cpp)
target_link_libraries (bgjsunix ${PROJECT_SOURCE_DIR}/libs/osx-x64/libv8_base.a ${PROJECT_SOURCE_DIR}/libs/osx-x64/libv8_snapshot.a bgjs)
