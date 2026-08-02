cmake_minimum_required(VERSION 3.16)

project(seafox
    VERSION 1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB_RECURSE SRC_FILES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.c"
)

add_executable(seafox ${SRC_FILES})

target_include_directories(seafox
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/compiler
)

target_compile_options(seafox
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(seafox
    PRIVATE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Debug>:-O0 -g>
)

target_link_options(seafox
    PRIVATE
        -static
)