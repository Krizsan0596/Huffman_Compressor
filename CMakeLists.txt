cmake_minimum_required(VERSION 3.10)
project(huffman)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_executable(${PROJECT_NAME}
    src/main.c
    lib/file.c
    lib/compress.c
    lib/decompress.c
)

target_include_directories(${PROJECT_NAME} PRIVATE lib)
target_link_libraries(${PROJECT_NAME} PRIVATE m)

add_executable(file_io_test tests/test_file_io.c lib/file.c lib/compress.c)
target_include_directories(file_io_test PRIVATE lib)
target_link_libraries(file_io_test m)
add_test(NAME FileIOTest COMMAND file_io_test)

add_executable(compress_test tests/test_compress.c lib/compress.c)
target_include_directories(compress_test PRIVATE lib)
target_link_libraries(compress_test m)
add_test(NAME CompressTest COMMAND compress_test)
