#####
#####  This file is part of vimix - video live mixer
#####  **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
#####

cmake_minimum_required(VERSION 3.8.2)
include( InstallRequiredSystemLibraries )

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
    TextSource.cpp
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
    set_target_properties(${VMIX_BINARY} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_PLIST_FILE}
    )

    # link with base frameworks
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

    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_BINARY_DRAGNDROP ON)

    set(CPACK_BUNDLE_NAME "vimix.app")
    set(CPACK_BUNDLE_PLIST ${CMAKE_SOURCE_DIR}/osx/Info.plist.in)
    set(CPACK_BUNDLE_ICON  ${CMAKE_SOURCE_DIR}/osx/vimix.icns)
    set(CPACK_BUNDLE_APPLE_CERT_APP ${APPLE_CODESIGN_IDENTITY})
    set(CPACK_BUNDLE_APPLE_ENTITLEMENTS ${APPLE_CODESIGN_ENTITLEMENTS})

    install(TARGETS ${VMIX_BINARY}
            CONFIGURATIONS Release RelWithDebInfo
            BUNDLE  DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

    # create GST plugins directory in Bundle Resources subfolder
    set(plugin_dest_dir ${CPACK_BUNDLE_NAME}/Contents/Resources/)

    ### install gst plugins
    message(STATUS "Install with gst-plugins ${PKG_GSTREAMER_PLUGIN_DIR}")
    install(DIRECTORY "${PKG_GSTREAMER_PLUGIN_DIR}" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)
    list(APPEND GSTPLUGINS_EXCLUDE "libgstgtk4.dylib" "libgstgtk.dylib" "libgstpython.dylib")

    # install the gst-plugin-scanner program (used by plugins at load time)
    set(PKG_GSTREAMER_SCANNER "${PKG_GSTREAMER_PREFIX}/libexec/gstreamer-1.0/gst-plugin-scanner")
    message(STATUS "Install with gst-plugin-scanner ${PKG_GSTREAMER_SCANNER}")
    install(FILES "${PKG_GSTREAMER_SCANNER}"
        DESTINATION "${plugin_dest_dir}"
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT Runtime
    )

    # install frei0r plugins (dependencies of gstreamer-1.0/libgstfrei0r.dylib plugin)
    pkg_check_modules(FREI0R QUIET frei0r)
    set(PKG_FREI0R_PLUGIN_DIR ${FREI0R_LIBDIR}/frei0r-1)
    message(STATUS "Install with frei0r plugins ${PKG_FREI0R_PLUGIN_DIR}")
    install(DIRECTORY "${PKG_FREI0R_PLUGIN_DIR}" DESTINATION "${plugin_dest_dir}" COMPONENT Runtime)

    # package runtime fixup bundle
    set(APPS "\${CMAKE_INSTALL_PREFIX}/${CPACK_BUNDLE_NAME}")

    ## APPLE BUNDLE FIX
    ## with install_name_tool using cmake 'fixup_bundle'
    install(CODE "
        file(GLOB_RECURSE GSTPLUGINS \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/gstreamer-1.0/*.dylib\")
        foreach(PLUGIN \${GSTPLUGINS})
            set(ADD_PLUGIN true)
            foreach(EXCLUDED ${GSTPLUGINS_EXCLUDE})
                string(FIND \${PLUGIN} \${EXCLUDED} _found)
                if(NOT \${_found} EQUAL -1)
                    file(REMOVE \${PLUGIN})
                    set(ADD_PLUGIN false)
                endif()
            endforeach()
            if(ADD_PLUGIN)
                list(APPEND ALLPLUGINS \${PLUGIN})
            endif()
        endforeach()
        file(GLOB FREI0RPLUGINS \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/frei0r-1/*.so\")
        list(APPEND ALLPLUGINS \${FREI0RPLUGINS} )
        list(LENGTH ALLPLUGINS SIZE)
        message(\"       - Adding \${SIZE} plugins to bundle...\")
        include(BundleUtilities)
        set(BU_CHMOD_BUNDLE_ITEMS TRUE)
        fixup_bundle(\"${APPS}\" \"\${ALLPLUGINS}\" \"/opt/homebrew/lib\" IGNORE_ITEM \"Python\")
        file(REMOVE_RECURSE \"${APPS}/Contents/Frameworks/Python.framework\")
        "
        COMPONENT Runtime
    )

    ## APPLE CODE SIGNING
    ## Sign the bundle and ALL binary (executables and dynamic library)
    string(LENGTH "${APPLE_CODESIGN_IDENTITY}" APPLE_CODESIGN_IDENTITY_LENGHT)
    if( ${APPLE_CODESIGN_IDENTITY_LENGHT} LESS 40 )
        message(STATUS "Not signing bundle. Specify APPLE_CODESIGN_IDENTITY to cmake before running cpack if you want to sign the bundle")
    else()
        message(STATUS "Signing bundle with identity " ${APPLE_CODESIGN_IDENTITY})
        install(CODE "
                cmake_policy(VERSION 3.8)
                file(GLOB_RECURSE DEPENDENCIES ${APPS}/Contents/*.dylib)
                file(GLOB_RECURSE SO_DEPENDENCIES ${APPS}/Contents/*.so)
                list(APPEND DEPENDENCIES \${SO_DEPENDENCIES})
                foreach(DEP \${DEPENDENCIES})
                    execute_process(COMMAND
                        codesign --verify --verbose=0 --force
                        --sign \"${APPLE_CODESIGN_IDENTITY}\"
                        \"\${DEP}\"
                    )
                endforeach()
                list(LENGTH DEPENDENCIES SIZE)
                message(\" - \${SIZE} dependency libraries signed.\")
                message(\" - Signing executables in ${APPS}\")
                execute_process(COMMAND
                    codesign -vvv --force
                    --options runtime --timestamp
                    --entitlements \"${APPLE_CODESIGN_ENTITLEMENTS}\"
                    --sign \"${APPLE_CODESIGN_IDENTITY}\"
                    \"${APPS}/Contents/MacOS/vimix\"
                    \"${APPS}/Contents/Resources/gst-plugin-scanner\"
                )
                message(\" - Signing bundle ${APPS}\")
                execute_process(COMMAND
                    codesign -vvv --force --strict
                    --options runtime --timestamp
                    --entitlements \"${APPLE_CODESIGN_ENTITLEMENTS}\"
                    --sign \"${APPLE_CODESIGN_IDENTITY}\"
                    \"${APPS}\"
                )
                "
                COMPONENT Runtime
        )
        # install(CODE "
        #         include(BundleUtilities)
        #         get_bundle_all_executables(\"${APPS}\" EXECUTABLES)
        #         foreach(EXE \${EXECUTABLES})
        #             message(\"SIGNING EXECUTABLE \${EXE}\")
        #             execute_process(COMMAND
        #             codesign --verbose=2 --force
        #             --sign \"${APPLE_CODESIGN_IDENTITY}\"
        #             \"\${EXE}\"
        #             )
        #         endforeach()
        #         "
        #         COMPONENT Runtime
        # )
        # install(CODE "
        #         execute_process(COMMAND
        #         codesign --verbose=2 --force
        #         --sign \"${APPLE_CODESIGN_IDENTITY}\"
        #         \"${DMGS}\"
        #         )
        #         message(\"codesign ${DMGS} \")
        #         "
        #         COMPONENT Runtime
        # )
        # install(CODE "
        #         execute_process(COMMAND
        #         xcrun notarytool submit \"${DMGS}\"
        #         --keychain-profile \"${APPLE_CODESIGN_STORED_CREDENTIAL}\"
        #         --wait
        #         )
        #         message(\"notary tool ${DMGS}  ${APPLE_CODESIGN_STORED_CREDENTIAL} \")
        #         "
        #         COMPONENT Runtime
        # )
    endif()

   # install (SCRIPT "${CMAKE_SOURCE_DIR}/osx/PostInstall.cmake")


ELSE(APPLE)

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/share DESTINATION ${CMAKE_INSTALL_PREFIX})

    install(TARGETS ${VMIX_BINARY}
            CONFIGURATIONS Release RelWithDebInfo
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

ENDIF(APPLE)
