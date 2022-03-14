
cmake_minimum_required(VERSION 3.8.2)
project(vimix VERSION 0.0.1 LANGUAGES CXX C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# use git
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
      string(SUBSTRING ${GIT_DESCRIBE_VERSION} 4 1 VIMIX_VERSION_PATCH)
      add_definitions(-DVIMIX_VERSION_MAJOR=${VIMIX_VERSION_MAJOR})
      add_definitions(-DVIMIX_VERSION_MINOR=${VIMIX_VERSION_MINOR})
      add_definitions(-DVIMIX_VERSION_PATCH=${VIMIX_VERSION_PATCH})
      message(STATUS "Compiling vimix version ${VIMIX_VERSION_MAJOR}.${VIMIX_VERSION_MINOR}.${VIMIX_VERSION_PATCH}")
  else()
      message(STATUS "Compiling vimix (unknown version)")
  endif()
endif()

set(CMAKE_INCLUDE_CURRENTDIR ON)

# Find the cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} /usr/share/cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules )
include(MacroLogFeature)
include(MacroFindGStreamerLibrary)

if(UNIX)
    if (APPLE)
        add_definitions(-DAPPLE)

        # the RPATH to be used when installing
        set(CMAKE_SKIP_RPATH TRUE)
        set(OpenGL_DIR /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/)
        set(CMAKE_OSX_ARCHITECTURES "x86_64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14")

        # find icu4c in OSX (pretty well hidden...)
        set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/opt/icu4c/lib/pkgconfig")

    else()
        add_definitions(-DLINUX)

        # linux opengl
        set(OpenGL_GL_PREFERENCE "GLVND")

        # linux dialogs use GTK
        find_package(GTK 3.0 REQUIRED)
        macro_log_feature(GTK_FOUND "GTK" "GTK cross-platform widget toolkit" "http://www.gtk.org" TRUE)

    endif()
    add_definitions(-DUNIX)
elseif(WIN32)
    add_definitions(-DWIN32)
    add_definitions(-DMINGW32)
endif()

# static sub packages in ext
set(BUILD_STATIC_LIBS ON)

#####
##### Dependencies
#####

# Basics
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

find_package(GLIB2)
macro_log_feature(GLIB2_FOUND "GLib" "GTK general-purpose utility library" "http://www.gtk.org" TRUE)

find_package(GObject)
macro_log_feature(GOBJECT_FOUND "GObject" "GTK object-oriented framework" "http://www.gtk.org" TRUE)

find_package(PNG REQUIRED)
macro_log_feature(PNG_FOUND "PNG" "Portable Network Graphics" "http://www.libpng.org" TRUE)

#
# GSTREAMER
#
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

#
# ICU4C
#
if (PKG_CONFIG_FOUND)
    pkg_check_modules(ICU REQUIRED icu-i18n icu-uc icu-io)
else ()
    find_package(ICU REQUIRED COMPONENTS i18n io uc)
endif ()
macro_log_feature(ICU_FOUND "ICU" "International Components for Unicode" "http://site.icu-project.org" TRUE)

#
# GLFW3
#
if (PKG_CONFIG_FOUND)
    pkg_check_modules(GLFW3 REQUIRED glfw3>=3.2)
else ()
    find_package(glfw3 3.2 REQUIRED)
endif()
macro_log_feature(GLFW3_FOUND "glfw3" "Open Source multi-platform library for OpenGL" "http://www.glfw.org" TRUE)

#
# GLM
#
if (PKG_CONFIG_FOUND)
    pkg_check_modules(GLM QUIET glm>=0.9.9)
else ()
    find_package(glm QUIET)
endif()

if (GLM_FOUND)
    macro_log_feature(GLM_FOUND "GLM" "OpenGL mathematics" "https://glm.g-truc.net" TRUE)
else ()
    set(GLM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ext/glm)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/glm)
    message(STATUS "Compiling 'GLM' OpenGL mathematics https://glm.g-truc.net -- ${CMAKE_CURRENT_SOURCE_DIR}/ext/glm")
endif()

#
# TINY XML 2
#
if (PKG_CONFIG_FOUND)
    pkg_check_modules(TINYXML2 QUIET tinyxml2>=8.0)
else ()
    find_package(tinyxml2 QUIET)
endif()

if (TINYXML2_FOUND)
    macro_log_feature(TINYXML2_FOUND "TinyXML2" "TinyXML2 library" "https://github.com/leethomason/tinyxml2.git" TRUE)
else ()
    set(TINYXML2_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ext/tinyxml2)
    set(TINYXML2_LIBRARIES TINYXML2)
    add_library(TINYXML2 "${CMAKE_CURRENT_SOURCE_DIR}/ext/tinyxml2/tinyxml2.cpp")
    message(STATUS "Compiling 'TinyXML2' from https://github.com/leethomason/tinyxml2.git -- ${TINYXML2_INCLUDE_DIRS}")
endif()

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

#
# FILE DIALOG: use tinyfiledialog for all except Linux
#
if(UNIX)
    if (APPLE)
        set(TINYFD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/tfd)
        add_library(TINYFD "${CMAKE_CURRENT_SOURCE_DIR}/ext/tfd/tinyfiledialogs.c")
        message(STATUS "Compiling 'TinyFileDialog' from https://git.code.sf.net/p/tinyfiledialogs/code -- ${TINYFD_INCLUDE_DIR}.")
        set(TINYFD_LIBRARY TINYFD)
    endif()
else()
    set(TINYFD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/tfd)
    add_library(TINYFD "${CMAKE_CURRENT_SOURCE_DIR}/ext/tfd/tinyfiledialogs.c")
    message(STATUS "Compiling 'TinyFileDialog' from https://git.code.sf.net/p/tinyfiledialogs/code -- ${TINYFD_INCLUDE_DIR}.")
    set(TINYFD_LIBRARY TINYFD)
endif()

#
# DIRENT (windows only)
if(WIN32)
    set(DIRENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/Dirent/include)
    message(STATUS "Including 'Dirent' from https://github.com/tronkko/dirent -- ${DIRENT_INCLUDE_DIR}.")
endif()

#####
##### Custom built libraries
#####

#
# DEAR IMGUI
#
set(IMGUI_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui//imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui//imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui//imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/examples/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/examples/imgui_impl_opengl3.cpp
)
add_definitions(-DIMGUI_USE_STB_SPRINTF)
set(IMGUI_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui)
add_library(IMGUI "${IMGUI_SRCS}")
target_compile_definitions(IMGUI PRIVATE "IMGUI_IMPL_OPENGL_LOADER_GLAD")
message(STATUS "Compiling 'Dear ImGui' from https://github.com/ocornut/imgui.git -- ${IMGUI_INCLUDE_DIR}")

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
set(OSCPACK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/OSCPack)
add_library(OSCPACK "${OSCPACK_SRCS}")
message(STATUS "Compiling 'OSCPack' from http://www.rossbencina.com/code/oscpack -- ${OSCPACK_INCLUDE_DIR}")

#
# GLAD
#
set(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/glad/include)
add_library(GLAD "${CMAKE_CURRENT_SOURCE_DIR}/ext/glad/src/glad.c")
message(STATUS "Including 'GLAD' Open source multi-language OpenGL loader https://glad.dav1d.de -- ${GLAD_INCLUDE_DIR}")

#
# ImGui Color Text Editor
#
set(IMGUITEXTEDIT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/ImGuiColorTextEdit)
set(IMGUITEXTEDIT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/ImGuiColorTextEdit/TextEditor.cpp
)
message(STATUS "Including 'ImGuiColorTextEdit' from https://github.com/BalazsJako/ImGuiColorTextEdit -- ${IMGUITEXTEDIT_INCLUDE_DIR}")


macro_display_feature_log()

#####
##### Application
#####

# Setup the environment
include_directories(
    ${GSTREAMER_INCLUDE_DIR}
    ${GSTREAMER_AUDIO_INCLUDE_DIR}
    ${GSTREAMER_VIDEO_INCLUDE_DIR}
    ${GSTREAMER_BASE_INCLUDE_DIR}
    ${GSTREAMER_APP_INCLUDE_DIR}
    ${GSTREAMER_PBUTILS_INCLUDE_DIR}
    ${GSTREAMER_GL_INCLUDE_DIR}
    ${GLFW3_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}/examples
    ${IMGUITEXTEDIT_INCLUDE_DIR}
    ${TINYXML2_INCLUDE_DIRS}
    ${TINYFD_INCLUDE_DIR}
    ${STB_INCLUDE_DIRS}
    ${DIRENT_INCLUDE_DIR}
    ${OSCPACK_INCLUDE_DIR}
    ${link_HEADERS}
)

link_directories(
    ${GLFW3_LIBRARY_DIRS}
    ${ICU_LIBRARY_DIRS}
)


set(VMIX_BINARY "vimix")
set(VMIX_SRCS
    main.cpp
    Log.cpp
    BaseToolkit.cpp
    Shader.cpp
    ImageShader.cpp
    ImageProcessingShader.cpp
    UpdateCallback.cpp
    Scene.cpp
    Primitives.cpp
    Mesh.cpp
    Decorations.cpp
    View.cpp
    RenderView.cpp
    GeometryView.cpp
    MixingView.cpp
    MixingGroup.cpp
    LayerView.cpp
    TextureView.cpp
    TransitionView.cpp
    Source.cpp
    CloneSource.cpp
    RenderSource.cpp
    SourceCallback.cpp
    SourceList.cpp
    Session.cpp
    Selection.cpp
    SessionSource.cpp
    SessionVisitor.cpp
    Interpolator.cpp
    SessionCreator.cpp
    SessionParser.cpp
    Mixer.cpp
    FrameGrabber.cpp
    Recorder.cpp
    Streamer.cpp
    Loopback.cpp
    Settings.cpp
    Screenshot.cpp
    Resource.cpp
    Timeline.cpp
    Stream.cpp
    MediaPlayer.cpp
    MediaSource.cpp
    StreamSource.cpp
    PatternSource.cpp
    DeviceSource.cpp
    NetworkSource.cpp
    MultiFileSource.cpp
    FrameBuffer.cpp
    RenderingManager.cpp
    UserInterfaceManager.cpp
    PickingVisitor.cpp
    BoundingBoxVisitor.cpp
    DrawVisitor.cpp
    SearchVisitor.cpp
    ImGuiToolkit.cpp
    ImGuiVisitor.cpp
    InfoVisitor.cpp
    GstToolkit.cpp
    GlmToolkit.cpp
    SystemToolkit.cpp
    DialogToolkit.cpp
    tinyxml2Toolkit.cpp
    NetworkToolkit.cpp
    Connection.cpp
    ActionManager.cpp
    Overlay.cpp
    Metronome.cpp
    ControlManager.cpp
    VideoBroadcast.cpp
    SrtReceiverSource.cpp
)


set(VMIX_RSC_FILES
    ./rsc/shaders/simple.fs
    ./rsc/shaders/simple.vs
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
    ./rsc/fonts/Hack-Regular.ttf
    ./rsc/fonts/Roboto-Regular.ttf
    ./rsc/fonts/Roboto-Bold.ttf
    ./rsc/fonts/Roboto-Italic.ttf
    ./rsc/fonts/fa-regular-400.ttf
    ./rsc/fonts/fa-solid-900.ttf
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
    ./rsc/mesh/border_handles_lock.ply
    ./rsc/mesh/border_handles_lock_open.ply
    ./rsc/mesh/border_handles_shadow.ply
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
    ./rsc/mesh/icon_vector_square_slash.ply
    ./rsc/mesh/icon_cube.ply
    ./rsc/mesh/icon_sequence.ply
    ./rsc/mesh/icon_receive.ply
    ./rsc/mesh/h_line.ply
    ./rsc/mesh/h_mark.ply
)

# Include the CMake RC module
include(CMakeRC)
cmrc_add_resource_library(vmix-resources ALIAS vmix::rc NAMESPACE vmix WHENCE rsc ${VMIX_RSC_FILES})
message(STATUS "Using 'CMakeRC ' from https://github.com/vector-of-bool/cmrc.git -- ${CMAKE_MODULE_PATH}.")

### DEFINE THE TARGET (OS specific)

IF(APPLE)

    # set icon
    set(MACOSX_BUNDLE_ICON vimix.icns)
    set(MACOSX_BUNDLE_ICON_FILE ${CMAKE_SOURCE_DIR}/osx/${MACOSX_BUNDLE_ICON})
    # set where in the bundle to put the icns file
    set_source_files_properties(${MACOSX_BUNDLE_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # create the application
    add_executable(${VMIX_BINARY} MACOSX_BUNDLE
        ${VMIX_SRCS}
        ${IMGUITEXTEDIT_SRC}
        ${MACOSX_BUNDLE_ICON_FILE}
    )

    # set the Info.plist file
    set(MACOSX_BUNDLE_PLIST_FILE ${CMAKE_SOURCE_DIR}/osx/Info.plist)
    set_target_properties(${VMIX_BINARY} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_PLIST_FILE})

    set(PLATFORM_LIBS
        "-framework CoreFoundation"
        "-framework Appkit"
    )

ELSE(APPLE)

    link_directories (${GTK3_LIBRARY_DIRS})

    add_executable(${VMIX_BINARY}
        ${VMIX_SRCS}
        ${IMGUITEXTEDIT_SRC}
    )

    set(PLATFORM_LIBS
        GTK::GTK
    )

ENDIF(APPLE)

### COMPILE THE TARGET (all OS)

set_property(TARGET ${VMIX_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${VMIX_BINARY} PROPERTY C_STANDARD 11)

target_compile_definitions(${VMIX_BINARY} PUBLIC "IMGUI_IMPL_OPENGL_LOADER_GLAD")

target_link_libraries(${VMIX_BINARY} LINK_PRIVATE
    vmix::rc
    ${GLM_LIBRARIES}
    GLAD
    IMGUI
    OSCPACK
    ${TINYFD_LIBRARY}
    ${GLFW3_LIBRARIES}
    ${TINYXML2_LIBRARIES}
    ${ICU_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${GOBJECT_LIBRARIES}
    ${GSTREAMER_LIBRARY}
    ${GSTREAMER_BASE_LIBRARY}
    ${GSTREAMER_APP_LIBRARY}
    ${GSTREAMER_AUDIO_LIBRARY}
    ${GSTREAMER_VIDEO_LIBRARY}
    ${GSTREAMER_PBUTILS_LIBRARY}
    ${GSTREAMER_GL_LIBRARY}
    Threads::Threads
    PNG::PNG
    Ableton::Link
    ${PLATFORM_LIBS}
)


### DEFINE THE PACKAGING (all OS)

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


# optimize size ?
SET(CPACK_STRIP_FILES TRUE)

### DEFINE THE PACKAGING (OS specific)

IF(APPLE)

    # Bundle target
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_BINARY_DRAGNDROP ON)

    # OSX cpack info
    set(CPACK_SYSTEM_NAME  "OSX_${CMAKE_OSX_DEPLOYMENT_TARGET}_${CMAKE_OSX_ARCHITECTURES}")

    install(TARGETS ${VMIX_BINARY}
            CONFIGURATIONS Release RelWithDebInfo
            BUNDLE  DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

    # create GST plugins directory in Bundle Resources subfolder
    set(plugin_dest_dir vimix.app/Contents/Resources/)

    ### TODO configure auto to find installation dir of gst

    message(STATUS "install  gst-plugins ${PKG_GSTREAMER_PLUGIN_DIR}")
    message(STATUS "install  gst-plugins-base ${PKG_GSTREAMER_BASE_PLUGIN_DIR}")

    if (PKG_CONFIG_FOUND)
        pkg_check_modules(PKG_GSTREAMER_PLUGINS_BAD gstreamer-plugins-bad-${GSTREAMER_ABI_VERSION})
        set(PKG_GSTREAMER_BAD_PLUGIN_DIR ${PKG_GSTREAMER_PLUGINS_BAD_LIBDIR}/gstreamer-${GSTREAMER_ABI_VERSION})
        message(STATUS "install  gst-plugins-bad ${PKG_GSTREAMER_BAD_PLUGIN_DIR}")
    endif()


    # intall the gst-plugin-scanner program (used by plugins at load time)
    set(PKG_GSTREAMER_SCANNER "${PKG_GSTREAMER_PREFIX}/libexec/gstreamer-1.0/gst-plugin-scanner")
    message(STATUS "install  gst-plugin-scanner ${PKG_GSTREAMER_SCANNER}")
    install(FILES "${PKG_GSTREAMER_SCANNER}"
        DESTINATION "${plugin_dest_dir}"
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT Runtime
    )

    # Install the gst-plugins (all those installed with brew )
    install(DIRECTORY "${PKG_GSTREAMER_PLUGIN_DIR}" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
    install(DIRECTORY "${PKG_GSTREAMER_BASE_PLUGIN_DIR}" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
    install(DIRECTORY "${PKG_GSTREAMER_BAD_PLUGIN_DIR}" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)

    install(DIRECTORY "/usr/local/Cellar/gst-plugins-good/1.18.4/lib/gstreamer-1.0" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
    install(DIRECTORY "/usr/local/Cellar/gst-plugins-ugly/1.18.4_1/lib/gstreamer-1.0" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
    install(DIRECTORY "/usr/local/Cellar/gst-libav/1.18.4/lib/gstreamer-1.0" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)

    # install locally recompiled & installed gst-plugins (because not included in brew package)
    install(FILES   "/usr/local/lib/gstreamer-1.0/libgstapplemedia.dylib"
                    "/usr/local/lib/gstreamer-1.0/libgstde265.dylib"
                    "/usr/local/lib/gstreamer-1.0/libgstx265.dylib"
            DESTINATION "${plugin_dest_dir}/gstreamer-1.0" COMPONENT Runtime)

    # install frei0r plugins (dependencies of gstreamer-1.0/libgstfrei0r.dylib plugin)
    install(FILES "/usr/local/Cellar/frei0r/1.7.0/lib/frei0r-1/lissajous0r.so"
                  "/usr/local/Cellar/frei0r/1.7.0/lib/frei0r-1/rgbnoise.so"
            DESTINATION "${plugin_dest_dir}/frei0r-1" COMPONENT Runtime)


    # ICU DATA LIB GST dependency : undocumented and hacked here : seems to work
#    install(FILES "${ICU_LINK_LIBRARIES}" DESTINATION "${plugin_dest_dir}/gstreamer-1.0" COMPONENT Runtime)
    install(FILES "/usr/local/Cellar/icu4c/69.1/lib/libicudata.69.1.dylib" DESTINATION "${plugin_dest_dir}/gstreamer-1.0" RENAME "libicudata.69.dylib" COMPONENT Runtime)
    message(STATUS "install  ${ICU_LINK_LIBRARIES} from ${ICU_LIBRARY_DIRS}")

    # package runtime fixup bundle
    set(APPS "\${CMAKE_INSTALL_PREFIX}/vimix.app")
    install(CODE "
            file(GLOB_RECURSE GSTPLUGINS \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/gstreamer-1.0/*.dylib\")
            list(APPEND LIBS_PATH \"\${ICU_LIBRARY_DIRS}\")
            include(BundleUtilities)
            set(BU_CHMOD_BUNDLE_ITEMS TRUE)
            fixup_bundle(\"${APPS}\" \"\${GSTPLUGINS}\" \"${LIBS_PATH}\")
            "
            COMPONENT Runtime
    )

    set(APPLE_CODESIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/osx/entitlements.plist")
    set(APPLE_CODESIGN_IDENTITY "" CACHE STRING "")
    string(LENGTH "${APPLE_CODESIGN_IDENTITY}" APPLE_CODESIGN_IDENTITY_LENGHT)
    if( ${APPLE_CODESIGN_IDENTITY_LENGHT} LESS 40 )
        message(STATUS "Not signing bundle. Specify APPLE_CODESIGN_IDENTITY to cmake before running cpack to sign")
    else()
        install(CODE "
                execute_process(COMMAND
                codesign --verbose=4 --deep --force
                --entitlements \"${APPLE_CODESIGN_ENTITLEMENTS}\"
                --sign \"${APPLE_CODESIGN_IDENTITY}\"
                \"${APPS}\" )
                "
                COMPONENT Runtime
        )
    endif()

#    # package runtime fixup bundle and codesign
#    set(BUNDLE_NAME "vimix.app")
#    set(BUNDLE_LIBS_DIR "${plugin_dest_dir}/gstreamer-1.0")
#    set(BUNDLE_DIRS "${ICU_LIBRARY_DIRS}")
#    set(APPLE_CODESIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/osx/entitlements.plist")

#    configure_file(cmake/modules/BundleInstall.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/BundleInstall.cmake" @ONLY)
#    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/BundleInstall.cmake" COMPONENT Runtime)

ELSE(APPLE)

    install(TARGETS ${VMIX_BINARY}
            CONFIGURATIONS Release RelWithDebInfo
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

ENDIF(APPLE)

# Package full name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}_${CPACK_SYSTEM_NAME}")

# To Create a package, run "cpack"
include(CPack)
