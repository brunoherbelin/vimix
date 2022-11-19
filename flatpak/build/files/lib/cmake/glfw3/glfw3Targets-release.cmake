#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "glfw" for configuration "Release"
set_property(TARGET glfw APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(glfw PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libglfw.so.3.3"
  IMPORTED_SONAME_RELEASE "libglfw.so.3"
  )

list(APPEND _cmake_import_check_targets glfw )
list(APPEND _cmake_import_check_files_for_glfw "${_IMPORT_PREFIX}/lib/libglfw.so.3.3" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
