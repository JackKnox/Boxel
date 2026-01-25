file(GLOB_RECURSE SANDBOX_SOURCES "src/**.c")
add_executable(Sandbox ${SANDBOX_SOURCES})

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)
FetchContent_GetProperties(stb)

target_link_libraries(Sandbox PUBLIC Boxel)
target_include_directories(Sandbox PRIVATE ${stb_SOURCE_DIR})

add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:Sandbox>/assets/
)

# Find all shader source files
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
file(GLOB SHADER_SOURCES
    "${SHADER_DIR}/*.vert"
    "${SHADER_DIR}/*.frag"
    "${SHADER_DIR}/*.geom"
    "${SHADER_DIR}/*.comp")

foreach(SHADER_FILE ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER_FILE} NAME)
    set(SPV_FILE "${SHADER_FILE}.spv")

    add_custom_command(TARGET Sandbox POST_BUILD
        COMMAND glslc ${SHADER_FILE} -o ${SPV_FILE}
        COMMENT "Compiling shader: ${FILE_NAME}"
        VERBATIM
    )
endforeach()