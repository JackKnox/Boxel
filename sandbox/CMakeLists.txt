file(GLOB_RECURSE SANDBOX_SOURCES "src/**.c")
add_executable(Sandbox ${SANDBOX_SOURCES})

target_link_libraries(Sandbox PUBLIC Boxel)

add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:Sandbox>/assets/
)