add_executable(cmd_reference
   src/main.cpp
   src/make_command_reference.hpp
)

target_link_libraries(cmd_reference
    PUBLIC
        XenSequencerLib
)