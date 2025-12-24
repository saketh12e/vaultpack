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
