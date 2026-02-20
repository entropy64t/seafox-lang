cmake_minimum_required(VERSION 3.16)

project(seafox
    VERSION 1.0
    LANGUAGES C
)

# Use modern C standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB SRC_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

# Create executable
add_executable(seafox
    ${SRC_FILES}
)

# Include directories
target_include_directories(seafox
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# Compiler warnings
target_compile_options(seafox
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        --static
)
