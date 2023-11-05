#####
#####  This file is part of vimix - video live mixer
#####  **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
#####

cmake_minimum_required(VERSION 3.8.2)

#####
##### VIMIX TARGET BINARY
#####

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
    FrameBufferFilter.cpp
    ImageFilter.cpp
    DelayFilter.cpp
    RenderingManager.cpp
    UserInterfaceManager.cpp
    WorkspaceWindow.cpp
    SourceControlWindow.cpp
    OutputPreviewWindow.cpp
    TimerMetronomeWindow.cpp
    InputMappingWindow.cpp
    ShaderEditWindow.cpp
    PickingVisitor.cpp
    BoundingBoxVisitor.cpp
    DrawVisitor.cpp
    SearchVisitor.cpp
    ImGuiToolkit.cpp
    ImGuiVisitor.cpp
    InfoVisitor.cpp
    CountVisitor.cpp
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
    ShmdataBroadcast.cpp
    SrtReceiverSource.cpp
    MultiFileRecorder.cpp
    DisplaysView.cpp
    ScreenCaptureSource.cpp
    MousePointer.cpp
    Grid.cpp
    Playlist.cpp
    Audio.cpp
)

#####
##### DEFINE THE TARGET (OS specific)
#####

IF(APPLE)

    # set icon
    set(MACOSX_BUNDLE_ICON vimix.icns)
    set(MACOSX_BUNDLE_ICON_FILE ${CMAKE_SOURCE_DIR}/osx/${MACOSX_BUNDLE_ICON})
    # set where in the bundle to put the icns file
    set_source_files_properties(${MACOSX_BUNDLE_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # create the application
    add_executable(${VMIX_BINARY} MACOSX_BUNDLE
        ${VMIX_SRCS}
        ${MACOSX_BUNDLE_ICON_FILE}
    )

    # set the Info.plist file
    set(MACOSX_BUNDLE_PLIST_FILE ${CMAKE_SOURCE_DIR}/osx/Info.plist.in)
    set_target_properties(${VMIX_BINARY} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_PLIST_FILE})

    set(PLATFORM_LIBS
        "-framework CoreFoundation"
        "-framework Appkit"
    )

ELSE(APPLE)

    link_directories (${GTK3_LIBRARY_DIRS})

    add_executable(${VMIX_BINARY}
        ${VMIX_SRCS}
    )

    set(PLATFORM_LIBS
        GTK::GTK
        X11::X11
        X11::xcb
    )

ENDIF(APPLE)

#####
##### COMPILE THE TARGET (all OS)
#####

set_property(TARGET ${VMIX_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${VMIX_BINARY} PROPERTY C_STANDARD 11)

target_compile_definitions(${VMIX_BINARY} PUBLIC "IMGUI_IMPL_OPENGL_LOADER_GLAD")

target_link_libraries(${VMIX_BINARY} LINK_PRIVATE
    ${PLATFORM_LIBS}
    ${GLM_LIBRARIES}
    ${GLAD_LIBRARIES}
    ${OSCPACK_LIBRARIES}
    ${IMGUI_LIBRARIES}
    ${TINYFD_LIBRARIES}
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
    ZLIB::ZLIB
    Ableton::Link
    vmix::rc
)

#####
##### DEFINE THE APPLICATION PACKAGING (OS specific)
#####

IF(APPLE)

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

    set(APPLE_CODESIGN_IDENTITY "" CACHE STRING "")
    string(LENGTH "${APPLE_CODESIGN_IDENTITY}" APPLE_CODESIGN_IDENTITY_LENGHT)
    if( ${APPLE_CODESIGN_IDENTITY_LENGHT} LESS 40 )
        message(STATUS "Not signing bundle. Specify APPLE_CODESIGN_IDENTITY to cmake before running cpack to sign")
    else()
        install(CODE "
                execute_process(COMMAND
                codesign -vvv --deep --force
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

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/share DESTINATION ${CMAKE_INSTALL_PREFIX})

    install(TARGETS ${VMIX_BINARY}
            CONFIGURATIONS Release RelWithDebInfo
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

ENDIF(APPLE)
