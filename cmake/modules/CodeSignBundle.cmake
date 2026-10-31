#####
#####  This file is part of vimix - video live mixer
#####  **Copyright** (C) 2026 Bruno Herbelin <bruno.herbelin@gmail.com>
#####
#####  Sign a macOS application bundle with a codesign identity, resilient to
#####  offline builds. Invoked with
#####      cmake -DBUNDLE=... -DIDENTITY=... -P CodeSignBundle.cmake
#####
#####  Signing with a Developer ID identity requests a secure timestamp from
#####  Apple's server (http://timestamp.apple.com), which fails without
#####  internet access. When the timestamp server cannot be reached, sign with
#####  --timestamp=none instead: the signature still gives macOS a stable
#####  application identity (so the application keeps its granted permissions
#####  across rebuilds); only notarization would require a timestamped
#####  signature.
#####

if(NOT DEFINED BUNDLE OR NOT DEFINED IDENTITY)
    message(FATAL_ERROR "codesign: BUNDLE and IDENTITY shall be given.")
endif()

# is Apple's secure timestamp server reachable?
set(TIMESTAMP_SERVER_ONLINE FALSE)
find_program(CURL_EXECUTABLE NAMES curl)
if(CURL_EXECUTABLE)
    execute_process(
        COMMAND ${CURL_EXECUTABLE} --silent --head --max-time 3 http://timestamp.apple.com
        RESULT_VARIABLE CURL_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(CURL_RESULT EQUAL 0)
        set(TIMESTAMP_SERVER_ONLINE TRUE)
    endif()
endif()

set(RESULT 1)
if(TIMESTAMP_SERVER_ONLINE)
    message(STATUS "Signing ${BUNDLE} with ${IDENTITY}")
    execute_process(
        COMMAND codesign --force --sign "${IDENTITY}" "${BUNDLE}"
        RESULT_VARIABLE RESULT
    )
    if(NOT RESULT EQUAL 0)
        # the probe can be fooled (e.g. captive portal): sign without timestamp
        message(STATUS "Signing with secure timestamp failed; signing without secure timestamp")
        execute_process(
            COMMAND codesign --force --sign "${IDENTITY}" --timestamp=none "${BUNDLE}"
            RESULT_VARIABLE RESULT
        )
    endif()
else()
    message(STATUS "No internet access: signing ${BUNDLE} with ${IDENTITY} without secure timestamp")
    execute_process(
        COMMAND codesign --force --sign "${IDENTITY}" --timestamp=none "${BUNDLE}"
        RESULT_VARIABLE RESULT
    )
endif()

if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "codesign: failed to sign ${BUNDLE}")
endif()
