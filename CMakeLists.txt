cmake_minimum_required(VERSION 3.13)
project(vaultpack
    VERSION 0.1.0
    DESCRIPTION "Compact binary archive format for embedded/IoT firmware payloads"
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
                    -Wmissing-prototypes -Wpointer-arith -Wcast-qual
                    -Wno-unused-parameter)

add_library(vaultpack
    src/util.c
    src/error.c
    src/archive.c
    src/section.c
    src/metadata.c)

target_include_directories(vaultpack
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

add_executable(vpack src/vpack_cli.c)
target_link_libraries(vpack PRIVATE vaultpack)

enable_testing()
add_subdirectory(tests)

install(TARGETS vaultpack
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

install(DIRECTORY include/vaultpack
    DESTINATION include)
