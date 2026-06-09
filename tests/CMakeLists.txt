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

add_executable(test_validate test_validate.c)
target_link_libraries(test_validate PRIVATE vaultpack)
add_test(NAME test_validate COMMAND test_validate)

add_executable(test_manifest test_manifest.c)
target_link_libraries(test_manifest PRIVATE vaultpack)
add_test(NAME test_manifest COMMAND test_manifest)

add_executable(test_decompress test_decompress.c)
target_link_libraries(test_decompress PRIVATE vaultpack)
add_test(NAME test_decompress COMMAND test_decompress)

add_executable(test_section_pipeline test_section_pipeline.c)
target_link_libraries(test_section_pipeline PRIVATE vaultpack)
add_test(NAME test_section_pipeline COMMAND test_section_pipeline)

add_executable(test_pack_ctx test_pack_ctx.c)
target_link_libraries(test_pack_ctx PRIVATE vaultpack)
add_test(NAME test_pack_ctx COMMAND test_pack_ctx)

add_executable(test_chunk_assembler test_chunk_assembler.c)
target_link_libraries(test_chunk_assembler PRIVATE vaultpack)
add_test(NAME test_chunk_assembler COMMAND test_chunk_assembler)

add_executable(test_range_io test_range_io.c)
target_link_libraries(test_range_io PRIVATE vaultpack)
add_test(NAME test_range_io COMMAND test_range_io)

add_executable(test_section_cache test_section_cache.c)
target_link_libraries(test_section_cache PRIVATE vaultpack)
add_test(NAME test_section_cache COMMAND test_section_cache)

add_executable(test_interval_engine test_interval_engine.c)
target_link_libraries(test_interval_engine PRIVATE vaultpack)
add_test(NAME test_interval_engine COMMAND test_interval_engine)

add_executable(test_archive_merge test_archive_merge.c)
target_link_libraries(test_archive_merge PRIVATE vaultpack)
add_test(NAME test_archive_merge COMMAND test_archive_merge)

add_executable(test_archive_repair test_archive_repair.c)
target_link_libraries(test_archive_repair PRIVATE vaultpack)
add_test(NAME test_archive_repair COMMAND test_archive_repair)

add_executable(test_meta_schema test_meta_schema.c)
target_link_libraries(test_meta_schema PRIVATE vaultpack)
add_test(NAME test_meta_schema COMMAND test_meta_schema)

add_executable(test_index2 test_index2.c)
target_link_libraries(test_index2 PRIVATE vaultpack)
add_test(NAME test_index2 COMMAND test_index2)

add_executable(test_cross_module_archive test_cross_module_archive.c)
target_link_libraries(test_cross_module_archive PRIVATE vaultpack)
add_test(NAME test_cross_module_archive COMMAND test_cross_module_archive)

add_executable(test_bug_path_coverage test_bug_path_coverage.c
               ../fuzz/fuzz_bridge.c)
target_include_directories(test_bug_path_coverage PRIVATE
    ${CMAKE_SOURCE_DIR}/fuzz
    ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_bug_path_coverage PRIVATE vaultpack)
add_test(NAME test_bug_path_coverage COMMAND test_bug_path_coverage)
