cmake_minimum_required(VERSION 3.10)
project(huffman)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
    src/main.c
    lib/file.c
)

target_include_directories(${PROJECT_NAME} PRIVATE lib)
