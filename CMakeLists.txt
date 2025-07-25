cmake_minimum_required(VERSION 3.25)
project(XenSequencer VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# DEPENDENCIES -------------------------------------------------------------------------

add_subdirectory(external/signals-light)
add_subdirectory(external/MicrotonalStepSequencer)
add_subdirectory(external/JUCE SYSTEM)
add_subdirectory(external/JUCE/extras/AudioPluginHost SYSTEM)
add_subdirectory(external/yaml-cpp SYSTEM)
add_subdirectory(external/json SYSTEM)
if (NOT TARGET Catch2)
    add_subdirectory(external/Catch2 SYSTEM)
endif()

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

target_compile_definitions(XenSequencer
    PUBLIC
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
    # PRIVATE
        # $<$<CONFIG:Debug>:_GLIBCXX_DEBUG>
)

target_sources(XenSequencer
    PRIVATE
        src/actions.cpp
        src/chord.cpp
        src/command.cpp
        src/command_history.cpp
        src/copy_paste.cpp
        src/input_mode.cpp
        src/key_core.cpp
        src/message_level.cpp
        src/midi.cpp
        src/midi_engine.cpp
        src/modulator.cpp
        src/scale.cpp
        src/selection.cpp
        src/user_directory.cpp
        src/utility.cpp
        src/string_manip.cpp
        src/serialize.cpp
        src/parse_args.cpp
        src/guide_text.cpp
        src/xen_command_tree.cpp
        src/xen_editor.cpp
        src/xen_processor.cpp

        src/gui/accordion.cpp
        src/gui/bg_sequence.cpp
        src/gui/bottom_bar.cpp
        src/gui/cell.cpp
        src/gui/center_component.cpp
        src/gui/command_bar.cpp
        src/gui/directory_list_box.cpp
        # src/gui/graph.cpp
        src/gui/library_view.cpp
        src/gui/message_log.cpp
        src/gui/modulation_pane.cpp
        src/gui/plugin_window.cpp
        src/gui/sequence_bank.cpp
        src/gui/status_bar.cpp
        src/gui/themes.cpp
        src/gui/tile.cpp
        src/gui/tuning_reference.cpp
        src/gui/xen_list_box.cpp
        src/gui/xen_slider.cpp

        include/xen/actions.hpp
        include/xen/double_buffer.hpp
        include/xen/chord.hpp
        include/xen/clock.hpp
        include/xen/copy_paste.hpp
        include/xen/command.hpp
        include/xen/command_history.hpp
        include/xen/constants.hpp
        include/xen/guide_text.hpp
        include/xen/input_mode.hpp
        include/xen/key_core.hpp
        include/xen/lock_free_optional.hpp
        include/xen/lock_free_queue.hpp
        include/xen/message_level.hpp
        include/xen/midi.hpp
        include/xen/midi_engine.hpp
        include/xen/modulator.hpp
        include/xen/parse_args.hpp
        include/xen/selection.hpp
        include/xen/serialize.hpp
        include/xen/scale.hpp
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
        include/xen/gui/bg_sequence.hpp
        include/xen/gui/bottom_bar.hpp
        include/xen/gui/cell.hpp
        include/xen/gui/command_bar.hpp
        include/xen/gui/fonts.hpp
        # include/xen/gui/graph.hpp
        include/xen/gui/themes.hpp
        include/xen/gui/directory_list_box.hpp
        include/xen/gui/homogenous_row.hpp
        include/xen/gui/library_view.hpp
        include/xen/gui/message_log.hpp
        include/xen/gui/modulation_pane.hpp
        include/xen/gui/plugin_window.hpp
        include/xen/gui/sequence_bank.hpp
        include/xen/gui/status_bar.hpp
        include/xen/gui/tile.hpp
        include/xen/gui/tuning_reference.hpp
        include/xen/gui/xen_list_box.hpp
        include/xen/gui/xen_slider.hpp

        include/xen/gui/utility/dereference_iterator.hpp
)


target_include_directories(XenSequencer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(XenSequencer
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
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_options(XenSequencer INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/W1>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# TESTS --------------------------------------------------------------------------------

add_executable(XenTests
    # test/command.test.cpp
    # test/utility.test.cpp
    # test/midi.test.cpp
    test/command2.test.cpp
)

target_link_libraries(XenTests
    PUBLIC
        XenSequencer
        Catch2::Catch2WithMain
)

# TOOLS --------------------------------------------------------------------------------

add_subdirectory(tools/cmd_reference)
add_subdirectory(tools/keypress)
