cmake_minimum_required(VERSION 3.10)
project(
    example
    LANGUAGES C
    VERSION 1.0.0
)

include(cmake/CPM.cmake)
CPMAddPackage("gh:open-s4c/libvsync@3.6.0")
CPMAddPackage("gh:open-s4c/tilt#main")
# Alternatively, include the parent directory
# add_subdirectory(../.. ../..)

add_executable(example src/example.c)
target_link_libraries(example pthread)

add_library(lock SHARED src/lock.c)
target_link_libraries(lock tilt vsync)

add_library(mcslock SHARED src/mcslock.c)
target_link_libraries(mcslock tilt vsync)
