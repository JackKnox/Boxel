cmake_minimum_required(VERSION 3.15)
project(Boxel VERSION 0.1 LANGUAGES C)

include(FetchContent)
set(CMAKE_C_STANDARD 11)

# Allow custom build types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Dist" CACHE STRING "" FORCE)

# Single-config generators (Ninja, Makefiles)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_subdirectory(engine)
add_subdirectory(sandbox)

FetchContent_Declare(
    GLFW
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(GLFW)