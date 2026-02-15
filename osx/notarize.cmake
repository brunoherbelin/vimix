#
# Notarization script for macOS DMG
# Usage: cmake -DDMG_PATH=<path> -DCODESIGN_IDENTITY=<identity> -DKEYCHAIN_PROFILE=<profile> -P notarize.cmake
#

if(NOT DMG_PATH)
    message(FATAL_ERROR "DMG_PATH not specified. Usage: cmake -DDMG_PATH=<path> -P notarize.cmake")
endif()

if(NOT CODESIGN_IDENTITY)
    message(FATAL_ERROR "CODESIGN_IDENTITY not specified.")
endif()

if(NOT KEYCHAIN_PROFILE)
    set(KEYCHAIN_PROFILE "vimix")
endif()

message("=== macOS DMG Notarization ===")
message("DMG: ${DMG_PATH}")
message("Identity: ${CODESIGN_IDENTITY}")
message("Keychain Profile: ${KEYCHAIN_PROFILE}")

# Step 1: Sign the DMG
message("")
message("Step 1: Signing DMG...")
execute_process(
    COMMAND codesign --force --sign "${CODESIGN_IDENTITY}" "${DMG_PATH}"
    RESULT_VARIABLE RESULT
    COMMAND_ECHO STDOUT
)
if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to sign DMG")
endif()

# Verify signature
message("Verifying signature...")
execute_process(
    COMMAND codesign --verify --verbose=2 "${DMG_PATH}"
    RESULT_VARIABLE RESULT
    COMMAND_ECHO STDOUT
)
if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "DMG signature verification failed")
endif()
message("DMG signed successfully.")

# Step 2: Submit to notarization
message("")
message("Step 2: Submitting to Apple notarization service (this may take several minutes)...")
execute_process(
    COMMAND xcrun notarytool submit "${DMG_PATH}" --keychain-profile "${KEYCHAIN_PROFILE}" --wait --progress
    RESULT_VARIABLE RESULT
    COMMAND_ECHO STDOUT
)
if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "Notarization submission failed")
endif()
message("Notarization completed.")

# Step 3: Staple the ticket
message("")
message("Step 3: Stapling notarization ticket to DMG...")
execute_process(
    COMMAND xcrun stapler staple "${DMG_PATH}"
    RESULT_VARIABLE RESULT
    COMMAND_ECHO STDOUT
)
if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to staple ticket to DMG")
endif()
message("Ticket stapled successfully.")

# Step 4: Final verification
message("")
message("Step 4: Verifying notarization...")
execute_process(
    COMMAND spctl -a -vv -t install "${DMG_PATH}"
    RESULT_VARIABLE RESULT
    COMMAND_ECHO STDOUT
)
if(NOT RESULT EQUAL 0)
    message(WARNING "spctl verification returned non-zero, but this may be normal for some configurations")
endif()

message("")
message("=== Notarization complete! ===")
message("DMG is ready for distribution: ${DMG_PATH}")
