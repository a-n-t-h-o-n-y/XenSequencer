juce_add_gui_app(KeyPress
    PRODUCT_NAME "KeyPress")

target_sources(KeyPress PRIVATE
    src/main.cpp
)

target_compile_features(KeyPress
    PUBLIC
        cxx_std_20
)

target_link_libraries(KeyPress PRIVATE
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_events
    juce::juce_core
)
