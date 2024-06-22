cmake_minimum_required(VERSION 3.10)
project(
    tilt
    LANGUAGES C
    VERSION 2.1.0
)

include_directories(include)
add_library(tilt INTERFACE)
target_include_directories(tilt INTERFACE include)

enable_testing()
add_subdirectory(tests)
