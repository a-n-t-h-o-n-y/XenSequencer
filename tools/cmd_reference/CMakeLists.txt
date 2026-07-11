add_executable(cmd_reference
   main.cpp
)

target_link_libraries(cmd_reference
    PRIVATE
        XenApplication
)

add_custom_target(update_command_reference
    COMMAND $<TARGET_FILE:cmd_reference> "${PROJECT_SOURCE_DIR}/docs"
    DEPENDS cmd_reference
    VERBATIM
)

if (XEN_BUILD_TESTS)
    add_test(
        NAME CommandReferenceUpToDate
        COMMAND $<TARGET_FILE:cmd_reference> --check "${PROJECT_SOURCE_DIR}/docs"
    )
endif()
