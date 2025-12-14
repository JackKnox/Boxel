cmake_minimum_required(VERSION 3.15)
project(Boxel VERSION 0.1 LANGUAGES C)

include(FetchContent)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_COMPILER 11)

set(TINYCTHREAD_DISABLE_TESTS ON CACHE BOOL "" FORCE)

add_subdirectory(engine)
add_subdirectory(sandbox)