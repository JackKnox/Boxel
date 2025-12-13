file(GLOB_RECURSE BOXEL_SOURCES "src/**.c")
add_library(Boxel STATIC ${BOXEL_SOURCES})

find_package(Vulkan REQUIRED)

target_include_directories(Boxel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(Boxel PUBLIC tinycthread Vulkan::Vulkan)