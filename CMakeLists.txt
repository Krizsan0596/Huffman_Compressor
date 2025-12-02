cmake_minimum_required(VERSION 3.10)
project(huffman)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_executable(${PROJECT_NAME}
    src/main.c
    lib/file.c
    lib/huffman.c
    lib/directory.c
    lib/util.c
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Werror -g)
target_include_directories(${PROJECT_NAME} PRIVATE lib)
target_link_libraries(${PROJECT_NAME} PRIVATE m)

add_executable(file_io_test tests/test_file_io.c lib/file.c lib/directory.c lib/util.c lib/huffman.c)
target_include_directories(file_io_test PRIVATE lib)
target_link_libraries(file_io_test m)
add_test(NAME FileIOTest COMMAND file_io_test)
target_compile_options(file_io_test PRIVATE --no-warnings -g)

add_executable(compress_test tests/test_compress.c lib/file.c lib/directory.c lib/util.c lib/huffman.c)
target_include_directories(compress_test PRIVATE lib)
target_link_libraries(compress_test m)
add_test(NAME CompressTest COMMAND compress_test)
target_compile_options(compress_test PRIVATE --no-warnings -g)

add_executable(test_compress_decompress tests/test_compress_decompress.c lib/file.c lib/directory.c lib/util.c lib/huffman.c)
target_include_directories(test_compress_decompress PRIVATE lib)
target_link_libraries(test_compress_decompress m)
add_test(NAME CompressDecompressTest COMMAND test_compress_decompress)
target_compile_options(test_compress_decompress PRIVATE --no-warnings -g)

add_executable(directory_test tests/test_directory.c lib/directory.c lib/file.c lib/util.c lib/huffman.c)
target_include_directories(directory_test PRIVATE lib)
target_link_libraries(directory_test m)
add_test(NAME DirectoryTest COMMAND directory_test)
target_compile_options(directory_test PRIVATE --no-warnings -g)
