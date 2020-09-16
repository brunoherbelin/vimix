
cmake_minimum_required(VERSION 3.8.0)
project(vimix VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON")
set(CMAKE_INCLUDE_CURRENTDIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules )
include(MacroLogFeature)

if(UNIX)
    if (APPLE)
        add_definitions(-DAPPLE)

        # the RPATH to be used when installing
        set(CMAKE_SKIP_RPATH TRUE)

        set(OpenGL_DIR /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/)
        set(CMAKE_OSX_ARCHITECTURES x86_64)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13")

    else()
        add_definitions(-DLINUX)

        set(OpenGL_GL_PREFERENCE "GLVND")

    endif()
    add_definitions(-DUNIX)
elseif(WIN32)
    add_definitions(-DWIN32)
    add_definitions(-DMINGW32)
endif()


# Include the CMake RC module
include(CMakeRC)

#
# GSTREAMER
#

find_package(GStreamer 1.0.0 COMPONENTS base)
macro_log_feature(GSTREAMER_FOUND "GStreamer"
"Open Source Multiplatform Multimedia Framework"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")
macro_log_feature(GSTREAMER_BASE_LIBRARY_FOUND "GStreamer base library"
"${GSTREAMER_BASE_LIBRARY}"
"http://gstreamer.freedesktop.org/" FALSE "1.0.0")

find_package(GStreamerPluginsBase 1.0.0 COMPONENTS app audio video pbutils gl)
macro_log_feature(GSTREAMER_APP_LIBRARY_FOUND "GStreamer app library"
"${GSTREAMER_APP_LIBRARY}"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_AUDIO_LIBRARY_FOUND "GStreamer audio library"
"${GSTREAMER_AUDIO_LIBRARY}"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_VIDEO_LIBRARY_FOUND "GStreamer video library"
"${GSTREAMER_VIDEO_LIBRARY}"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_PBUTILS_LIBRARY_FOUND "GStreamer pbutils library"
"${GSTREAMER_PBUTILS_LIBRARY}"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")

macro_log_feature(GSTREAMER_GL_LIBRARY_FOUND "GStreamer opengl library"
"${GSTREAMER_GL_LIBRARY}"
"http://gstreamer.freedesktop.org/" TRUE "1.0.0")


#find_package(GStreamerPluginsBad 1.0.0 COMPONENTS player)
#macro_log_feature(GSTREAMER_PLAYER_LIBRARY_FOUND "GStreamer player library"
#"${GSTREAMER_PLAYER_LIBRARY}"
#"http://gstreamer.freedesktop.org/" TRUE "1.0.0")


# Various preprocessor definitions for GST
add_definitions(-DGST_DISABLE_XML -DGST_DISABLE_LOADSAVE)

# Basics
find_package(GLIB2)
macro_log_feature(GLIB2_FOUND "GLib" "GTK general-purpose utility library" "http://www.gtk.org/" TRUE)

find_package(GObject)
macro_log_feature(GOBJECT_FOUND "GObject" "GTK object-oriented framework" "http://www.gtk.org/" TRUE)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
set(THREAD_LIBRARY Threads::Threads)

find_package(PNG REQUIRED)
set(PNG_LIBRARY PNG::PNG)

#
# GLFW3
# NB: set glfw3_PATH to /usr/local/Cellar/glfw/3.3.2/lib/cmake/glfw3
#
find_package(glfw3 3.2 REQUIRED)
macro_log_feature(glfw3_FOUND "GLFW3" "Open Source multi-platform library for OpenGL" "http://www.glfw.org/" TRUE)
set(GLFW_LIBRARY glfw)

#find_package(OpenGL REQUIRED)

# static sub packages in ext
set(BUILD_STATIC_LIBS ON)

#
# GLM
#
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/glm)
message(STATUS "Compiling 'GLM' OpenGL mathematics https://glm.g-truc.net.")

#
# GLAD
#
set(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/glad/include)
add_library(GLAD "${CMAKE_CURRENT_SOURCE_DIR}/ext/glad/src/glad.c")
message(STATUS "Compiling 'GLAD' Open source multi-language OpenGL loader https://glad.dav1d.de/ -- ${GLAD_INCLUDE_DIR}.")

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
set(IMGUI_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui)
add_library(IMGUI "${IMGUI_SRCS}")
target_compile_definitions(IMGUI PRIVATE "IMGUI_IMPL_OPENGL_LOADER_GLAD")
message(STATUS "Compiling 'Dear ImGui' from https://github.com/ocornut/imgui.git -- ${IMGUI_INCLUDE_DIR}.")

#
# ImGui Color Text Editor
#
set(IMGUITEXTEDIT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/ImGuiColorTextEdit)
set(IMGUITEXTEDIT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/ImGuiColorTextEdit/TextEditor.cpp
)
message(STATUS "Including 'ImGuiColorTextEdit' from https://github.com/BalazsJako/ImGuiColorTextEdit -- ${IMGUITEXTEDIT_INCLUDE_DIR}.")

#
# TINY XML 2
#
set(TINYXML2_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/tinyxml2)
add_library(TINYXML2 "${CMAKE_CURRENT_SOURCE_DIR}/ext/tinyxml2/tinyxml2.cpp")
message(STATUS "Compiling 'TinyXML2' from https://github.com/leethomason/tinyxml2.git -- ${TINYXML2_INCLUDE_DIR}.")

#
# STB
#
set(STB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/stb)
add_definitions(-DIMGUI_USE_STB_SPRINTF)
message(STATUS "Including 'STB Nothings' from https://github.com/nothings/stb -- ${STB_INCLUDE_DIR}.")

#
# DIRENT
#
if(WIN32)
    set(DIRENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/Dirent/include)
    message(STATUS "Including 'Dirent' from https://github.com/tronkko/dirent -- ${DIRENT_INCLUDE_DIR}.")
endif( WIN32 )

#
# TINY FILE DIALOG
#
set(TINYFD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/tfd)
add_library(TINYFD "${CMAKE_CURRENT_SOURCE_DIR}/ext/tfd/tinyfiledialogs.c")
message(STATUS "Compiling 'TinyFileDialog' from https://github.com/native-toolkit/tinyfiledialogs.git -- ${TINYFD_INCLUDE_DIR}.")

#
# OBJ LOADER
#
#set(OBJLOADER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/obj)
#add_library(OBJLOADER "${CMAKE_CURRENT_SOURCE_DIR}/ext/obj/ObjLoader.cpp")
#message(STATUS "Compiling 'ObjLoader' from https://github.com/mortennobel/OpenGL_3_2_Utils -- ${OBJLOADER_INCLUDE_DIR}.")

# find_package(PkgConfig REQUIRED)
# pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

#
#  Application
#

# Setup the environment
include_directories(
    ${GSTREAMER_INCLUDE_DIR}
    ${GSTREAMER_AUDIO_INCLUDE_DIR}
    ${GSTREAMER_VIDEO_INCLUDE_DIR}
    ${GSTREAMER_BASE_INCLUDE_DIR}
    ${GSTREAMER_APP_INCLUDE_DIR}
    ${GSTREAMER_PBUTILS_INCLUDE_DIR}
    ${GSTREAMER_GL_INCLUDE_DIR}
    ${GLIB2_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${GLM_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}/examples
    ${IMGUITEXTEDIT_INCLUDE_DIR}
    ${TINYXML2_INCLUDE_DIR}
    ${TINYFD_INCLUDE_DIR}
    ${STB_INCLUDE_DIR}
    ${DIRENT_INCLUDE_DIR}
    ${OBJLOADER_INCLUDE_DIR}
)


set(VMIX_BINARY "vimix")
set(VMIX_SRCS
    main.cpp
    Log.cpp
    Shader.cpp
    ImageShader.cpp
    ImageProcessingShader.cpp
    UpdateCallback.cpp
    Scene.cpp
    Primitives.cpp
    Mesh.cpp
    Decorations.cpp
    View.cpp
    Source.cpp
    Session.cpp
    Selection.cpp
    SessionSource.cpp
    SessionVisitor.cpp
    GarbageVisitor.cpp
    SessionCreator.cpp
    Mixer.cpp
    Recorder.cpp
    Settings.cpp
    Screenshot.cpp
    Resource.cpp
    FileDialog.cpp
    Timeline.cpp
    MediaPlayer.cpp
    MediaSource.cpp
    FrameBuffer.cpp
    RenderingManager.cpp
    UserInterfaceManager.cpp
    PickingVisitor.cpp
    BoundingBoxVisitor.cpp
    DrawVisitor.cpp
    SearchVisitor.cpp
    ImGuiToolkit.cpp
    ImGuiVisitor.cpp
    GstToolkit.cpp
    GlmToolkit.cpp
    SystemToolkit.cpp
    tinyxml2Toolkit.cpp
)

set(VMIX_RSC_FILES
    ./rsc/shaders/simple.fs
    ./rsc/shaders/simple.vs
    ./rsc/shaders/image.fs
    ./rsc/shaders/image.vs
    ./rsc/shaders/imageprocessing.fs
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
    ./rsc/images/shadow_perspective.dds
    ./rsc/mesh/disk.ply
    ./rsc/mesh/circle.ply
    ./rsc/mesh/shadow.ply
    ./rsc/mesh/glow.ply
    ./rsc/mesh/border_round.ply
    ./rsc/mesh/border_top.ply
    ./rsc/mesh/border_sharp.ply
    ./rsc/mesh/border_large_round.ply
    ./rsc/mesh/border_large_top.ply
    ./rsc/mesh/border_handles_rotation.ply
    ./rsc/mesh/border_handles_scale.ply
    ./rsc/mesh/border_handles_overlay.ply
    ./rsc/mesh/border_handles_overlay_filled.ply
    ./rsc/mesh/border_handles_sharp.ply
    ./rsc/mesh/border_large_sharp.ply
    ./rsc/mesh/border_vertical_overlay.ply
    ./rsc/mesh/perspective_layer.ply
    ./rsc/mesh/shadow_perspective.ply
    ./rsc/mesh/point.ply
    ./rsc/mesh/square_point.ply
    ./rsc/mesh/icon_video.ply
    ./rsc/mesh/icon_image.ply
    ./rsc/mesh/icon_render.ply
    ./rsc/mesh/icon_clone.ply
    ./rsc/mesh/icon_vimix.ply
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
    ./rsc/mesh/h_line.ply
    ./rsc/mesh/h_mark.ply
)

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

ELSE(APPLE)

    add_executable(${VMIX_BINARY}
        ${VMIX_SRCS}
        ${IMGUITEXTEDIT_SRC}
    )

ENDIF(APPLE)


### COMPILE THE TARGET (all OS)

set_property(TARGET ${VMIX_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${VMIX_BINARY} PROPERTY C_STANDARD 11)

target_compile_definitions(${VMIX_BINARY} PUBLIC "IMGUI_IMPL_OPENGL_LOADER_GLAD")

cmrc_add_resource_library(vmix-resources ALIAS vmix::rc NAMESPACE vmix WHENCE rsc ${VMIX_RSC_FILES})
message(STATUS "Using 'CMakeRC ' from https://github.com/vector-of-bool/cmrc.git -- ${CMAKE_MODULE_PATH}.")


target_link_libraries(${VMIX_BINARY} LINK_PRIVATE 
    ${GLFW_LIBRARY}
    GLAD
    glm::glm
    ${CMAKE_DL_LIBS}
    ${GOBJECT_LIBRARIES}
    ${GSTREAMER_LIBRARY}
    ${GSTREAMER_BASE_LIBRARY}
    ${GSTREAMER_APP_LIBRARY}
    ${GSTREAMER_AUDIO_LIBRARY}
    ${GSTREAMER_VIDEO_LIBRARY}
    ${GSTREAMER_PBUTILS_LIBRARY}
    ${GSTREAMER_GL_LIBRARY}
    ${GSTREAMER_PLAYER_LIBRARY}
    ${NFD_LIBRARY}
    ${PNG_LIBRARY}
    ${THREAD_LIBRARY}
    TINYXML2
    TINYFD
    IMGUI
    vmix::rc
)

macro_display_feature_log()


### DEFINE THE PACKAGING (all OS)

SET(CPACK_PACKAGE_NAME "vimix")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "vimix\n Real-time video mixing for live performance.")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
SET(CPACK_PACKAGE_CONTACT "bruno.herbelin@gmail.com")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING.txt")
SET(CPACK_PACKAGE_VERSION_MAJOR "0")
SET(CPACK_PACKAGE_VERSION_MINOR "3")
SET(CPACK_PACKAGE_VENDOR "Bruno Herbelin")
SET(CPACK_SOURCE_IGNORE_FILES
    "/\\\\.git/"
    "/\\\\.gitignore$"
    "/\\\\.gitmodules$"
)


### DEFINE THE PACKAGING (OS specific)

IF(APPLE)

    # Bundle target
    set(CPACK_GENERATOR "DragNDrop")

    # OSX cpack info
    set(CPACK_SYSTEM_NAME  "OSX_${CMAKE_OSX_DEPLOYMENT_TARGET}_${CMAKE_OSX_ARCHITECTURES}")
    set(CPACK_BUNDLE_NAME  ${CPACK_PACKAGE_NAME})
    set(CPACK_BUNDLE_ICON  ${MACOSX_BUNDLE_ICON_FILE})
    set(CPACK_BUNDLE_PLIST ${MACOSX_BUNDLE_PLIST_FILE})

    set(APPS "\${CMAKE_INSTALL_PREFIX}/vimix.app")
    #    set( APPS "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/vimix${CMAKE_EXECUTABLE_SUFFIX}")

    install(TARGETS ${VMIX_BINARY}
            CONFIGURATIONS Release RelWithDebInfo
            BUNDLE  DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

    # create GST plugins directory in Bundle Resources subfolder
    set(plugin_dest_dir vimix.app/Contents/Resources/)

    # intall the gst-plugin-scanner program (used by plugins at load time)
    install(FILES "/usr/local/Cellar/gstreamer/1.18.0/libexec/gstreamer-1.0/gst-plugin-scanner"
        DESTINATION "${plugin_dest_dir}"
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT Runtime
    )

    # ICU DATA LIB GST dependency : undocumented and hacked here : seems to work
    install(FILES "/usr/local/Cellar/icu4c/67.1/lib/libicudata.67.1.dylib" DESTINATION "${plugin_dest_dir}/gstreamer-1.0" RENAME "libicudata.67.dylib" COMPONENT Runtime)

    # Install the gst-plugins (all those installed with brew )
    install(DIRECTORY "${PKG_GSTREAMER_PLUGIN_DIR}" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
    install(DIRECTORY "/usr/local/Cellar/gst-plugins-base/1.18.0/lib/gstreamer-1.0" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
    install(DIRECTORY "/usr/local/Cellar/gst-plugins-good/1.18.0/lib/gstreamer-1.0" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
#    install(DIRECTORY "/usr/local/Cellar/gst-plugins-bad/1.18.0/lib/gstreamer-1.0" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
    install(DIRECTORY "/usr/local/Cellar/gst-plugins-ugly/1.18.0/lib/gstreamer-1.0" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
    install(DIRECTORY "/usr/local/Cellar/gst-libav/1.18.0/lib/gstreamer-1.0" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)

    # install locally recompiled gst-plugins-bad
    install(FILES "/Users/herbelin/Development/gst/gst-plugins-bad-1.16.2/build/ext/libde265/libgstde265.dylib" DESTINATION "${plugin_dest_dir}/gstreamer-1.0" COMPONENT Runtime)
    install(FILES "/Users/herbelin/Development/gst/gst-plugins-bad-1.16.2/build/ext/x265/libgstx265.dylib" DESTINATION "${plugin_dest_dir}/gstreamer-1.0" COMPONENT Runtime)
    install(FILES "/Users/herbelin/Development/gst/gst-plugins-bad-1.16.2/build/sys/applemedia/libgstapplemedia.dylib" DESTINATION "${plugin_dest_dir}/gstreamer-1.0" COMPONENT Runtime)
    install(FILES "/Users/herbelin/Development/gst/gst-plugins-bad-1.16.2/build/sys/decklink/libgstdecklink.dylib" DESTINATION "${plugin_dest_dir}/gstreamer-1.0" COMPONENT Runtime)
    install(FILES "/Users/herbelin/Development/gst/gst-plugins-bad-1.16.2/build/ext/aom/libgstaom.dylib" DESTINATION "${plugin_dest_dir}/gstreamer-1.0" COMPONENT Runtime)



    # package runtime fixup bundle
    install(CODE "
            file(GLOB_RECURSE GSTPLUGINS \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/gstreamer-1.0/*.dylib\")
            list(APPEND LIBS_PATH \"/usr/local/Cellar/icu4c/67.1/lib\")
            include(BundleUtilities)
            set(BU_CHMOD_BUNDLE_ITEMS TRUE)
            fixup_bundle(\"${APPS}\" \"\${GSTPLUGINS}\" \"${LIBS_PATH}\")
            "
            COMPONENT Runtime
    )

    set(CPACK_BINARY_DRAGNDROP ON)

ELSE(APPLE)

    set(APPS "\${CMAKE_INSTALL_PREFIX}/bin/vimix")

    install(TARGETS ${VMIX_BINARY}
            CONFIGURATIONS Release RelWithDebInfo
            RUNTIME DESTINATION bin COMPONENT Runtime
    )


ENDIF(APPLE)

# Package full name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}_${CPACK_SYSTEM_NAME}")

# To Create a package, run "cpack"
include(CPack)
