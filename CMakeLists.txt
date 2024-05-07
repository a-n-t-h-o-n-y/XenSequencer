cmake_minimum_required(VERSION 3.25)
project(XenSequencer VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED)

add_subdirectory(external/signals-light)
add_subdirectory(external/MicrotonalStepSequencer)
add_subdirectory(external/JUCE SYSTEM)
add_subdirectory(external/JUCE/extras/AudioPluginHost SYSTEM)
add_subdirectory(external/yaml-cpp SYSTEM)
add_subdirectory(external/json SYSTEM)
add_subdirectory(external/nng SYSTEM)
if (NOT TARGET Catch2)
    add_subdirectory(external/Catch2 SYSTEM)
endif()

# XenSequencer Code
add_library(XenSequencerLib STATIC)

target_include_directories(XenSequencerLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(XenSequencerLib
    PUBLIC
        cxx_std_20
)
target_compile_options(XenSequencerLib INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/W1>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

set_target_properties(XenSequencerLib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Add config files as a binary resource
juce_add_binary_data(EmbedKeys
    HEADER_NAME "embed_keys.hpp"
    NAMESPACE embed_keys
    SOURCES
        data/keys/keys.yml
        data/keys/user_keys.yml
)

# Add demo files as a binary resource
juce_add_binary_data(EmbedDemos
    HEADER_NAME "embed_demos.hpp"
    NAMESPACE embed_demos
    SOURCES
        data/demos/double\ note.json
        data/demos/double\ note\ 2.json
)


target_link_libraries(XenSequencerLib
    PUBLIC
        sequencer
        signals-light
        juce::juce_audio_utils
        yaml-cpp
        nlohmann_json::nlohmann_json
        EmbedKeys
        EmbedDemos
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        nng
        Boost::boost
)

target_sources(XenSequencerLib
    PUBLIC
        src/main.cpp
        src/actions.cpp
        src/input_mode.cpp
        src/key_core.cpp
        src/midi.cpp
        src/selection.cpp
        src/user_directory.cpp
        src/utility.cpp
        src/string_manip.cpp
        src/serialize.cpp
        src/active_sessions.cpp
        src/parse_args.cpp
        src/guide_text.cpp
        src/instance_directory.cpp
        src/inter_process_relay.cpp

        src/gui/active_sessions.cpp
        src/gui/directory_view.cpp
        src/gui/sequence.cpp
        src/gui/themes.cpp
        src/gui/timeline.cpp
        src/gui/plugin_window.cpp

        include/xen/actions.hpp
        include/xen/command.hpp
        include/xen/command_history.hpp
        include/xen/constants.hpp
        include/xen/guide_text.hpp
        include/xen/input_mode.hpp
        include/xen/instance_directory.hpp
        include/xen/inter_process_relay.hpp
        include/xen/invoke.hpp
        include/xen/key_core.hpp
        include/xen/message_level.hpp
        include/xen/midi.hpp
        include/xen/parse_args.hpp
        include/xen/plugin_processor.hpp
        include/xen/selection.hpp
        include/xen/serialize.hpp
        include/xen/active_sessions.hpp
        include/xen/signature.hpp
        include/xen/state.hpp
        include/xen/string_manip.hpp
        include/xen/timeline.hpp
        include/xen/user_directory.hpp
        include/xen/utility.hpp
        include/xen/xen_command_tree.hpp
        include/xen/xen_editor.hpp
        include/xen/xen_processor.hpp

        include/xen/gui/accordion.hpp
        include/xen/gui/active_sessions.hpp
        include/xen/gui/bottom_bar.hpp
        include/xen/gui/color_ids.hpp
        include/xen/gui/command_bar.hpp
        include/xen/gui/themes.hpp
        include/xen/gui/directory_view.hpp
        include/xen/gui/file_button.hpp
        include/xen/gui/focusable_component.hpp
        include/xen/gui/heading.hpp
        include/xen/gui/homogenous_row.hpp
        include/xen/gui/mantissa_box.hpp
        include/xen/gui/measure.hpp
        include/xen/gui/measure_grid.hpp
        include/xen/gui/number_box.hpp
        include/xen/gui/phrase.hpp
        include/xen/gui/phrase_editor.hpp
        include/xen/gui/plugin_window.hpp
        include/xen/gui/sequence.hpp
        include/xen/gui/sequence.old.hpp
        include/xen/gui/status_bar.hpp
        include/xen/gui/timeline.hpp
        include/xen/gui/tuning.hpp

        include/xen/gui/utility/dereference_iterator.hpp
)


add_executable(XenTests
    test/command.test.cpp
    test/utility.test.cpp
    test/midi.test.cpp
)
target_link_libraries(XenTests
    PUBLIC
        XenSequencerLib
        Catch2::Catch2WithMain
)

# `juce_add_plugin` adds a static library target with the name passed as the first argument
# (AudioPluginExample here). This target is a normal CMake target, but has a lot of extra properties set
# up by default. As well as this shared code static library, this function adds targets for each of
# the formats specified by the FORMATS arguments. This function accepts many optional arguments.
# Check the readme at `docs/CMake API.md` in the JUCE repo for the full list.

juce_add_plugin(XenSequencer
    PRODUCT_NAME "XenSequencer"             # The name of the final executable
    COMPANY_NAME "A"                        # Specify the name of the plugin's author
    IS_SYNTH FALSE                          # Is this a synth or an effect?
    NEEDS_MIDI_INPUT FALSE                  # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT TRUE                  # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                    # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE        # Does the editor need keyboard focus?
    PLUGIN_MANUFACTURER_CODE Juce           # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Dem0                        # A unique four-character plugin id with exactly one upper-case character
                                            # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS VST3 Standalone # AU            # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
)

# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. This should be included with `#include <JuceHeader.h>`. The include path for
# this header will be automatically added to the target. The main function of the JuceHeader is to
# include all your JUCE module headers; if you're happy to include module headers directly, you
# probably don't need to call this.

# juce_generate_juce_header(AudioPluginExample)

# `target_compile_definitions` adds some preprocessor definitions to our target. In a Projucer
# project, these might be passed in the 'Preprocessor Definitions' field. JUCE modules also make use
# of compile definitions to switch certain features on/off, so if there's a particular feature you
# need that's not on by default, check the module header for the correct flag to set here. These
# definitions will be visible both to your code, and also the JUCE module code, so for new
# definitions, pick unique names that are unlikely to collide! This is a standard CMake command.

target_compile_definitions(XenSequencerLib
    PUBLIC
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)

target_sources(XenSequencer
    PRIVATE
        src/plugin_processor.cpp
        src/xen_processor.cpp
        src/xen_editor.cpp

        include/xen/plugin_processor.hpp
        include/xen/xen_editor.hpp
        include/xen/xen_processor.hpp
)

target_link_libraries(XenSequencer
    PUBLIC
        XenSequencerLib
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_options(XenSequencer INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/W1>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

add_subdirectory(tools/cmd_reference)
add_subdirectory(tools/keypress)