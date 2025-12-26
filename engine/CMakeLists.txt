file(GLOB_RECURSE BOXEL_SOURCES "src/**.c")
add_library(Boxel STATIC ${BOXEL_SOURCES})

target_compile_definitions(Boxel PRIVATE
    $<$<CONFIG:Debug>:BOX_BUILD_DEV>
    $<$<CONFIG:Release>:BOX_BUILD_RELEASE>
    $<$<CONFIG:Dist>:BOX_BUILD_DIST>
)

if (MSVC)
    target_compile_options(Boxel PRIVATE
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2 /Zi>
        $<$<CONFIG:Dist>:/O2>
    )
else ()
    target_compile_options(Boxel PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:Release>:-O2 -g>
        $<$<CONFIG:Dist>:-O3>
    )
endif()

find_package(Vulkan REQUIRED)

target_include_directories(Boxel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(Boxel PUBLIC glfw Vulkan::Vulkan)