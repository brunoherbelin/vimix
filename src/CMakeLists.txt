#####
#####  This file is part of vimix - video live mixer
#####  **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
#####

cmake_minimum_required(VERSION 3.10)
include( InstallRequiredSystemLibraries )

#####
##### VIMIX TARGET BINARY
#####

set(VMIX_BINARY "vimix")
set(VMIX_SRCS
    # Main source files
    main.cpp
    Log.cpp
    ActionManager.cpp
    Audio.cpp
    Connection.cpp
    ControlManager.cpp
    FrameBuffer.cpp
    FrameGrabber.cpp
    Grid.cpp
    Interpolator.cpp
    Loopback.cpp
    MediaPlayer.cpp
    Metronome.cpp
    Mixer.cpp
    MixingGroup.cpp
    MousePointer.cpp
    MultiFileRecorder.cpp
    Overlay.cpp
    Playlist.cpp
    Recorder.cpp
    RenderingManager.cpp
    Resource.cpp
    Screenshot.cpp
    Selection.cpp
    Session.cpp
    SessionCreator.cpp
    SessionParser.cpp
    Settings.cpp
    Shader.cpp
    ImageShader.cpp
    ImageProcessingShader.cpp
    ShmdataBroadcast.cpp
    Stream.cpp
    Streamer.cpp
    TabletInput.cpp
    Timeline.cpp
    Transcoder.cpp
    UpdateCallback.cpp
    UserInterfaceManager.cpp
    VideoBroadcast.cpp
    
    # Filter/ folder
    Filter/DelayFilter.cpp
    Filter/FrameBufferFilter.cpp
    Filter/ImageFilter.cpp
    
    # Scene/ folder
    Scene/Decorations.cpp
    Scene/Mesh.cpp
    Scene/Primitives.cpp
    Scene/Scene.cpp
    
    # Source/ folder
    Source/CloneSource.cpp
    Source/DeviceSource.cpp
    Source/MediaSource.cpp
    Source/MultiFileSource.cpp
    Source/NetworkSource.cpp
    Source/PatternSource.cpp
    Source/RenderSource.cpp
    Source/ScreenCaptureSource.cpp
    Source/SessionSource.cpp
    Source/ShaderSource.cpp
    Source/Source.cpp
    Source/SourceCallback.cpp
    Source/SourceList.cpp
    Source/SrtReceiverSource.cpp
    Source/StreamSource.cpp
    Source/TextSource.cpp
    
    # Toolkit/ folder
    Toolkit/BaseToolkit.cpp
    Toolkit/DialogToolkit.cpp
    Toolkit/GlmToolkit.cpp
    Toolkit/GstToolkit.cpp
    Toolkit/ImGuiToolkit.cpp
    Toolkit/NetworkToolkit.cpp
    Toolkit/SystemToolkit.cpp
    Toolkit/tinyxml2Toolkit.cpp
    
    # View/ folder
    View/DisplaysView.cpp
    View/GeometryView.cpp
    View/LayerView.cpp
    View/MixingView.cpp
    View/RenderView.cpp
    View/TextureView.cpp
    View/TransitionView.cpp
    View/View.cpp
    
    # Visitor/ folder
    Visitor/BoundingBoxVisitor.cpp
    Visitor/CountVisitor.cpp
    Visitor/DrawVisitor.cpp
    Visitor/ImGuiVisitor.cpp
    Visitor/InfoVisitor.cpp
    Visitor/PickingVisitor.cpp
    Visitor/SearchVisitor.cpp
    Visitor/SessionVisitor.cpp
    
    # Window/ folder
    Window/InputMappingWindow.cpp
    Window/OutputPreviewWindow.cpp
    Window/ShaderEditWindow.cpp
    Window/SourceControlWindow.cpp
    Window/TimerMetronomeWindow.cpp
    Window/WorkspaceWindow.cpp
)

#####
##### DEFINE THE TARGET (OS specific)
#####

IF(APPLE)

    # Add macOS tablet input implementation
    list(APPEND VMIX_SRCS ${CMAKE_SOURCE_DIR}/osx/TabletInput_macos.mm)
    # Enable Objective-C++
    set_source_files_properties(${CMAKE_SOURCE_DIR}/osx/TabletInput_macos.mm PROPERTIES COMPILE_FLAGS "-x objective-c++")

ELSE(APPLE)

    # Add Linux tablet input implementation
    if(X11_Xi_FOUND)
        # Use X11/XInput2 version (works in Flatpak and everywhere)
        list(APPEND VMIX_SRCS TabletInput_x11.cpp)
    else()
        if(LIBINPUT_FOUND AND LIBUDEV_FOUND)
            # Use X11/XInput2 version (works in Flatpak and everywhere)
            list(APPEND VMIX_SRCS TabletInput_linux_libinput.cpp)
        endif()
    endif()

ENDIF(APPLE)

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

    # Add XInput2 library for tablet support on Linux (if available)
    if(X11_Xi_FOUND)
        list(APPEND PLATFORM_LIBS ${X11_Xi_LIB})
    endif()

    # Add libinput and libudev for tablet support on Linux (if available)
    if(LIBINPUT_FOUND AND LIBUDEV_FOUND)
        list(APPEND PLATFORM_LIBS ${LIBINPUT_LIBRARIES} ${LIBUDEV_LIBRARIES})
    endif()

ENDIF(APPLE)

#####
##### COMPILE THE TARGET (all OS)
#####

set_property(TARGET ${VMIX_BINARY} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${VMIX_BINARY} PROPERTY C_STANDARD 11)

# Add src directory to include path so Visitor subfolder files can include without ../
target_include_directories(${VMIX_BINARY} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

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
    ${GSTREAMER_LIBRARIES}
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

    ##
    ## APPLE BUNDLE FIX
    ## with install_name_tool using cmake 'fixup_bundle'
    ##
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

    ##
    ## APPLE CODE SIGNING
    ## Sign the bundle and ALL binary (executables and dynamic library)
    ##
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
    endif()

ELSE(APPLE)

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/share DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES "${CMAKE_SOURCE_DIR}/rsc/launch_vimix.sh"
        DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT Runtime
    )

    install(TARGETS ${VMIX_BINARY}
            CONFIGURATIONS Release RelWithDebInfo
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

ENDIF(APPLE)
