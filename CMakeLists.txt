cmake_minimum_required(VERSION 3.25)
project(XenSequencer VERSION 0.3.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles([=[
    #include <version>

    #if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
    #error "std::expected is required"
    #endif
    #if !defined(__cpp_lib_print) || __cpp_lib_print < 202207L
    #error "std::print is required"
    #endif
    #if !defined(__cpp_lib_ranges_contains) || __cpp_lib_ranges_contains < 202207L
    #error "std::ranges::contains is required"
    #endif
    #if !defined(__cpp_lib_ranges_to_container) || __cpp_lib_ranges_to_container < 202202L
    #error "std::ranges::to is required"
    #endif
    #if !defined(__cpp_lib_ranges_zip) || __cpp_lib_ranges_zip < 202110L
    #error "C++23 range zip and enumerate views are required"
    #endif
    #if !defined(__cpp_lib_to_underlying) || __cpp_lib_to_underlying < 202102L
    #error "std::to_underlying is required"
    #endif

    int main() {}
]=] XEN_HAS_REQUIRED_CXX23_LIBRARY_FEATURES_V2)
if (NOT XEN_HAS_REQUIRED_CXX23_LIBRARY_FEATURES_V2)
    message(FATAL_ERROR
        "XenSequencer requires a C++23 standard library with std::expected, "
        "std::print, C++23 ranges, and std::to_underlying support.")
endif()

include(FetchContent)

# DEPENDENCIES -------------------------------------------------------------------------

add_subdirectory(external/MicrotonalStepSequencer SYSTEM)

option(XEN_BUILD_TESTS "Build XenSequencer tests" ON)
option(XEN_BUILD_TOOLS "Build XenSequencer tools" ON)
option(XEN_BUILD_AUDIO_PLUGIN_HOST "Build JUCE AudioPluginHost target" ON)

set(XEN_WEBVIEW_HARDWARE_ACCELERATION_POLICY "on-demand" CACHE STRING
    "Linux WebKitGTK hardware acceleration policy: on-demand, always, or never")
set_property(CACHE XEN_WEBVIEW_HARDWARE_ACCELERATION_POLICY PROPERTY STRINGS
    on-demand always never)

if (XEN_WEBVIEW_HARDWARE_ACCELERATION_POLICY STREQUAL "on-demand")
    set(XEN_JUCE_WEBKIT_HARDWARE_ACCELERATION_POLICY
        JUCE_WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND)
elseif (XEN_WEBVIEW_HARDWARE_ACCELERATION_POLICY STREQUAL "always")
    set(XEN_JUCE_WEBKIT_HARDWARE_ACCELERATION_POLICY
        JUCE_WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS)
elseif (XEN_WEBVIEW_HARDWARE_ACCELERATION_POLICY STREQUAL "never")
    set(XEN_JUCE_WEBKIT_HARDWARE_ACCELERATION_POLICY
        JUCE_WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER)
else()
    message(FATAL_ERROR
        "Invalid XEN_WEBVIEW_HARDWARE_ACCELERATION_POLICY="
        "'${XEN_WEBVIEW_HARDWARE_ACCELERATION_POLICY}'. Expected on-demand, always, or never.")
endif()

set(XEN_WEB_UI_MODE "DEV_SERVER" CACHE STRING
    "Web UI source mode: DEV_SERVER or EMBEDDED")
set_property(CACHE XEN_WEB_UI_MODE PROPERTY STRINGS DEV_SERVER EMBEDDED)
set(XEN_WEB_UI_DEV_URL "http://127.0.0.1:5173,http://localhost:5173" CACHE STRING
    "Comma-separated ordered dev-server URLs when XEN_WEB_UI_MODE=DEV_SERVER")
set(XEN_WEB_UI_DIST_DIR "${CMAKE_SOURCE_DIR}/../xen-frontend/dist" CACHE PATH
    "Path to frontend dist/ directory when XEN_WEB_UI_MODE=EMBEDDED")

if (CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR
        "XenSequencer Web UI build currently supports single-config generators only.")
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set. Defaulting to Debug.")
endif()

if (NOT XEN_WEB_UI_MODE STREQUAL "DEV_SERVER" AND
    NOT XEN_WEB_UI_MODE STREQUAL "EMBEDDED")
    message(FATAL_ERROR
        "Invalid XEN_WEB_UI_MODE='${XEN_WEB_UI_MODE}'. Expected DEV_SERVER or EMBEDDED.")
endif()

if (XEN_WEB_UI_MODE STREQUAL "EMBEDDED")
    if (NOT XEN_WEB_UI_DIST_DIR)
        message(FATAL_ERROR
            "XEN_WEB_UI_DIST_DIR is required when XEN_WEB_UI_MODE=EMBEDDED.")
    endif()

    if (NOT IS_DIRECTORY "${XEN_WEB_UI_DIST_DIR}")
        message(FATAL_ERROR
            "XEN_WEB_UI_DIST_DIR does not exist or is not a directory: "
            "${XEN_WEB_UI_DIST_DIR}")
    endif()

    if (NOT EXISTS "${XEN_WEB_UI_DIST_DIR}/index.html")
        message(FATAL_ERROR
            "XEN_WEB_UI_DIST_DIR must contain index.html: "
            "${XEN_WEB_UI_DIST_DIR}/index.html")
    endif()
endif()

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(SIGNALS_LIGHT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if (XEN_BUILD_TESTS)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
endif()

# Use tags + shallow for deps that publish tags.
set(XEN_JUCE_TAG "8.0.14" CACHE STRING "JUCE git tag")
set(XEN_YAML_CPP_TAG "yaml-cpp-0.9.0" CACHE STRING "yaml-cpp git tag")
set(XEN_JSON_TAG "v3.12.0" CACHE STRING "nlohmann/json git tag")
set(XEN_CATCH2_TAG "v3.15.1" CACHE STRING "Catch2 git tag")
set(XEN_SIGNALS_LIGHT_TAG "v0.9" CACHE STRING "signals-light git tag")

FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${XEN_JUCE_TAG}
    GIT_SHALLOW TRUE
    PATCH_COMMAND
        ${CMAKE_COMMAND}
        -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/juce-linux-webkit-hardware-acceleration.patch
        -DSOURCE_DIR=<SOURCE_DIR>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
    SYSTEM
)

FetchContent_Declare(
    yaml_cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG ${XEN_YAML_CPP_TAG}
    GIT_SHALLOW TRUE
    SYSTEM
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG ${XEN_JSON_TAG}
    GIT_SHALLOW TRUE
    SYSTEM
)

FetchContent_Declare(
    signals_light
    GIT_REPOSITORY https://github.com/a-n-t-h-o-n-y/signals-light.git
    GIT_TAG ${XEN_SIGNALS_LIGHT_TAG}
    GIT_SHALLOW TRUE
    SYSTEM
)

FetchContent_MakeAvailable(juce yaml_cpp json signals_light)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(juce_gui_extra INTERFACE
        "JUCE_WEBKIT_HARDWARE_ACCELERATION_POLICY=${XEN_JUCE_WEBKIT_HARDWARE_ACCELERATION_POLICY}")
    message(STATUS
        "Linux WebKitGTK hardware acceleration policy: "
        "${XEN_WEBVIEW_HARDWARE_ACCELERATION_POLICY}")
endif()

if (XEN_BUILD_TESTS)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG ${XEN_CATCH2_TAG}
        GIT_SHALLOW TRUE
        SYSTEM
    )
    FetchContent_MakeAvailable(catch2)
endif()

if (XEN_BUILD_AUDIO_PLUGIN_HOST)
    add_subdirectory(${juce_SOURCE_DIR}/extras/AudioPluginHost
                     ${juce_BINARY_DIR}/extras/AudioPluginHost
                     SYSTEM)
endif()

# BINARY DATA --------------------------------------------------------------------------

# Add scale files as a binary resource
juce_add_binary_data(EmbedScales
    HEADER_NAME "embed_scales.hpp"
    NAMESPACE embed_scales
    SOURCES
        data/scales/scales.yml
        data/scales/user_scales.yml
)

# Add chord files as a binary resource
juce_add_binary_data(EmbedChords
    HEADER_NAME "embed_chords.hpp"
    NAMESPACE embed_chords
    SOURCES
        data/chords/chords.yml
        data/chords/user_chords.yml
)

if (XEN_WEB_UI_MODE STREQUAL "EMBEDDED")
    file(GLOB_RECURSE XEN_WEB_UI_DIST_FILES CONFIGURE_DEPENDS
        LIST_DIRECTORIES false
        "${XEN_WEB_UI_DIST_DIR}/*")

    if (NOT XEN_WEB_UI_DIST_FILES)
        message(FATAL_ERROR
            "No files found in XEN_WEB_UI_DIST_DIR: ${XEN_WEB_UI_DIST_DIR}")
    endif()

    juce_add_binary_data(EmbedWebUI
        HEADER_NAME "embed_webui.hpp"
        NAMESPACE embed_webui
        SOURCES
            ${XEN_WEB_UI_DIST_FILES}
    )
endif()

# BUILD TARGETS ------------------------------------------------------------------------

set(XEN_DOMAIN_SOURCES
    src/actions.cpp
    src/chord.cpp
    src/composition.cpp
    src/command.cpp
    src/message_level.cpp
    src/midi_compiler.cpp
    src/realtime_midi_player.cpp
    src/modulator.cpp
    src/project_validation.cpp
    src/scale.cpp
    src/selection.cpp
    src/serialize.cpp
    src/string_manip.cpp
    src/text_file.cpp
    src/user_directory.cpp
    src/utility.cpp
)

set(XEN_APPLICATION_SOURCES
    src/command_catalog.cpp
    src/command_catalog_bind.cpp
    src/command_catalog_docs.cpp
    src/command_catalog_metadata.cpp
    src/command_catalog_parse.cpp
    src/command_catalog_specs_bootstrap.cpp
    src/command_catalog_specs_composition.cpp
    src/command_catalog_specs_edit.cpp
    src/command_catalog_specs_set_and_shift.cpp
    src/command_catalog_specs_transform.cpp
    src/command_transaction.cpp
    src/coordinator_launcher.cpp
    src/coordinator_registry.cpp
    src/coordinator_server.cpp
    src/document_storage.cpp
    src/keymap.cpp
    src/ipc_sequencer_session_client.cpp
    src/midi_compilation_service.cpp
    src/ipc_protocol.cpp
    src/parse_args.cpp
    src/preferences.cpp
    src/sequencer_session.cpp
    src/session_coordinator.cpp
    src/submission_effects.cpp
    src/workspace_settings.cpp
)

set(XEN_BRIDGE_PROTOCOL_SOURCES
    src/bridge_serialize.cpp
    src/webview_bridge.cpp
    src/webview_bridge_protocol.cpp
    src/webview_bridge_services.cpp
)

set(XEN_JUCE_UI_SOURCES
    src/xen_editor.cpp
    src/gui/webview_host.cpp
)

set(XEN_PROCESSOR_SOURCES
    src/xen_processor.cpp
)

function(xen_apply_compile_settings target_name definitions_scope)
    target_compile_definitions(${target_name}
        ${definitions_scope}
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_MODAL_LOOPS_PERMITTED=1
    )

    target_compile_options(${target_name} PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:/W1>
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
        -Wall
        -Wextra
        -Wpedantic
        -Wsign-conversion
      >
    )
endfunction()

add_library(XenDomain STATIC ${XEN_DOMAIN_SOURCES})
target_include_directories(XenDomain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(XenDomain
    PUBLIC
        sequencer
        signals-light
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_cryptography
        yaml-cpp
        nlohmann_json::nlohmann_json
        EmbedScales
        EmbedChords
)
xen_apply_compile_settings(XenDomain PUBLIC)

add_library(XenApplication STATIC ${XEN_APPLICATION_SOURCES})
target_include_directories(XenApplication PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(XenApplication
    PUBLIC
        XenDomain
        juce::juce_events
        nlohmann_json::nlohmann_json
        yaml-cpp
)
xen_apply_compile_settings(XenApplication PUBLIC)

add_executable(XenSequencerCoordinator
    src/ipc/coordinator_main.cpp
)
target_include_directories(XenSequencerCoordinator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(XenSequencerCoordinator
    PUBLIC
        XenApplication
        juce::juce_core
        juce::juce_events
)
xen_apply_compile_settings(XenSequencerCoordinator PRIVATE)

target_compile_definitions(XenApplication
    PUBLIC
        XEN_COORDINATOR_HELPER_PATH="$<TARGET_FILE:XenSequencerCoordinator>"
)

add_library(XenBridgeProtocol STATIC ${XEN_BRIDGE_PROTOCOL_SOURCES})
target_include_directories(XenBridgeProtocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(XenBridgeProtocol
    PUBLIC
        XenApplication
        nlohmann_json::nlohmann_json
)
xen_apply_compile_settings(XenBridgeProtocol PUBLIC)

add_library(XenJuceUI STATIC ${XEN_JUCE_UI_SOURCES})
target_include_directories(XenJuceUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(XenJuceUI
    PUBLIC
        XenApplication
        XenBridgeProtocol
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_gui_extra
)

if ((CMAKE_SYSTEM_NAME STREQUAL "Linux") OR (CMAKE_SYSTEM_NAME MATCHES ".*BSD"))
    target_link_libraries(XenJuceUI PUBLIC juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS)
endif()

if (TARGET EmbedWebUI)
    target_link_libraries(XenJuceUI PUBLIC EmbedWebUI)
endif()

target_compile_definitions(XenJuceUI
    PUBLIC
        XEN_WEB_UI_USE_DEV_SERVER=$<STREQUAL:${XEN_WEB_UI_MODE},DEV_SERVER>
        XEN_WEB_UI_USE_EMBEDDED=$<STREQUAL:${XEN_WEB_UI_MODE},EMBEDDED>
        XEN_WEB_UI_DEV_URL="${XEN_WEB_UI_DEV_URL}"
        XEN_WEB_UI_DIST_DIR="${XEN_WEB_UI_DIST_DIR}"
)
xen_apply_compile_settings(XenJuceUI PRIVATE)

# PLUGIN -------------------------------------------------------------------------------

# `juce_add_plugin` adds a static library target with the name passed as the first
# argument. This target is a normal CMake target, but has a lot of extra properties set
# up by default. As well as this shared code static library, this function adds targets
# for each of the formats specified by the FORMATS arguments. This function accepts many
# optional arguments. Check the readme at `docs/CMake API.md` in the JUCE repo for the
# full list.

juce_add_plugin(XenSequencer
    PRODUCT_NAME "XenSequencer"             # The name of the final executable
    COMPANY_NAME "A"                        # Specify the name of the plugin's author
    IS_SYNTH TRUE                           # Is this a synth or an effect?
    NEEDS_MIDI_INPUT TRUE                   # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT TRUE                  # Does the plugin need midi output?
    NEEDS_WEB_BROWSER TRUE                  # Enables JUCE WebView support
    IS_MIDI_EFFECT FALSE                    # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE        # Does the editor need keyboard focus?
    PLUGIN_MANUFACTURER_CODE Juce           # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Dem0                        # A unique four-character plugin id with exactly one upper-case character
                                            # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS VST3 Standalone                 # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
)

target_sources(XenSequencer PRIVATE ${XEN_PROCESSOR_SOURCES})
target_include_directories(XenSequencer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(XenSequencer
    PUBLIC
        XenApplication
        XenJuceUI
        juce::juce_recommended_lto_flags
        juce::juce_recommended_config_flags
)
xen_apply_compile_settings(XenSequencer PUBLIC)

foreach(format_target XenSequencer_VST3 XenSequencer_Standalone)
    if (TARGET ${format_target})
        add_custom_command(TARGET ${format_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:XenSequencerCoordinator>
                $<TARGET_FILE_DIR:${format_target}>/$<TARGET_FILE_NAME:XenSequencerCoordinator>
            VERBATIM
        )
        add_dependencies(${format_target} XenSequencerCoordinator)
    endif()
endforeach()

# TESTS --------------------------------------------------------------------------------

if (XEN_BUILD_TESTS)
    enable_testing()

    set(XEN_CORE_TEST_SOURCES
        test/core/command_handler_execution.test.cpp
        test/core/command_catalog.test.cpp
        test/core/command_parse.test.cpp
        test/core/keymap.test.cpp
        test/core/preferences.test.cpp
        test/core/state_actions.test.cpp
        test/core/timeline_commit.test.cpp
        test/core/webview_bridge.test.cpp
        test/data_model_refactor.test.cpp
        test/midi.test.cpp
        test/realtime_midi_player.test.cpp
        test/numeric_safety.test.cpp
        test/processor/command_effects.test.cpp
        test/processor/document_persistence.test.cpp
        test/processor/processor_commands.test.cpp
        test/processor/processor_timeline_commit.test.cpp
        test/sync/audio_thread_state_exchange.test.cpp
        test/sync/compiled_midi_mailbox.test.cpp
        test/sync/engine_snapshot_contract.test.cpp
        test/sync/session_coordinator.test.cpp
        test/utility.test.cpp
    )

    set(XEN_ADAPTER_TEST_SOURCES
        test/processor/processor_state_roundtrip.test.cpp
    )

    if (XEN_CORE_TEST_SOURCES)
        add_executable(XenCoreTests ${XEN_CORE_TEST_SOURCES})

        target_link_libraries(XenCoreTests
            PUBLIC
                XenBridgeProtocol
                Catch2::Catch2WithMain
        )
        xen_apply_compile_settings(XenCoreTests PRIVATE)

        add_test(NAME XenCoreTests COMMAND $<TARGET_FILE:XenCoreTests>)
    else()
        message(STATUS "XEN_BUILD_TESTS is ON, but no Xen core test sources are configured.")
    endif()

    if (XEN_ADAPTER_TEST_SOURCES)
        add_executable(XenAdapterTests ${XEN_ADAPTER_TEST_SOURCES})

        target_link_libraries(XenAdapterTests
            PUBLIC
                XenSequencer
                Catch2::Catch2WithMain
        )
        xen_apply_compile_settings(XenAdapterTests PRIVATE)
        add_dependencies(XenAdapterTests XenSequencerCoordinator)

        if ((CMAKE_SYSTEM_NAME STREQUAL "Linux") OR
            (CMAKE_SYSTEM_NAME MATCHES ".*BSD"))
            juce_link_with_embedded_linux_subprocess(XenAdapterTests)
        endif()

        add_test(NAME XenAdapterTests COMMAND $<TARGET_FILE:XenAdapterTests>)
    else()
        message(STATUS "XEN_BUILD_TESTS is ON, but no Xen adapter test sources are configured.")
    endif()

    if (UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_executable(XenRealtimeMidiAllocationTests
            test/realtime_midi_allocation.test.cpp
        )
        target_include_directories(XenRealtimeMidiAllocationTests
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(XenRealtimeMidiAllocationTests PUBLIC XenDomain)
        target_link_options(XenRealtimeMidiAllocationTests PRIVATE
            -Wl,--wrap=malloc
            -Wl,--wrap=calloc
            -Wl,--wrap=realloc
            -Wl,--wrap=free
        )
        xen_apply_compile_settings(XenRealtimeMidiAllocationTests PRIVATE)
        add_test(NAME XenRealtimeMidiAllocationTests
            COMMAND $<TARGET_FILE:XenRealtimeMidiAllocationTests>)
    endif()
endif()

# TOOLS --------------------------------------------------------------------------------

if (XEN_BUILD_TOOLS)
    add_subdirectory(tools/cmd_reference)
    add_subdirectory(tools/keypress)
endif()
