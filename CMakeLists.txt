cmake_minimum_required(VERSION 3.15)
project(XenSequencer VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(external/signals-light)
add_subdirectory(external/MicrotonalStepSequencer)
add_subdirectory(external/JUCE)
add_subdirectory(external/yaml-cpp)
add_subdirectory(external/json)

# `juce_add_plugin` adds a static library target with the name passed as the first argument
# (AudioPluginExample here). This target is a normal CMake target, but has a lot of extra properties set
# up by default. As well as this shared code static library, this function adds targets for each of
# the formats specified by the FORMATS arguments. This function accepts many optional arguments.
# Check the readme at `docs/CMake API.md` in the JUCE repo for the full list.

juce_add_plugin(XenSequencer
    VERSION 0.1                                 # Set this if the plugin version is different to the project version
    # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
    # ICON_SMALL ...
    COMPANY_NAME "A"                            # Specify the name of the plugin's author
    IS_SYNTH FALSE                              # Is this a synth or an effect?
    NEEDS_MIDI_INPUT FALSE                       # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT TRUE                      # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                        # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE           # Does the editor need keyboard focus?
    COPY_PLUGIN_AFTER_BUILD FALSE               # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE Juce               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Dem0                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS VST3 Standalone # AU                # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME "XenSequencer"                 # The name of the final executable, which can differ from the target name
)

# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. This should be included with `#include <JuceHeader.h>`. The include path for
# this header will be automatically added to the target. The main function of the JuceHeader is to
# include all your JUCE module headers; if you're happy to include module headers directly, you
# probably don't need to call this.

# juce_generate_juce_header(AudioPluginExample)

target_sources(XenSequencer
    PRIVATE
        src/actions.cpp
        src/command.cpp
        src/command_core.cpp
        src/input_mode.cpp
        src/key_core.cpp
        src/midi.cpp
        src/plugin_processor.cpp
        src/selection.cpp
        src/user_directory.cpp
        src/util.cpp
        src/xen_command_core.cpp
        src/xen_editor.cpp
        src/xen_processor.cpp
        src/actions.hpp
        src/command.hpp
        src/command_core.hpp
        src/input_mode.hpp
        src/key_core.hpp
        src/midi.hpp
        src/parse_args.hpp
        src/plugin_processor.hpp
        src/selection.hpp
        src/serialize_state.cpp
        src/serialize_state.hpp
        src/signature.hpp
        src/state.hpp
        src/timeline.hpp
        src/user_directory.hpp
        src/util.hpp
        src/xen_command_core.hpp
        src/xen_editor.hpp
        src/xen_processor.hpp
        src/xen_timeline.hpp

        src/gui/sequence.cpp
        src/gui/command_bar.hpp
        src/gui/custom_look_and_feel.hpp
        src/gui/file_button.hpp
        src/gui/focusable_component.hpp
        src/gui/heading.hpp
        src/gui/homogenous_row.hpp
        src/gui/mantissa_box.hpp
        src/gui/measure.hpp
        src/gui/number_box.hpp
        src/gui/phrase.hpp
        src/gui/phrase_editor.hpp
        src/gui/plugin_window.hpp
        src/gui/sequence.hpp
        src/gui/status_bar.hpp
        src/gui/tuning.hpp

        src/gui/utility/dereference_iterator.hpp
)

# `target_compile_definitions` adds some preprocessor definitions to our target. In a Projucer
# project, these might be passed in the 'Preprocessor Definitions' field. JUCE modules also make use
# of compile definitions to switch certain features on/off, so if there's a particular feature you
# need that's not on by default, check the module header for the correct flag to set here. These
# definitions will be visible both to your code, and also the JUCE module code, so for new
# definitions, pick unique names that are unlikely to collide! This is a standard CMake command.

target_compile_definitions(XenSequencer
    PUBLIC
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)

# Add keys.yml as a binary resource
juce_add_binary_data(EmbedKeys
    HEADER_NAME "embed_keys.hpp"
    NAMESPACE embed_keys
    SOURCES
        keys.yml
)

target_link_libraries(XenSequencer
    PRIVATE
        signals-light
        sequencer
        juce::juce_audio_utils
        yaml-cpp
        nlohmann_json::nlohmann_json
        EmbedKeys
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_options(XenSequencer INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/W1>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)