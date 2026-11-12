# Reusable iOS app bundle and IPA packaging helper.
#
# Every iOS app in this repository goes through nw_ios_add_app() so the
# bundle layout, Info.plist, resource copying, and IPA packaging stay in one
# place.

include_guard(GLOBAL)

find_program(NW_IOS_ZIP NAMES zip REQUIRED)

# Optional codesigning.  NW_IOS_SIGN_MODE selecting anything other than "none"
# requires rcodesign (apple-codesign).  The default image ships it; when it is
# missing the configure step stops with a message that points at
# NW_IOS_RCODESIGN instead of producing an app that looks signed but is not.
set(NW_IOS_SIGN_MODE "none" CACHE STRING
    "iOS signing mode: none, adhoc, or apple")
set_property(CACHE NW_IOS_SIGN_MODE PROPERTY STRINGS none adhoc apple)
set(NW_IOS_SIGN_P12 "" CACHE FILEPATH
    "PKCS#12 certificate/key for NW_IOS_SIGN_MODE=apple")
set(NW_IOS_SIGN_P12_PASSWORD_FILE "" CACHE FILEPATH
    "File containing the PKCS#12 password (optional)")
set(NW_IOS_SIGN_ENTITLEMENTS "" CACHE FILEPATH
    "Entitlements plist embedded in the signature (optional)")
set(NW_IOS_SIGN_PROVISIONING_PROFILE "" CACHE FILEPATH
    "Provisioning profile copied to embedded.mobileprovision (optional)")
set(NW_IOS_SIGN_TEAM_ID "" CACHE STRING
    "Team identifier to embed in the signature (optional)")
set(NW_IOS_RCODESIGN "" CACHE FILEPATH
    "Explicit path to rcodesign (default: search PATH)")
set(NW_IOS_SIGN_IDENTITY "" CACHE STRING
    "Bundle identifier for signing; defaults to the app's BUNDLE_ID")

# Append the signing commands to a target that has already been fully built
# (PostBuild runs after every resource copy).  Ad-hoc signing still produces an
# installable-on-jailbreak app; the apple mode needs a real certificate and,
# for a stock device, a matching provisioning profile.
function(nw_ios_sign_app target app_dir bundle_id)
    if(NW_IOS_SIGN_MODE STREQUAL "none")
        return()
    endif()

    if(NW_IOS_SIGN_MODE STREQUAL "adhoc")
        set(_cert_args)
    elseif(NW_IOS_SIGN_MODE STREQUAL "apple")
        if(NOT NW_IOS_SIGN_P12 OR NOT EXISTS "${NW_IOS_SIGN_P12}")
            message(FATAL_ERROR
                "NW_IOS_SIGN_MODE=apple needs NW_IOS_SIGN_P12 pointing at "
                "a PKCS#12 certificate")
        endif()
        set(_cert_args --p12-file "${NW_IOS_SIGN_P12}")
        if(NW_IOS_SIGN_P12_PASSWORD_FILE)
            if(NOT EXISTS "${NW_IOS_SIGN_P12_PASSWORD_FILE}")
                message(FATAL_ERROR
                    "NW_IOS_SIGN_P12_PASSWORD_FILE not found: "
                    "${NW_IOS_SIGN_P12_PASSWORD_FILE}")
            endif()
            list(APPEND _cert_args
                --p12-password-file "${NW_IOS_SIGN_P12_PASSWORD_FILE}")
        endif()
        if(NW_IOS_SIGN_TEAM_ID)
            list(APPEND _cert_args --team-name "${NW_IOS_SIGN_TEAM_ID}")
        endif()
    else()
        message(FATAL_ERROR
            "NW_IOS_SIGN_MODE must be none, adhoc, or apple; got "
            "'${NW_IOS_SIGN_MODE}'")
    endif()

    if(NW_IOS_RCODESIGN)
        set(_rcodesign "${NW_IOS_RCODESIGN}")
    else()
        # Use a fresh variable name: find_program does not search when its
        # result variable is already defined in the current scope.
        find_program(_nw_ios_rcodesign_auto NAMES rcodesign)
        set(_rcodesign "${_nw_ios_rcodesign_auto}")
    endif()
    if(NOT _rcodesign OR NOT EXISTS "${_rcodesign}")
        message(FATAL_ERROR
            "NW_IOS_SIGN_MODE=${NW_IOS_SIGN_MODE} needs rcodesign.  Install "
            "apple-codesign in the image or set NW_IOS_RCODESIGN to its path.")
    endif()

    set(_bundle_id "${bundle_id}")
    if(NW_IOS_SIGN_IDENTITY)
        set(_bundle_id "${NW_IOS_SIGN_IDENTITY}")
    endif()

    set(_extra_args)
    set(_profile_entitlements_cmd)
    if(NW_IOS_SIGN_ENTITLEMENTS)
        if(NOT EXISTS "${NW_IOS_SIGN_ENTITLEMENTS}")
            message(FATAL_ERROR
                "NW_IOS_SIGN_ENTITLEMENTS not found: "
                "${NW_IOS_SIGN_ENTITLEMENTS}")
        endif()
        list(APPEND _extra_args --entitlements-xml-file
            "${NW_IOS_SIGN_ENTITLEMENTS}")
    elseif(NW_IOS_SIGN_PROVISIONING_PROFILE)
        if(NOT EXISTS "${NW_IOS_SIGN_PROVISIONING_PROFILE}")
            message(FATAL_ERROR
                "NW_IOS_SIGN_PROVISIONING_PROFILE not found: "
                "${NW_IOS_SIGN_PROVISIONING_PROFILE}")
        endif()
        # A development profile carries the entitlements the device expects
        # (application-identifier, team identifier, get-task-allow).  Derive
        # them with the helper instead of requiring the caller to write a
        # parallel plist that silently drifts from the profile.
        if(NOT Python3_EXECUTABLE)
            message(FATAL_ERROR
                "Deriving entitlements from "
                "NW_IOS_SIGN_PROVISIONING_PROFILE needs a Python interpreter")
        endif()
        set(_profile_tool "${NW_ROOT}/tools/mobileprovision.py")
        if(NOT EXISTS "${_profile_tool}")
            message(FATAL_ERROR
                "Missing provisioning-profile helper: ${_profile_tool}")
        endif()
        set(_profile_entitlements
            "${CMAKE_BINARY_DIR}/ios-signing/${target}-entitlements.plist")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ios-signing")
        set(_profile_entitlements_cmd
            COMMAND "${Python3_EXECUTABLE}" "${_profile_tool}"
                    --input "${NW_IOS_SIGN_PROVISIONING_PROFILE}"
                    --entitlements-out "${_profile_entitlements}"
                    --bundle-id "${_bundle_id}")
        list(APPEND _extra_args --entitlements-xml-file
            "${_profile_entitlements}")
    endif()

    if(NW_IOS_SIGN_IDENTITY)
        list(APPEND _extra_args --binary-identifier "${_bundle_id}")
    endif()

    set(_profile_copy)
    if(NW_IOS_SIGN_PROVISIONING_PROFILE)
        if(NOT EXISTS "${NW_IOS_SIGN_PROVISIONING_PROFILE}")
            message(FATAL_ERROR
                "NW_IOS_SIGN_PROVISIONING_PROFILE not found: "
                "${NW_IOS_SIGN_PROVISIONING_PROFILE}")
        endif()
        set(_profile_copy
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${NW_IOS_SIGN_PROVISIONING_PROFILE}"
                    "${app_dir}/embedded.mobileprovision")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        ${_profile_copy}
        ${_profile_entitlements_cmd}
        COMMAND "${_rcodesign}" sign
                ${_cert_args} ${_extra_args}
                "${app_dir}"
        COMMENT "Signing ${app_dir} (${NW_IOS_SIGN_MODE})"
        VERBATIM)
endfunction()

# nw_ios_add_app(<target>
#     BUNDLE_ID <id> BUNDLE_NAME <name> DISPLAY_NAME <name>
#     EXECUTABLE <file> VERSION <v> BUILD_NUMBER <n>
#     [ICON_SOURCE <png>] [RESOURCES <file>...])
function(nw_ios_add_app target)
    cmake_parse_arguments(ARG ""
        "BUNDLE_ID;BUNDLE_NAME;DISPLAY_NAME;EXECUTABLE;VERSION;BUILD_NUMBER;ICON_SOURCE"
        "RESOURCES"
        ${ARGN})

    if(NOT ARG_BUNDLE_ID)
        message(FATAL_ERROR "nw_ios_add_app(${target}): BUNDLE_ID is required")
    endif()
    if(NOT ARG_BUNDLE_NAME)
        set(ARG_BUNDLE_NAME "${target}")
    endif()
    if(NOT ARG_DISPLAY_NAME)
        set(ARG_DISPLAY_NAME "${ARG_BUNDLE_NAME}")
    endif()
    if(NOT ARG_EXECUTABLE)
        set(ARG_EXECUTABLE "${target}")
    endif()
    if(NOT ARG_VERSION)
        set(ARG_VERSION "0.1.0")
    endif()
    if(NOT ARG_BUILD_NUMBER)
        set(ARG_BUILD_NUMBER "1")
    endif()

    set(app_dir "${CMAKE_BINARY_DIR}/${ARG_BUNDLE_NAME}.app")
    file(MAKE_DIRECTORY "${app_dir}")

    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${ARG_EXECUTABLE}"
        RUNTIME_OUTPUT_DIRECTORY "${app_dir}"
        MACOSX_BUNDLE FALSE
        SUFFIX ""
    )

    # Plain PNG icons are enough for development sideloading.  A future App
    # Store pipeline can add an asset catalog without changing this interface.
    set(NW_IOS_ICON_PLIST "")
    if(ARG_ICON_SOURCE)
        if(NOT IS_ABSOLUTE "${ARG_ICON_SOURCE}")
            set(_icon_source "${NW_ROOT}/${ARG_ICON_SOURCE}")
        else()
            set(_icon_source "${ARG_ICON_SOURCE}")
        endif()
        if(EXISTS "${_icon_source}")
            set(_icon_tool "${NW_ROOT}/tools/gen_ios_icons.py")
            if(EXISTS "${_icon_tool}" AND Python3_EXECUTABLE)
                execute_process(
                    COMMAND "${Python3_EXECUTABLE}" "${_icon_tool}"
                            --source "${_icon_source}" --out "${app_dir}"
                    RESULT_VARIABLE _icon_result
                    OUTPUT_QUIET
                    ERROR_VARIABLE _icon_error)
                if(NOT _icon_result EQUAL 0)
                    message(WARNING
                        "Could not generate iOS icons: ${_icon_error}")
                else()
                    set(NW_IOS_ICON_PLIST
"    <key>CFBundleIcons</key>
    <dict>
        <key>CFBundlePrimaryIcon</key>
        <dict>
            <key>CFBundleIconFiles</key>
            <array>
                <string>AppIcon60x60</string>
            </array>
            <key>CFBundleIconName</key>
            <string>AppIcon</string>
        </dict>
    </dict>
    <key>CFBundleIcons~ipad</key>
    <dict>
        <key>CFBundlePrimaryIcon</key>
        <dict>
            <key>CFBundleIconFiles</key>
            <array>
                <string>AppIcon76x76</string>
                <string>AppIcon83.5x83.5</string>
            </array>
            <key>CFBundleIconName</key>
            <string>AppIcon</string>
        </dict>
    </dict>")
                endif()
            else()
                message(WARNING "iOS icon generator or Python interpreter missing")
            endif()
        else()
            message(WARNING "iOS icon source not found: ${_icon_source}")
        endif()
    endif()

    set(NW_IOS_BUNDLE_ID "${ARG_BUNDLE_ID}")
    set(NW_IOS_BUNDLE_NAME "${ARG_BUNDLE_NAME}")
    set(NW_IOS_DISPLAY_NAME "${ARG_DISPLAY_NAME}")
    set(NW_IOS_EXECUTABLE "${ARG_EXECUTABLE}")
    set(NW_IOS_VERSION "${ARG_VERSION}")
    set(NW_IOS_BUILD_NUMBER "${ARG_BUILD_NUMBER}")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Info.plist.in"
                   "${app_dir}/Info.plist" @ONLY)
    file(WRITE "${app_dir}/PkgInfo" "APPL????")

    set(_resource_sources)
    foreach(resource IN LISTS ARG_RESOURCES)
        if(IS_ABSOLUTE "${resource}")
            set(_resource_source "${resource}")
        else()
            set(_resource_source "${NW_ROOT}/${resource}")
        endif()
        if(EXISTS "${_resource_source}")
            list(APPEND _resource_sources "${_resource_source}")
            get_filename_component(_resource_name "${_resource_source}" NAME)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${_resource_source}"
                        "${app_dir}/${_resource_name}"
                COMMENT "Copying iOS resource ${_resource_name}")
        else()
            message(WARNING
                "iOS resource not found, skipping: ${_resource_source}")
        endif()
    endforeach()

    add_custom_target(${target}-app DEPENDS ${target}
        COMMENT "iOS app bundle ready: ${app_dir}")

    nw_ios_sign_app(${target} "${app_dir}" "${ARG_BUNDLE_ID}")

    set(_ipa "${CMAKE_BINARY_DIR}/${ARG_BUNDLE_NAME}.ipa")
    set(_stage "${CMAKE_BINARY_DIR}/ipa-${target}")
    # The custom target's working directory must exist before Ninja emits its
    # leading `cd`; the recipe removes and recreates it.
    file(MAKE_DIRECTORY "${_stage}")
    add_custom_target(${target}-ipa
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_stage}/Payload"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${_stage}/Payload"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${app_dir}" "${_stage}/Payload/${ARG_BUNDLE_NAME}.app"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${_ipa}"
        COMMAND "${NW_IOS_ZIP}" -r -X "${_ipa}" "Payload"
        WORKING_DIRECTORY "${_stage}"
        DEPENDS ${target}-app
        BYPRODUCTS "${_ipa}"
        COMMENT "Packaging ${_ipa}")

    set(NW_IOS_APP_DIR "${app_dir}" PARENT_SCOPE)
    set(NW_IOS_IPA_PATH "${_ipa}" PARENT_SCOPE)
    message(STATUS "iOS app ${target}: ${app_dir}")
    message(STATUS "iOS IPA ${target}: ${_ipa}")
endfunction()
