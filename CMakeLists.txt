#####
#####  This file is part of vimix - video live mixer
#####  **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
#####

cmake_minimum_required(VERSION 3.8.2)
project(vimix VERSION 0.0.1 LANGUAGES CXX C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#####
##### Use git to read version from tags
#####
find_package (Git)
if(GIT_EXECUTABLE)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
    RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(NOT GIT_DESCRIBE_ERROR_CODE)
      string(SUBSTRING ${GIT_DESCRIBE_VERSION} 0 1 VIMIX_VERSION_MAJOR)
      string(SUBSTRING ${GIT_DESCRIBE_VERSION} 2 1 VIMIX_VERSION_MINOR)
      string(LENGTH ${GIT_DESCRIBE_VERSION} VIMIX_VERSION_LENGTH)
      if (VIMIX_VERSION_LENGTH GREATER 4)
          string(SUBSTRING ${GIT_DESCRIBE_VERSION} 4 1 VIMIX_VERSION_PATCH)
      else()
          set(VIMIX_VERSION_PATCH "0")
      endif()
      add_definitions(-DVIMIX_GIT="${GIT_DESCRIBE_VERSION}")
      add_definitions(-DVIMIX_VERSION_MAJOR=${VIMIX_VERSION_MAJOR})
      add_definitions(-DVIMIX_VERSION_MINOR=${VIMIX_VERSION_MINOR})
      add_definitions(-DVIMIX_VERSION_PATCH=${VIMIX_VERSION_PATCH})
      message(STATUS "Compiling vimix version ${VIMIX_VERSION_MAJOR}.${VIMIX_VERSION_MINOR}.${VIMIX_VERSION_PATCH} (${GIT_DESCRIBE_VERSION})")
  endif()
endif()


#####
##### Find the cmake modules
#####

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} /usr/share/cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules )
include(MacroLogFeature)
include(MacroFindGStreamerLibrary)


#####
##### Configure cmake
#####

set(BUILD_STATIC_LIBS ON)
set(CMAKE_INCLUDE_CURRENTDIR ON)

if(UNIX)
    if (APPLE)
        add_definitions(-DAPPLE)

        # the RPATH to be used when installing
        set(CMAKE_SKIP_RPATH TRUE)
        set(OpenGL_DIR /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/)

        # MacOS target
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "13" CACHE STRING "")

        # CPACK
        set(CPACK_SYSTEM_NAME  "OSX_${CMAKE_OSX_DEPLOYMENT_TARGET}_${CMAKE_OSX_ARCHITECTURES}")
        set(CPACK_GENERATOR DragNDrop)
        set(CPACK_BINARY_DRAGNDROP ON)

        # Apple ID code signing
        set(APPLE_CODESIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/osx/entitlements.plist")
        ## team ID, for example if your certificate is: Developer ID Application: JOHN, DOE (X4MF6H9XZ6) the team ID is: X4MF6H9XZ6
        set(APPLE_CODESIGN_TEAM "" CACHE STRING "")
        ## Apple identity, in the form Developer ID Application
        set(APPLE_CODESIGN_IDENTITY "Developer ID Application:" CACHE STRING "")
        ## Apple keychain profile for app-specific password
        ## 1. Create a password for "vimix" at  https://support.apple.com/en-us/102654
        ## 2. Run e.g.
        ## xcrun notarytool store-credentials "vimix" --apple-id <email_login_apple_id> --team-id <team> --password <string_given_https://appleid.apple.com/account/manage>
        set(APPLE_CODESIGN_STORED_CREDENTIAL "vimix" CACHE STRING "")

        # find icu4c in OSX (pretty well hidden...)
        set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/opt/icu4c/lib/pkgconfig:/opt/homebrew/opt/icu4c/lib/pkgconfig")

    else()
        add_definitions(-DLINUX)
        # add_definitions(-DUSE_GST_OPENGL_SYNC_HANDLER)
        # add_definitions(-DUSE_GL_BUFFER_SUBDATA)
        # add_definitions(-DIGNORE_GST_ERROR_MESSAGE)

        # CPACK
        set(CPACK_SYSTEM_NAME "${CMAKE_HOST_SYSTEM_NAME}")

        # linux opengl
        set(OpenGL_GL_PREFERENCE "GLVND")

        # linux dialogs use GTK
        find_package(GTK 3.0 REQUIRED)
        macro_log_feature(GTK_FOUND "GTK" "GTK cross-platform widget toolkit" "http://www.gtk.org" TRUE)

        find_package(X11 REQUIRED COMPONENTS xcb)
        include_directories(
            ${X11_INCLUDE_DIR}
        )

    endif()
    add_definitions(-DUNIX)
elseif(WIN32)
    add_definitions(-DWIN32)
    add_definitions(-DMINGW32)
endif()

#####
##### Dependencies
#####

#
# Basics
#
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

find_package(ZLIB REQUIRED)

#
# GSTREAMER
#
find_package(GLIB2 REQUIRED)
macro_log_feature(GLIB2_FOUND "GLib" "GTK general-purpose utility library" "http://www.gtk.org" TRUE)

find_package(GObject REQUIRED)
macro_log_feature(GOBJECT_FOUND "GObject" "GTK object-oriented framework" "http://www.gtk.org" TRUE)

find_package(GStreamer 1.0.0 COMPONENTS base)
macro_log_feature(GSTREAMER_FOUND "GStreamer"
"Open Source Multiplatform Multimedia Framework"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

find_package(GStreamerPluginsBase 1.0.0 COMPONENTS app audio video pbutils gl)
macro_log_feature(GSTREAMER_APP_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer app library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_AUDIO_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer audio library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_VIDEO_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer video library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_PBUTILS_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer pbutils library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_GL_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer opengl library"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

# Various preprocessor definitions for GST
add_definitions(-DGST_DISABLE_XML -DGST_DISABLE_LOADSAVE)

include_directories(
    ${GLIB2_INCLUDE_DIR}
    ${GSTREAMER_INCLUDE_DIR}
    ${GSTREAMER_AUDIO_INCLUDE_DIR}
    ${GSTREAMER_VIDEO_INCLUDE_DIR}
    ${GSTREAMER_BASE_INCLUDE_DIR}
    ${GSTREAMER_APP_INCLUDE_DIR}
    ${GSTREAMER_PBUTILS_INCLUDE_DIR}
    ${GSTREAMER_GL_INCLUDE_DIR}
)

#
# ICU4C
#
if (PKG_CONFIG_FOUND)
    pkg_check_modules(ICU REQUIRED icu-i18n icu-uc icu-io)
else ()
    find_package(ICU REQUIRED COMPONENTS i18n io uc)
endif ()

macro_log_feature(ICU_FOUND "ICU" "International Components for Unicode" "http://site.icu-project.org" TRUE)

include_directories(
    ${ICU_INCLUDE_DIRS}
)

link_directories(
    ${ICU_LIBRARY_DIRS}
)

#
# GLFW3
#
if (PKG_CONFIG_FOUND)
    pkg_check_modules(GLFW3 REQUIRED glfw3>=3.2)
else ()
    find_package(glfw3 3.2 REQUIRED)
endif()

macro_log_feature(GLFW3_FOUND "glfw3" "Open Source multi-platform library for OpenGL" "http://www.glfw.org" TRUE)

include_directories(
    ${GLFW3_INCLUDE_DIRS}
)

link_directories(
    ${GLFW3_LIBRARY_DIRS}
)

#
# GLM
#
if (PKG_CONFIG_FOUND)
    pkg_check_modules(GLM QUIET glm>=0.9.9)
else ()
    find_package(glm 0.9.9 QUIET)
endif()

if (GLM_FOUND)
    macro_log_feature(GLM_FOUND "GLM" "OpenGL mathematics" "https://glm.g-truc.net" TRUE)
else ()
    set(GLM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ext/glm)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/glm)
    message(STATUS "Compiling 'GLM' OpenGL mathematics https://glm.g-truc.net -- ${CMAKE_CURRENT_SOURCE_DIR}/ext/glm")
endif()

include_directories(
    ${GLM_INCLUDE_DIRS}
)

#
# TINY XML 2
#
if (PKG_CONFIG_FOUND)
    pkg_check_modules(TINYXML2 QUIET tinyxml2>=8.0)
else ()
    find_package(tinyxml2 8.0 QUIET)
endif()

if (TINYXML2_FOUND)
    macro_log_feature(TINYXML2_FOUND "TinyXML2" "TinyXML2 library" "https://github.com/leethomason/tinyxml2.git" TRUE)
else ()
    set(TINYXML2_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ext/tinyxml2)
    set(TINYXML2_LIBRARIES TINYXML2)
    add_library(TINYXML2 "${CMAKE_CURRENT_SOURCE_DIR}/ext/tinyxml2/tinyxml2.cpp")
    set_property(TARGET TINYXML2 PROPERTY CXX_STANDARD 17)
    set_property(TARGET TINYXML2 PROPERTY C_STANDARD 11)
    message(STATUS "Compiling 'TinyXML2' from https://github.com/leethomason/tinyxml2.git -- ${TINYXML2_INCLUDE_DIRS}")
endif()

include_directories(
    ${TINYXML2_INCLUDE_DIRS}
)

#
# STB
#
if (PKG_CONFIG_FOUND)
    pkg_check_modules(STB QUIET stb)
else ()
    find_package(stb QUIET)
endif()

if (STB_FOUND)
    macro_log_feature(STB_FOUND "STB" "single-file image and audio processing" "https://github.com/nothings/stb" TRUE)
else ()
    set(STB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ext/stb)
    message(STATUS "Including 'STB' single-file image and audio processing from https://github.com/nothings/stb -- ${STB_INCLUDE_DIRS}")
endif()

include_directories(
    ${STB_INCLUDE_DIRS}
)

#
# Ableton LINK
#
find_package(AbletonLink QUIET)

if (AbletonLink_FOUND)
    macro_log_feature(AbletonLink_FOUND "AbletonLink" "Ableton Link synchronizer" "https://github.com/Ableton/link" TRUE)
else ()
    include(${CMAKE_CURRENT_SOURCE_DIR}/ext/link/AbletonLinkConfig.cmake)
    message(STATUS "Compiling 'Ableton Link' from https://github.com/Ableton/link -- ${CMAKE_CURRENT_SOURCE_DIR}/ext/link")
endif()

include_directories(
    ${link_HEADERS}
)


#
# FILE DIALOG: use tinyfiledialog for all except Linux
#
if(APPLE OR WIN32)
    set(TINYFD_LIBRARIES TINYFD)
    set(TINYFD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/tfd)
    include_directories(
        ${TINYFD_INCLUDE_DIR}
    )
    add_library(TINYFD "${CMAKE_CURRENT_SOURCE_DIR}/ext/tfd/tinyfiledialogs.c")
    message(STATUS "Compiling 'TinyFileDialog' from https://git.code.sf.net/p/tinyfiledialogs/code -- ${TINYFD_INCLUDE_DIR}.")
endif()

#
# DIRENT (windows only)
if(WIN32)
    set(DIRENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/Dirent/include)
    include_directories(
        ${DIRENT_INCLUDE_DIR}
    )
    message(STATUS "Including 'Dirent' from https://github.com/tronkko/dirent -- ${DIRENT_INCLUDE_DIR}.")
endif()


#
# SHMDATA (Unix only)
#
if(UNIX)

    if (PKG_CONFIG_FOUND)
        pkg_check_modules(SHMDATA QUIET shmdata-1.3)
    endif()

    if(SHMDATA_FOUND)
        find_library(GSTREAMER_SHMDATA_LIBRARY
            NAMES gstshmdata
            HINTS ${SHMDATA_LIBRARY_DIRS}/gstreamer-1.0
        )
        if(GSTREAMER_SHMDATA_LIBRARY)
            add_definitions(-DGSTREAMER_SHMDATA_PLUGIN=\"${GSTREAMER_SHMDATA_LIBRARY}\")
        endif()

        macro_log_feature(GSTREAMER_SHMDATA_LIBRARY "GStreamerPluginShmdata" "Plugin to share any flow" "https://gitlab.com/sat-mtl/tools/shmdata" FALSE)
    endif(SHMDATA_FOUND)

endif(UNIX)


# show message about found libs
macro_display_feature_log()

#####
##### Locally built libraries
#####

#
# GLAD
#
set(GLAD_LIBRARIES GLAD)
set(GLAD_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ext/glad/include)
include_directories(
    ${GLAD_INCLUDE_DIRS}
)
add_library(GLAD "${CMAKE_CURRENT_SOURCE_DIR}/ext/glad/src/glad.c")
set_property(TARGET GLAD PROPERTY C_STANDARD 11)
message(STATUS "Compiling 'GLAD 2' Open source multi-language OpenGL loader https://gen.glad.sh -- ${GLAD_INCLUDE_DIRS}")

#
# DEAR IMGUI
#
set(IMGUI_LIBRARIES IMGUI IMGUITEXTEDIT)
set(IMGUI_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui)
set(IMGUI_BACKEND_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/examples)
include_directories(
    ${IMGUI_INCLUDE_DIRS}
    ${IMGUI_BACKEND_INCLUDE_DIRS}
)
set(IMGUI_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui//imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui//imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui//imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/examples/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/examples/imgui_impl_opengl3.cpp
    )
add_library(IMGUI "${IMGUI_SRCS}")
target_compile_definitions(IMGUI PRIVATE "IMGUI_IMPL_OPENGL_LOADER_GLAD")
target_compile_definitions(IMGUI PRIVATE "IMGUI_USE_STB_SPRINTF")
message(STATUS "Compiling 'Dear ImGui' from https://github.com/ocornut/imgui.git -- ${IMGUI_INCLUDE_DIRS}")

#
# ImGui Color Text Editor
#
set(IMGUITEXTEDIT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/ImGuiColorTextEdit)
include_directories(
    ${IMGUITEXTEDIT_INCLUDE_DIR}
)
add_library(IMGUITEXTEDIT "${CMAKE_CURRENT_SOURCE_DIR}/ext/ImGuiColorTextEdit/TextEditor.cpp")
set_property(TARGET IMGUITEXTEDIT PROPERTY CXX_STANDARD 17)
message(STATUS "Compiling 'ImGuiColorTextEdit' from https://github.com/BalazsJako/ImGuiColorTextEdit -- ${IMGUITEXTEDIT_INCLUDE_DIR}")


#
# OSCPack
#
if(UNIX)
    set(OSCPACK_PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/OSCPack/ip/posix/)
elseif(WIN32)
    set(OSCPACK_PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/OSCPack/ip/win32/)
endif()
set(OSCPACK_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/OSCPack/osc/OscTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/OSCPack/osc/OscReceivedElements.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/OSCPack/osc/OscPrintReceivedElements.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/OSCPack/osc/OscOutboundPacketStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/OSCPack/ip/IpEndpointName.cpp
    ${OSCPACK_PLATFORM_DIR}/NetworkingUtils.cpp
    ${OSCPACK_PLATFORM_DIR}/UdpSocket.cpp
)
set(OSCPACK_LIBRARIES OSCPACK)
set(OSCPACK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/OSCPack)
include_directories(
    ${OSCPACK_INCLUDE_DIR}
)
add_library(OSCPACK "${OSCPACK_SRCS}")
set_property(TARGET OSCPACK PROPERTY CXX_STANDARD 17)
set_property(TARGET OSCPACK PROPERTY C_STANDARD 11)
if(APPLE)
target_compile_definitions(OSCPACK
    PRIVATE
    _M_X64
)
endif(APPLE)
message(STATUS "Compiling 'OSCPack' from http://www.rossbencina.com/code/oscpack -- ${OSCPACK_INCLUDE_DIR}")


#####
##### Ressources
#####

set(RSC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rsc)

#
# Fonts
#

file(GLOB_RECURSE ROBOTO_REGULAR "${RSC_DIR}/*/Roboto-Regular.ttf")
if(NOT ROBOTO_REGULAR)
    file(GLOB_RECURSE ROBOTO_REGULAR "/usr/share/fonts/*/Roboto-Regular.ttf")
    message(STATUS "Copy ${ROBOTO_REGULAR} to ${RSC_DIR}/fonts")
    file(COPY ${ROBOTO_REGULAR} DESTINATION ${RSC_DIR}/fonts)
    set(ROBOTO_REGULAR "${RSC_DIR}/fonts/Roboto-Regular.ttf")
endif()

file(GLOB_RECURSE ROBOTO_BOLD "${RSC_DIR}/*/Roboto-Bold.ttf")
if(NOT ROBOTO_BOLD)
    file(GLOB_RECURSE ROBOTO_BOLD "/usr/share/fonts/*/Roboto-Bold.ttf")
    message(STATUS "Copy ${ROBOTO_BOLD} to ${RSC_DIR}/fonts")
    file(COPY ${ROBOTO_BOLD} DESTINATION ${RSC_DIR}/fonts)
    set(ROBOTO_BOLD "${RSC_DIR}/fonts/Roboto-Bold.ttf")
endif()

file(GLOB_RECURSE ROBOTO_ITALIC "${RSC_DIR}/*/Roboto-Italic.ttf")
if(NOT ROBOTO_ITALIC)
    file(GLOB_RECURSE ROBOTO_ITALIC "/usr/share/fonts/*/Roboto-Italic.ttf")
    message(STATUS "Copy ${ROBOTO_ITALIC} to ${RSC_DIR}/fonts")
    file(COPY ${ROBOTO_ITALIC} DESTINATION ${RSC_DIR}/fonts)
    set(ROBOTO_ITALIC "${RSC_DIR}/fonts/Roboto-Italic.ttf")
endif()

file(GLOB_RECURSE HACK_REGULAR "${RSC_DIR}/*/Hack-Regular.ttf")
if(NOT HACK_REGULAR)
    file(GLOB_RECURSE HACK_REGULAR "/usr/share/fonts/*/Hack-Regular.ttf")
    message(STATUS "Copy ${HACK_REGULAR} to ${RSC_DIR}/fonts")
    file(COPY ${HACK_REGULAR} DESTINATION ${RSC_DIR}/fonts)
    set(HACK_REGULAR "${RSC_DIR}/fonts/Hack-Regular.ttf")
endif()

file(GLOB_RECURSE AWESOME_SOLID "${RSC_DIR}/*/fa-solid-900.ttf")
if(NOT AWESOME_SOLID)
    file(GLOB_RECURSE AWESOME_SOLID "/usr/share/fonts/*/fa-solid-900.ttf")
    message(STATUS "Copy ${AWESOME_SOLID} to ${RSC_DIR}/fonts")
    file(COPY ${AWESOME_SOLID} DESTINATION ${RSC_DIR}/fonts)
    set(AWESOME_SOLID "${RSC_DIR}/fonts/fa-solid-900.ttf")
endif()

file(GLOB_RECURSE AWESOME_REGULAR "${RSC_DIR}/*/fa-regular-400.ttf")
if(NOT AWESOME_REGULAR)
    file(GLOB_RECURSE AWESOME_REGULAR "/usr/share/fonts/*/fa-regular-400.ttf")
    message(STATUS "Copy ${AWESOME_REGULAR} to ${RSC_DIR}/fonts")
    file(COPY ${AWESOME_REGULAR} DESTINATION ${RSC_DIR}/fonts)
    set(AWESOME_REGULAR "${RSC_DIR}/fonts/fa-regular-400.ttf")
endif()

include_directories(
    "${RSC_DIR}/fonts"
)


#
# CMake RC module
#

# Include the CMake RC module
include(CMakeRC)
message(STATUS "Using 'CMakeRC ' from https://github.com/vector-of-bool/cmrc.git -- ${CMAKE_MODULE_PATH}.")

# set the files to package
set(VMIX_RSC_FILES
    ${ROBOTO_REGULAR}
    ${ROBOTO_BOLD}
    ${ROBOTO_ITALIC}
    ${HACK_REGULAR}
    ${AWESOME_REGULAR}
    ${AWESOME_SOLID}
    ./rsc/shaders/simple.fs
    ./rsc/shaders/simple.vs
    ./rsc/shaders/texture.fs
    ./rsc/shaders/texture.vs
    ./rsc/shaders/image.fs
    ./rsc/shaders/mask_elipse.fs
    ./rsc/shaders/mask_box.fs
    ./rsc/shaders/mask_round.fs
    ./rsc/shaders/mask_horizontal.fs
    ./rsc/shaders/mask_vertical.fs
    ./rsc/shaders/mask_draw.fs
    ./rsc/shaders/image.vs
    ./rsc/shaders/imageprocessing.fs
    ./rsc/shaders/imageblending.fs
    ./rsc/images/mask_vignette.png
    ./rsc/images/mask_halo.png
    ./rsc/images/mask_glow.png
    ./rsc/images/mask_circle.png
    ./rsc/images/mask_roundcorner.png
    ./rsc/images/mask_linear_top.png
    ./rsc/images/mask_linear_bottom.png
    ./rsc/images/mask_linear_left.png
    ./rsc/images/mask_linear_right.png
    ./rsc/images/vimix_256x256.png
    ./rsc/images/vimix_crow_white.png
    ./rsc/images/icons.dds
    ./rsc/images/gradient_0_cross_linear.png
    ./rsc/images/gradient_1_black_linear.png
    ./rsc/images/gradient_2_cross_quad.png
    ./rsc/images/gradient_3_black_quad.png
    ./rsc/images/transparencygrid.png
    ./rsc/images/shadow.dds
    ./rsc/images/glow.dds
    ./rsc/images/checker.dds
    ./rsc/images/shadow_perspective.dds
    ./rsc/images/soft_shadow.dds
    ./rsc/mesh/disk.ply
    ./rsc/mesh/circle.ply
    ./rsc/mesh/corner.ply
    ./rsc/mesh/shadow.ply
    ./rsc/mesh/glow.ply
    ./rsc/mesh/border_round_left.ply
    ./rsc/mesh/border_round.ply
    ./rsc/mesh/border_top.ply
    ./rsc/mesh/border_perspective_round_left.ply
    ./rsc/mesh/border_perspective_round.ply
    ./rsc/mesh/border_perspective_top.ply
    ./rsc/mesh/border_sharp.ply
    ./rsc/mesh/border_large_round_left.ply
    ./rsc/mesh/border_large_round.ply
    ./rsc/mesh/border_large_top.ply
    ./rsc/mesh/border_handles_rotation.ply
    ./rsc/mesh/border_handles_scale.ply
    ./rsc/mesh/border_handles_overlay.ply
    ./rsc/mesh/border_handles_overlay_filled.ply
    ./rsc/mesh/border_handles_sharp.ply
    ./rsc/mesh/border_handles_menu.ply
    ./rsc/mesh/border_handles_crop.ply
    ./rsc/mesh/border_handles_shape.ply
    ./rsc/mesh/border_handles_lock.ply
    ./rsc/mesh/border_handles_lock_open.ply
    ./rsc/mesh/border_handles_shadow.ply
    ./rsc/mesh/border_handles_arrows.ply
    ./rsc/mesh/border_handles_roundcorner.ply
    ./rsc/mesh/border_large_sharp.ply
    ./rsc/mesh/border_vertical_overlay.ply
    ./rsc/mesh/perspective_layer.ply
    ./rsc/mesh/perspective_axis_left.ply
    ./rsc/mesh/perspective_axis_right.ply
    ./rsc/mesh/shadow_perspective.ply
    ./rsc/mesh/point.ply
    ./rsc/mesh/square_point.ply
    ./rsc/mesh/triangle_point.ply
    ./rsc/mesh/icon_video.ply
    ./rsc/mesh/icon_image.ply
    ./rsc/mesh/icon_render.ply
    ./rsc/mesh/icon_pattern.ply
    ./rsc/mesh/icon_gear.ply
    ./rsc/mesh/icon_camera.ply
    ./rsc/mesh/icon_share.ply
    ./rsc/mesh/icon_clone.ply
    ./rsc/mesh/icon_vimix.ply
    ./rsc/mesh/icon_group_vimix.ply
    ./rsc/mesh/icon_circles.ply
    ./rsc/mesh/icon_dots.ply
    ./rsc/mesh/icon_empty.ply
    ./rsc/mesh/icon_lock.ply
    ./rsc/mesh/icon_unlock.ply
    ./rsc/mesh/icon_circle.ply
    ./rsc/mesh/icon_square.ply
    ./rsc/mesh/icon_cross.ply
    ./rsc/mesh/icon_clock.ply
    ./rsc/mesh/icon_clock_hand.ply
    ./rsc/mesh/icon_grid.ply
    ./rsc/mesh/icon_rightarrow.ply
    ./rsc/mesh/icon_crop.ply
    ./rsc/mesh/icon_eye.ply
    ./rsc/mesh/icon_eye_slash.ply
    ./rsc/mesh/icon_tv.ply
    ./rsc/mesh/icon_cube.ply
    ./rsc/mesh/icon_sequence.ply
    ./rsc/mesh/icon_receive.ply
    ./rsc/mesh/icon_text.ply
    ./rsc/mesh/icon_play.ply
    ./rsc/mesh/icon_fastforward.ply
    ./rsc/mesh/h_line.ply
    ./rsc/mesh/h_mark.ply
    ./rsc/shaders/filters/default.glsl
    ./rsc/shaders/filters/blend.glsl
    ./rsc/shaders/filters/bloom.glsl
    ./rsc/shaders/filters/bokeh.glsl
    ./rsc/shaders/filters/talk.glsl
    ./rsc/shaders/filters/fisheye.glsl
    ./rsc/shaders/filters/blur.glsl
    ./rsc/shaders/filters/blur_1.glsl
    ./rsc/shaders/filters/blur_2.glsl
    ./rsc/shaders/filters/lens_1.glsl
    ./rsc/shaders/filters/lens_2.glsl
    ./rsc/shaders/filters/hashedblur.glsl
    ./rsc/shaders/filters/circularblur.glsl
    ./rsc/shaders/filters/hashederosion.glsl
    ./rsc/shaders/filters/hasheddilation.glsl
    ./rsc/shaders/filters/sharp.glsl
    ./rsc/shaders/filters/sharpen.glsl
    ./rsc/shaders/filters/sharpen_1.glsl
    ./rsc/shaders/filters/sharpen_2.glsl
    ./rsc/shaders/filters/contour_2.glsl
    ./rsc/shaders/filters/sharpenedge.glsl
    ./rsc/shaders/filters/chromakey.glsl
    ./rsc/shaders/filters/lumakey.glsl
    ./rsc/shaders/filters/coloralpha.glsl
    ./rsc/shaders/filters/bilinear.glsl
    ./rsc/shaders/filters/edge.glsl
    ./rsc/shaders/filters/sobel.glsl
    ./rsc/shaders/filters/freichen.glsl
    ./rsc/shaders/filters/kuwahara.glsl
    ./rsc/shaders/filters/pixelate.glsl
    ./rsc/shaders/filters/focus.glsl
    ./rsc/shaders/filters/denoise.glsl
    ./rsc/shaders/filters/noise.glsl
    ./rsc/shaders/filters/grain.glsl
    ./rsc/shaders/filters/stippling.glsl
    ./rsc/shaders/filters/dithering.glsl
    ./rsc/shaders/filters/erosion.glsl
    ./rsc/shaders/filters/dilation.glsl
    ./rsc/shaders/filters/tophat.glsl
    ./rsc/shaders/filters/blackhat.glsl
    ./rsc/shaders/filters/resample_double.glsl
    ./rsc/shaders/filters/resample_half.glsl
    ./rsc/shaders/filters/resample_quarter.glsl
    ./rsc/shaders/filters/earlybird.glsl
    ./rsc/shaders/filters/logo.glsl
    ./rsc/shaders/filters/whitebalance.glsl
    ./rsc/images/logo.vmx
)

cmrc_add_resource_library(vmix-resources ALIAS vmix::rc NAMESPACE vmix WHENCE rsc ${VMIX_RSC_FILES})


#####
##### DEFINE THE PACKAGING (all OS)
#####

SET(CPACK_PACKAGE_NAME "vimix")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "vimix\nReal-time video mixing for live performance.")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING.txt")
SET(CPACK_PACKAGE_CONTACT "bruno.herbelin@gmail.com")
SET(CPACK_PACKAGE_HOMEPAGE_URL "https://brunoherbelin.github.io/vimix")
SET(CPACK_PACKAGE_VERSION_MAJOR "${VIMIX_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${VIMIX_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${VIMIX_VERSION_PATCH}")
SET(CPACK_PACKAGE_VENDOR "Bruno Herbelin")
SET(CPACK_SOURCE_IGNORE_FILES
    "/\\\\.git/"
    "/\\\\.gitignore$"
    "/\\\\.gitmodules$"
)
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}_${CPACK_SYSTEM_NAME}")
set(CPACK_STRIP_FILES TRUE)


#####
##### Build Application
#####
add_subdirectory(src)


# To Create a package, run "cpack"
include(CPack)
