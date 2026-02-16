cmake_minimum_required(VERSION 3.25)
project(XenSequencer VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# DEPENDENCIES -------------------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    signals_light
    URL https://github.com/a-n-t-h-o-n-y/signals-light/archive/5b873d0aa515a2085138359e69746cd7d7a5362f.tar.gz
)
FetchContent_Declare(
    juce
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.12.tar.gz
)
FetchContent_Declare(
    yaml_cpp
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.9.0.tar.gz
)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz
)
FetchContent_Declare(
    catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.13.0.tar.gz
)

FetchContent_MakeAvailable(signals_light juce yaml_cpp nlohmann_json catch2)

add_subdirectory(external/MicrotonalStepSequencer)
add_subdirectory(${juce_SOURCE_DIR}/extras/AudioPluginHost SYSTEM)

# BINARY DATA --------------------------------------------------------------------------

# Add key config files as a binary resource
juce_add_binary_data(EmbedKeys
    HEADER_NAME "embed_keys.hpp"
    NAMESPACE embed_keys
    SOURCES
        data/keys/keys.yml
        data/keys/user_keys.yml
)

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

# Add demo files as a binary resource
juce_add_binary_data(EmbedDemos
    HEADER_NAME "embed_demos.hpp"
    NAMESPACE embed_demos
    SOURCES
        "data/demos/by this river.xss"
        "data/demos/in a landscape.xss"
)

# Add font files as a binary resource
juce_add_binary_data(EmbedFonts
    HEADER_NAME "embed_fonts.hpp"
    NAMESPACE embed_fonts
    SOURCES
        data/font/NotoSansSymbols2-Regular.ttf
        data/font/SourceCodePro-Black.ttf
        data/font/SourceCodePro-Bold.ttf
        data/font/SourceCodePro-ExtraBold.ttf
        data/font/SourceCodePro-ExtraLight.ttf
        data/font/SourceCodePro-Light.ttf
        data/font/SourceCodePro-Medium.ttf
        data/font/SourceCodePro-Regular.ttf
        data/font/SourceCodePro-SemiBold.ttf
        data/font/RobotoMono-Bold.ttf
        data/font/RobotoMono-ExtraLight.ttf
        data/font/RobotoMono-Light.ttf
        data/font/RobotoMono-Medium.ttf
        data/font/RobotoMono-Regular.ttf
        data/font/RobotoMono-SemiBold.ttf
        data/font/RobotoMono-Thin.ttf
)

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
    IS_MIDI_EFFECT FALSE                    # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE        # Does the editor need keyboard focus?
    PLUGIN_MANUFACTURER_CODE Juce           # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Dem0                        # A unique four-character plugin id with exactly one upper-case character
                                            # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS VST3 Standalone                 # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
)

# `target_compile_definitions` adds some preprocessor definitions to our target. In a
# Projucer project, these might be passed in the 'Preprocessor Definitions' field. JUCE
# modules also make use of compile definitions to switch certain features on/off, so if
# there's a particular feature you need that's not on by default, check the module
# header for the correct flag to set here. These definitions will be visible both to
# your code, and also the JUCE module code, so for new definitions, pick unique names
# that are unlikely to collide! This is a standard CMake command.

add_library(XenSequencerEngine STATIC)

target_sources(XenSequencerEngine
    PRIVATE
        src/actions.cpp
        src/chord.cpp
        src/command.cpp
        src/copy_paste.cpp
        src/engine.cpp
        src/guide_text.cpp
        src/input_mode.cpp
        src/message_level.cpp
        src/midi.cpp
        src/midi_engine.cpp
        src/modulator.cpp
        src/parse_args.cpp
        src/scale.cpp
        src/selection.cpp
        src/serialize.cpp
        src/string_manip.cpp
        src/user_directory.cpp
        src/utility.cpp
        src/xen_command_tree.cpp
)

target_include_directories(XenSequencerEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(XenSequencerEngine
    PUBLIC
        sequencer
        signals-light
        juce::juce_audio_utils
        yaml-cpp
        nlohmann_json::nlohmann_json
        EmbedKeys
        EmbedScales
        EmbedChords
        EmbedDemos
        EmbedFonts
        juce::juce_recommended_lto_flags
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_options(XenSequencerEngine PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W1>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

add_library(XenSequencerRuntime STATIC)

target_compile_definitions(XenSequencerRuntime
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)

target_sources(XenSequencerRuntime
    PRIVATE
        src/command_history.cpp
        src/key_core.cpp
        src/runtime_command_tree.cpp
        src/xen_editor.cpp
        src/xen_processor.cpp
        src/gui/accordion.cpp
        src/gui/bg_sequence.cpp
        src/gui/bottom_bar.cpp
        src/gui/cell.cpp
        src/gui/center_component.cpp
        src/gui/command_bar.cpp
        src/gui/directory_list_box.cpp
        src/gui/library_view.cpp
        src/gui/message_log.cpp
        src/gui/modulation_pane.cpp
        src/gui/modulation_slider.cpp
        src/gui/plugin_window.cpp
        src/gui/sequence_bank.cpp
        src/gui/status_bar.cpp
        src/gui/themes.cpp
        src/gui/tile.cpp
        src/gui/tuning_reference.cpp
        src/gui/xen_list_box.cpp
        src/gui/xen_slider.cpp
)

target_include_directories(XenSequencerRuntime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(XenSequencerRuntime
    PUBLIC
        XenSequencerEngine
)

target_compile_options(XenSequencerRuntime PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W1>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_compile_definitions(XenSequencer
    PRIVATE
        # Ensure JUCE plugin-client translation units get the same module config.
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(XenSequencer
    PRIVATE
        XenSequencerRuntime
)

# TESTS --------------------------------------------------------------------------------

add_executable(XenTests
    test/unit/command_framework.test.cpp
    test/unit/guide_text.test.cpp
    test/unit/parse_args.test.cpp
    test/unit/runtime_command_tree.test.cpp
    test/unit/serialize.test.cpp
    test/unit/string_manip.test.cpp
    test/unit/timeline.test.cpp
    test/unit/utility_math.test.cpp
    test/unit/engine.test.cpp
    test/integration/engine_commands.test.cpp
    src/command_history.cpp
    src/runtime_command_tree.cpp
    src/gui/themes.cpp
)

target_link_libraries(XenTests
    PUBLIC
        XenSequencerEngine
        Catch2::Catch2WithMain
)

# TOOLS --------------------------------------------------------------------------------

# add_subdirectory(tools/cmd_reference)
add_subdirectory(tools/keypress)
