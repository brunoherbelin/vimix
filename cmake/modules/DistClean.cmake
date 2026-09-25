#####
#####  This file is part of vimix - video live mixer
#####  **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
#####
#####  Script run by the 'distclean' target: empties the build directory,
#####  cmake configuration included. Invoked with
#####      cmake -DBUILD_DIR=... -DSOURCE_DIR=... -P DistClean.cmake
#####

if(NOT DEFINED BUILD_DIR OR NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "distclean: BUILD_DIR and SOURCE_DIR shall be given.")
endif()

get_filename_component(_build  "${BUILD_DIR}"  REALPATH)
get_filename_component(_source "${SOURCE_DIR}" REALPATH)

# never erase the sources of an in-source build
if(_build STREQUAL _source)
    message(FATAL_ERROR "distclean: refusing to erase the in-source build in ${_build}.")
endif()

if(NOT EXISTS "${_build}/CMakeCache.txt")
    message(STATUS "distclean: ${_build} is not a cmake build directory, nothing done.")
    return()
endif()

# every entry of the build directory, hidden files included
file(GLOB _entries LIST_DIRECTORIES true "${_build}/*" "${_build}/.??*")
foreach(_entry IN LISTS _entries)
    file(REMOVE_RECURSE "${_entry}")
endforeach()

message(STATUS "distclean: removed all build files from ${_build}")
message(STATUS "distclean: run cmake again to configure a new build.")
