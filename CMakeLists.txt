cmake_minimum_required(VERSION 3.15)
project(Boxel VERSION 0.1 LANGUAGES C)

include(FetchContent)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TINYCTHREAD_DISABLE_TESTS ON CACHE BOOL "" FORCE)

add_subdirectory(engine)

FetchContent_Declare(
    GLFW
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(GLFW)

FetchContent_Declare(
    TinyCThread
    GIT_REPOSITORY https://github.com/tinycthread/tinycthread.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(TinyCThread)