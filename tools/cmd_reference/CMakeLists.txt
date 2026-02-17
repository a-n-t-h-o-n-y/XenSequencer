add_executable(cmd_reference
   main.cpp
)

target_link_libraries(cmd_reference
    PRIVATE
        XenSequencer
)
