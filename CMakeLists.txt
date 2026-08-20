#####
#####  This file is part of vimix - video live mixer
#####  **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
#####

cmake_minimum_required(VERSION 3.10)
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
      message(STATUS "Compiling vimix \n   version ${VIMIX_VERSION_MAJOR}.${VIMIX_VERSION_MINOR}.${VIMIX_VERSION_PATCH} (${GIT_DESCRIBE_VERSION})\n")
  endif()
endif()

##### Build configuration info (compiler, platform, build type)
add_definitions(-DVIMIX_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
add_definitions(-DVIMIX_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}")
add_definitions(-DVIMIX_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}")
add_definitions(-DVIMIX_SYSTEM_PROCESSOR="${CMAKE_SYSTEM_PROCESSOR}")

if (NOT GIT_EXECUTABLE OR GIT_DESCRIBE_ERROR_CODE)
    #####
    ##### Fallback: Read version from snap/snapcraft.yaml if git is not available
    #####
    set(SNAPCRAFT_YAML "${CMAKE_CURRENT_SOURCE_DIR}/snap/snapcraft.yaml")
    if(EXISTS ${SNAPCRAFT_YAML})
    file(READ ${SNAPCRAFT_YAML} SNAPCRAFT_CONTENT)
    # Extract version line: "version: '0.8.5d'"
    string(REGEX MATCH "version: *'([0-9]+)\\.([0-9]+)\\.([0-9]+)?([a-z])?'" VERSION_MATCH ${SNAPCRAFT_CONTENT})
    if(VERSION_MATCH)
        set(VIMIX_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(VIMIX_VERSION_MINOR ${CMAKE_MATCH_2})
        set(VIMIX_VERSION_PATCH ${CMAKE_MATCH_3})
        add_definitions(-DVIMIX_VERSION_MAJOR=${VIMIX_VERSION_MAJOR})
        add_definitions(-DVIMIX_VERSION_MINOR=${VIMIX_VERSION_MINOR})
        add_definitions(-DVIMIX_VERSION_PATCH=${VIMIX_VERSION_PATCH})
        message(STATUS "Compiling vimix \n   version ${VIMIX_VERSION_MAJOR}.${VIMIX_VERSION_MINOR}.${VIMIX_VERSION_PATCH}\n")
    endif()
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

# NCNN and ONNX Runtime are optional 
option(USE_NCNN "Enable NCNN with Vulkan GPU backend" ON)
option(USE_ONNX "Enable ONNX Runtime backend" ON)

if(UNIX)
    if (APPLE)
        add_definitions(-DAPPLE)

        # macOS Bonjour/DNS-SD is always available (built-in SDK, no extra package)
        add_definitions(-DVIMIX_USE_BONJOUR)
        message(STATUS "mDNS/Bonjour: YES (macOS built-in)")

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

        # NCNN Vulkan GPU backend is not supported on macOS
        set(USE_NCNN OFF)

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
        if (CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR 
            (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64"))
            set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/homebrew/opt/icu4c/lib/pkgconfig")
        elseif (CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR 
                (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64"))
            set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/opt/icu4c/lib/pkgconfig")
        endif()

    else()
        add_definitions(-DLINUX)

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

        # XInput2 for tablet pressure support (optional)
        find_package(X11 COMPONENTS Xi)
        if(X11_Xi_FOUND)
            add_definitions(-DHAVE_X11TABLETINPUT)
            macro_log_feature(X11_Xi_FOUND "XInput2" "X11 Input extension for tablet support" "https://www.x.org/wiki/" FALSE)
        else()
       
            # alternatively libinput and libudev for tablet pressure support
            if (PKG_CONFIG_FOUND)
                pkg_check_modules(LIBINPUT libinput>=1.19)
                pkg_check_modules(LIBUDEV libudev)
            endif()

            if(LIBINPUT_FOUND AND LIBUDEV_FOUND)
                include_directories(${LIBINPUT_INCLUDE_DIRS} ${LIBUDEV_INCLUDE_DIRS})
                link_directories(${LIBINPUT_LIBRARY_DIRS} ${LIBUDEV_LIBRARY_DIRS})
                add_definitions(-DHAVE_LIBINPUT)
                macro_log_feature(LIBINPUT_FOUND "libinput" "Input device library for tablet support" "https://wayland.freedesktop.org/libinput" FALSE)
                macro_log_feature(LIBUDEV_FOUND "libudev" "Device management library" "https://www.freedesktop.org/software/systemd/man/libudev.html" FALSE)
            else()
                message(STATUS "XInput2 (Xi) or libinput or libudev not found - tablet pressure support will be disabled")
            endif()
        endif()

        # Linux: native Avahi client for mDNS peer discovery
        # (NOT avahi-compat-libdnssd — it has no .pc file and missing symbols)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(AVAHI avahi-client)
            if(AVAHI_FOUND)
                add_definitions(-DVIMIX_USE_AVAHI)
                message(STATUS "mDNS/Avahi: YES (libavahi-client)")
            else()
                message(STATUS "mDNS/Avahi: NO (install libavahi-client-dev for peer discovery)")
            endif()
        endif()

    endif()
    add_definitions(-DUNIX)
elseif(WIN32)
    add_definitions(-DWIN32)
    add_definitions(-DMINGW32)
endif()

#####
##### Preprocessor options
#####

option(USE_GST_OPENGL_SYNC_HANDLER "Enable GStreamer OpenGL sync handler" ON)
if(USE_GST_OPENGL_SYNC_HANDLER AND NOT APPLE)
    add_definitions(-DUSE_GST_OPENGL_SYNC_HANDLER)
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
if (PKG_CONFIG_FOUND)
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.24
        gstreamer-base-1.0
        gstreamer-pbutils-1.0
        gstreamer-app-1.0
        gstreamer-audio-1.0
        gstreamer-video-1.0
        gstreamer-gl-1.0
        gstreamer-transcoder-1.0
    )

    include_directories(
        ${GSTREAMER_INCLUDE_DIRS}
    )

    link_directories(
        ${GSTREAMER_LIBRARY_DIRS}
    )

else ()
    find_package(GLIB2 REQUIRED)
    macro_log_feature(GLIB2_FOUND "GLib" "GTK general-purpose utility library" "http://www.gtk.org" TRUE)

    find_package(GObject REQUIRED)
    macro_log_feature(GOBJECT_FOUND "GObject" "GTK object-oriented framework" "http://www.gtk.org" TRUE)

    find_package(GStreamer 1.0 COMPONENTS base REQUIRED)

    find_package(GStreamerPluginsBase 1.0.0 COMPONENTS app audio video pbutils gl transcoder)
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
    macro_log_feature(GSTREAMER_TRANSCODER_LIBRARY_FOUND "GStreamerPluginsBase" "GStreamer transcoder library"
    "http://gstreamer.freedesktop.org/" TRUE "1.0.0")

    include_directories(
        ${GLIB2_INCLUDE_DIR}
        ${GSTREAMER_INCLUDE_DIR}
        ${GSTREAMER_AUDIO_INCLUDE_DIR}
        ${GSTREAMER_VIDEO_INCLUDE_DIR}
        ${GSTREAMER_BASE_INCLUDE_DIR}
        ${GSTREAMER_APP_INCLUDE_DIR}
        ${GSTREAMER_PBUTILS_INCLUDE_DIR}
        ${GSTREAMER_GL_INCLUDE_DIR}
        ${GSTREAMER_TRANSCODER_INCLUDE_DIR}
    )
    set( GSTREAMER_LIBRARIES
        ${GOBJECT_LIBRARIES}
        ${GSTREAMER_LIBRARY}
        ${GSTREAMER_BASE_LIBRARY}
        ${GSTREAMER_APP_LIBRARY}
        ${GSTREAMER_AUDIO_LIBRARY}
        ${GSTREAMER_VIDEO_LIBRARY}
        ${GSTREAMER_PBUTILS_LIBRARY}
        ${GSTREAMER_GL_LIBRARY}
        ${GSTREAMER_TRANSCODER_LIBRARY}
    )

endif ()


# Various preprocessor definitions for GST
add_definitions(-DGST_DISABLE_XML -DGST_DISABLE_LOADSAVE)

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
    macro_log_feature(GLM_FOUND "GLM" "OpenGL mathematics" "https://glm.g-truc.net" FALSE)
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
    macro_log_feature(TINYXML2_FOUND "TinyXML2" "TinyXML2 library" "https://github.com/leethomason/tinyxml2.git" FALSE)
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
    macro_log_feature(STB_FOUND "STB" "single-file image and audio processing" "https://github.com/nothings/stb" FALSE)
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
    macro_log_feature(AbletonLink_FOUND "AbletonLink" "Ableton Link synchronizer" "https://github.com/Ableton/link" FALSE)
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
            add_definitions(-DGSTREAMER_SHMDATA_PLUGIN="${GSTREAMER_SHMDATA_LIBRARY}")
        endif()

        macro_log_feature(GSTREAMER_SHMDATA_LIBRARY "GStreamerPluginShmdata" "Plugin to share any flow" "https://gitlab.com/sat-mtl/tools/shmdata" FALSE)
    endif(SHMDATA_FOUND)

endif(UNIX)

# ONNX backend for Accelerated machine learning inference by Microsoft.
# Used by RIFE frame interpolator as CPU fallback.
# Search for system-installed package 
if(USE_ONNX)
  set(ORT_ROOT "" CACHE PATH "Extra prefix to search for ONNX Runtime")

  find_path(ORT_INCLUDE_DIR onnxruntime_cxx_api.h
    HINTS ${ORT_ROOT}/include
    PATH_SUFFIXES onnxruntime)
  find_library(ORT_LIBRARY onnxruntime HINTS ${ORT_ROOT}/lib)

  if(ORT_INCLUDE_DIR AND ORT_LIBRARY)
    add_library(onnxruntime::onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
      IMPORTED_LOCATION "${ORT_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${ORT_INCLUDE_DIR}")
  else()
    find_package(onnxruntime CONFIG QUIET HINTS ${ORT_ROOT})
    if(NOT onnxruntime_FOUND)
      set(USE_ONNX OFF)
    endif()
  endif()
    
  macro_log_feature(USE_ONNX "ONNX" "Backend for Accelerated machine learning inference" "https://onnxruntime.ai/" FALSE)

endif()

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
target_compile_options(IMGUI PRIVATE "-Wno-nontrivial-memcall")
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
# if(APPLE)
target_compile_definitions(OSCPACK
    PRIVATE
    _M_X64
)
# endif(APPLE)
message(STATUS "Compiling 'OSCPack' from http://www.rossbencina.com/code/oscpack -- ${OSCPACK_INCLUDE_DIR}")


#
# miniz - ZIP compression (optional, enabled when submodule is present)
# Uses miniz's own CMakeLists so generate_export_header() produces miniz_export.h
#
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/miniz/CMakeLists.txt")
    set(MINIZ_LIBRARIES miniz)
    # temporarily disable warning of CMake version 
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
    # BUILD_EXAMPLES/BUILD_TESTS/INSTALL_PROJECT default to OFF when miniz detects
    # it is included as a subdirectory (MINIZ_STANDALONE_PROJECT = OFF).
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/miniz)
    set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)
    message(STATUS "Compiling 'miniz' ZIP compression from https://github.com/richgel999/miniz")
else()
    set(MINIZ_LIBRARIES "")
    message(STATUS "miniz not found - ZIP compression disabled (run: git submodule update --init ext/miniz)")
endif()

#
# show synthetic table about found packages
#
macro_display_feature_log()

#
# NCNN Vulkan GPU backend. 
# Neural network inference framework, developed by Tencent, that utilizes 
# the Vulkan API for hardware-accelerated GPU computing.
# Used by RIFE frame interpolator and Real-ESRGAN upscaler. 
# The script setup-ext.sh runs automatically on first configure.
#
if(USE_NCNN)
  find_package(Vulkan QUIET)
  if(NOT Vulkan_FOUND)
    set(USE_NCNN OFF)
  endif()
endif()

if(USE_NCNN)
  set(RIFE_NCNN_DIR ${CMAKE_SOURCE_DIR}/ext/rife-ncnn-vulkan/src)
  set(RIFE_NCNN_INCLUDE_DIRS ${RIFE_NCNN_DIR} ${CMAKE_CURRENT_BINARY_DIR}) 
  set(NCNN_DIR ${CMAKE_SOURCE_DIR}/ext/ncnn)
  if(NOT EXISTS ${RIFE_NCNN_DIR}/rife.cpp
     OR NOT EXISTS ${NCNN_DIR}/CMakeLists.txt
     OR NOT EXISTS ${NCNN_DIR}/glslang/CMakeLists.txt)
    message(STATUS "Fetching extra sources (cmake/setup-ext.sh) ...")
    execute_process(
      COMMAND ${CMAKE_SOURCE_DIR}/cmake/setup-ext.sh
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      RESULT_VARIABLE EXT_SETUP_RESULT)
    if(NOT EXT_SETUP_RESULT EQUAL 0)
      message(WARNING "cmake/setup-ext.sh failed (git + network needed "
                      "on first configure) — building without RifeNCNN")
      set(USE_NCNN OFF)
    endif()
  endif()
endif()

if(USE_NCNN)
  find_package(Threads REQUIRED)
  message(STATUS "Compiling 'NCNN' Neural network inference for Vulkan GPU from https://github.com/Tencent/ncnn -- ${NCNN_DIR}")

  # ncnn build configuration (mirrors rife-ncnn-vulkan/src/CMakeLists.txt).
  # NCNN_VULKAN_ONLINE_SPIRV: shaders are embedded as source and compiled
  # at runtime, so no glslangValidator is needed at build time.
  set(NCNN_VULKAN ON CACHE BOOL "" FORCE)
  set(NCNN_VULKAN_ONLINE_SPIRV ON CACHE BOOL "" FORCE)
  # ncnn defaults to -fno-exceptions/-fno-rtti and propagates them to
  # consumers; our code and the ONNX Runtime C++ API rely on exceptions
  set(NCNN_DISABLE_EXCEPTION OFF CACHE BOOL "" FORCE)
  set(NCNN_DISABLE_RTTI OFF CACHE BOOL "" FORCE)
  set(NCNN_INSTALL_SDK OFF CACHE BOOL "" FORCE)
  set(NCNN_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
  set(NCNN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(NCNN_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(NCNN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(NCNN_PIXEL_ROTATE OFF CACHE BOOL "" FORCE)
  set(NCNN_PIXEL_AFFINE OFF CACHE BOOL "" FORCE)
  set(NCNN_PIXEL_DRAWING OFF CACHE BOOL "" FORCE)
  set(NCNN_INT8 OFF CACHE BOOL "" FORCE)

  # Build only the ncnn layers needed by RIFE models 
  file(STRINGS ${RIFE_NCNN_DIR}/CMakeLists.txt _rife_layer_opts
       REGEX "option\\(WITH_LAYER_")
  foreach(_line IN LISTS _rife_layer_opts)
    if(_line MATCHES "WITH_LAYER_([a-z0-9_]+) \"\" (ON|OFF)")
      set(WITH_LAYER_${CMAKE_MATCH_1} ${CMAKE_MATCH_2} CACHE BOOL "" FORCE)
    endif()
  endforeach()

  # The pinned ncnn/glslang declare cmake_minimum_required versions older than the one we use, so silence the warning.
  if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
  endif()
  set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
  add_subdirectory(${NCNN_DIR} ncnn EXCLUDE_FROM_ALL)
  set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)
  unset(CMAKE_POLICY_VERSION_MINIMUM)

  # ncnn is third-party code; silence its (harmless) unused-variable warnings
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ncnn PRIVATE -Wno-unused-variable)
  endif()

  # Compute shaders -> C headers (.comp.hex.h), same mechanism as upstream.
  macro(rife_add_shader SHADER_SRC)
    get_filename_component(SHADER_SRC_NAME_WE ${SHADER_SRC} NAME_WE)
    set(SHADER_COMP_HEADER ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_SRC_NAME_WE}.comp.hex.h)
    add_custom_command(
      OUTPUT ${SHADER_COMP_HEADER}
      COMMAND ${CMAKE_COMMAND}
        -DSHADER_SRC=${RIFE_NCNN_DIR}/${SHADER_SRC}
        -DSHADER_COMP_HEADER=${SHADER_COMP_HEADER}
        -P "${RIFE_NCNN_DIR}/generate_shader_comp_header.cmake"
      DEPENDS ${RIFE_NCNN_DIR}/${SHADER_SRC}
      COMMENT "Preprocessing shader source ${SHADER_SRC_NAME_WE}.comp"
      VERBATIM)
    set_source_files_properties(${SHADER_COMP_HEADER} PROPERTIES GENERATED TRUE)
    list(APPEND SHADER_SPV_HEX_FILES ${SHADER_COMP_HEADER})
  endmacro()

  rife_add_shader(rife_preproc.comp)
  rife_add_shader(rife_postproc.comp)
  rife_add_shader(rife_preproc_tta.comp)
  rife_add_shader(rife_postproc_tta.comp)
  rife_add_shader(rife_flow_tta_avg.comp)
  rife_add_shader(rife_v2_flow_tta_avg.comp)
  rife_add_shader(rife_v4_flow_tta_avg.comp)
  rife_add_shader(rife_flow_tta_temporal_avg.comp)
  rife_add_shader(rife_v2_flow_tta_temporal_avg.comp)
  rife_add_shader(rife_v4_flow_tta_temporal_avg.comp)
  rife_add_shader(rife_out_tta_temporal_avg.comp)
  rife_add_shader(rife_v4_timestep.comp)
  rife_add_shader(rife_v4_timestep_tta.comp)
  rife_add_shader(warp.comp)
  rife_add_shader(warp_pack4.comp)
  rife_add_shader(warp_pack8.comp)

  add_custom_target(generate-spirv DEPENDS ${SHADER_SPV_HEX_FILES})

  # Static library bundling the RIFE ncnn sources and their generated shader
  # headers, linked into the application instead of compiled inline.
  add_library(NCNNRIFE STATIC
      ${RIFE_NCNN_DIR}/rife.cpp
      ${RIFE_NCNN_DIR}/warp.cpp)
  add_dependencies(NCNNRIFE generate-spirv)
  target_include_directories(NCNNRIFE PUBLIC ${RIFE_NCNN_INCLUDE_DIRS})
  target_compile_definitions(NCNNRIFE PUBLIC HAVE_NCNN)
  target_link_libraries(NCNNRIFE PUBLIC ncnn)

endif()



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
    ./rsc/mesh/icon_code.ply
    ./rsc/mesh/icon_camera.ply
    ./rsc/mesh/icon_share.ply
    ./rsc/mesh/icon_clone.ply
    ./rsc/mesh/icon_vimix.ply
    ./rsc/mesh/icon_group_vimix.ply
    ./rsc/mesh/icon_circles.ply
    ./rsc/mesh/icon_empty.ply
    ./rsc/mesh/icon_lock.ply
    ./rsc/mesh/icon_unlock.ply
    ./rsc/mesh/icon_circle.ply
    ./rsc/mesh/icon_square.ply
    ./rsc/mesh/icon_cross.ply
    ./rsc/mesh/icon_clock.ply
    ./rsc/mesh/icon_clock_hand.ply
    ./rsc/mesh/icon_grid.ply
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
    ./rsc/mesh/icon_blend_normal.ply
    ./rsc/mesh/icon_blend_add.ply
    ./rsc/mesh/icon_blend_subtract.ply
    ./rsc/mesh/icon_blend_mult.ply
    ./rsc/mesh/icon_blend_soft_light.ply
    ./rsc/mesh/icon_blend_hard_light.ply
    ./rsc/mesh/icon_blend_soft_sub.ply
    ./rsc/mesh/icon_blend_lighten.ply
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
    ./rsc/shaders/filters/color.glsl
    ./rsc/shaders/filters/HSV.glsl
    ./rsc/shaders/filters/RGBnoise.glsl
    ./rsc/shaders/filters/3DPerlinNoise.glsl
    ./rsc/shaders/filters/3DSimplexNoise.glsl
    ./rsc/shaders/filters/source.glsl
    ./rsc/images/logo.vmx
)

#####
##### Sync gamecontrollerdb.txt from git submodule if different
#####
set(SUBMODULE_DB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ext/mdqinc/gamecontrollerdb.txt")
set(RSC_DB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/rsc/gamecontrollerdb.txt")

# Check if submodule file exists
if(EXISTS "${SUBMODULE_DB_PATH}")
    # Compare the files to see if they differ
    set(FILES_DIFFER FALSE)

    if(NOT EXISTS "${RSC_DB_PATH}")
        # rsc file doesn't exist, copy it
        set(FILES_DIFFER TRUE)
    else()
        # Both files exist, compare them
        file(MD5 "${SUBMODULE_DB_PATH}" SUBMODULE_MD5)
        file(MD5 "${RSC_DB_PATH}" RSC_MD5)

        if(NOT "${SUBMODULE_MD5}" STREQUAL "${RSC_MD5}")
            set(FILES_DIFFER TRUE)
        else()
            message(STATUS "gamecontrollerdb.txt is up-to-date with submodule")
        endif()
    endif()

    # Copy if different
    if(FILES_DIFFER)
        file(COPY "${SUBMODULE_DB_PATH}" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/rsc/")
        message(STATUS "Updated rsc/gamecontrollerdb.txt from ext/mdqinc submodule")

        # Show version info if available
        if(GIT_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/mdqinc/.git")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} -C "${CMAKE_CURRENT_SOURCE_DIR}/ext/mdqinc" log -1 --format="%h %s"
                OUTPUT_VARIABLE SUBMODULE_COMMIT
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            message(STATUS "  Submodule commit: ${SUBMODULE_COMMIT}")
        endif()
    endif()
else()
    # Submodule not initialized
    if(EXISTS "${RSC_DB_PATH}")
        message(STATUS "Using existing rsc/gamecontrollerdb.txt (submodule not initialized)")
    else()
        message(WARNING "gamecontrollerdb.txt not found!")
        message(WARNING "  Please run: git submodule update --init ext/mdqinc")
    endif()
endif()

# Add gamecontrollerdb.txt to resources
list(APPEND VMIX_RSC_FILES ./rsc/gamecontrollerdb.txt)

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
