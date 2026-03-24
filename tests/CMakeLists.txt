add_executable(test_section test_section.c)
target_link_libraries(test_section PRIVATE vaultpack)
add_test(NAME test_section COMMAND test_section)

add_executable(test_archive test_archive.c)
target_link_libraries(test_archive PRIVATE vaultpack)
add_test(NAME test_archive COMMAND test_archive)

add_executable(test_metadata test_metadata.c)
target_link_libraries(test_metadata PRIVATE vaultpack)
add_test(NAME test_metadata COMMAND test_metadata)

add_executable(test_index test_index.c)
target_link_libraries(test_index PRIVATE vaultpack)
add_test(NAME test_index COMMAND test_index)

add_executable(test_extract test_extract.c)
target_link_libraries(test_extract PRIVATE vaultpack)
add_test(NAME test_extract COMMAND test_extract)

add_executable(test_stream test_stream.c)
target_link_libraries(test_stream PRIVATE vaultpack)
add_test(NAME test_stream COMMAND test_stream)

add_executable(test_buffer test_buffer.c)
target_link_libraries(test_buffer PRIVATE vaultpack)
add_test(NAME test_buffer COMMAND test_buffer)

add_executable(test_crc32 test_crc32.c)
target_link_libraries(test_crc32 PRIVATE vaultpack)
add_test(NAME test_crc32 COMMAND test_crc32)

add_executable(test_writer test_writer.c)
target_link_libraries(test_writer PRIVATE vaultpack)
add_test(NAME test_writer COMMAND test_writer)

add_executable(test_codec test_codec.c)
target_link_libraries(test_codec PRIVATE vaultpack)
add_test(NAME test_codec COMMAND test_codec)
