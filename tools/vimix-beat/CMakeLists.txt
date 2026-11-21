# vimix-beat : beat detector setting the Ableton Link tempo
#
# Engines:
#   - GStreamer 'bpmdetect' element (gst-plugins-bad, SoundTouch): always available
#   - aubio (https://aubio.org): optional, used by default if found
#
# Standalone build:
#    cmake -S tools/vimix-beat -B build-beat && cmake --build build-beat
# Or from vimix top-level CMake with  add_subdirectory(tools/vimix-beat)

cmake_minimum_required(VERSION 3.10)

if(NOT TARGET Ableton::Link)
    project(vimix-beat CXX)
    # use the Ableton Link sources bundled with vimix
    include(${CMAKE_CURRENT_LIST_DIR}/../../ext/link/AbletonLinkConfig.cmake)
endif()

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(BEAT_GST REQUIRED gstreamer-1.0>=1.24 gstreamer-app-1.0 glib-2.0)

option(BEAT_USE_AUBIO "Use aubio in vimix-beat when available" ON)
if(BEAT_USE_AUBIO)
    pkg_check_modules(BEAT_AUBIO QUIET aubio)
endif()

add_executable(vimix-beat main.cpp)
target_include_directories(vimix-beat PRIVATE ${BEAT_GST_INCLUDE_DIRS})
target_link_directories(vimix-beat PRIVATE ${BEAT_GST_LIBRARY_DIRS})
target_link_libraries(vimix-beat PRIVATE ${BEAT_GST_LIBRARIES} Ableton::Link pthread)

if(BEAT_AUBIO_FOUND)
    message(STATUS "vimix-beat: aubio ${BEAT_AUBIO_VERSION} found, tempo engine 'aubio' enabled")
    target_compile_definitions(vimix-beat PRIVATE HAVE_AUBIO)
    target_include_directories(vimix-beat PRIVATE ${BEAT_AUBIO_INCLUDE_DIRS})
    target_link_directories(vimix-beat PRIVATE ${BEAT_AUBIO_LIBRARY_DIRS})
    target_link_libraries(vimix-beat PRIVATE ${BEAT_AUBIO_LIBRARIES})
else()
    message(STATUS "vimix-beat: aubio not found, using GStreamer bpmdetect only (apt install libaubio-dev)")
endif()

install(TARGETS vimix-beat
        CONFIGURATIONS Release RelWithDebInfo
        RUNTIME DESTINATION bin COMPONENT Runtime)
