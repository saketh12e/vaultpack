# VaultPack

VaultPack is a compact binary archive format and a full archive maintenance
toolkit. It provides a small, dependency-free C library for reading and
writing `.vpack` archives, a streaming pack context, a section LRU cache,
range-based I/O, a section decompression pipeline, archive merge / repair /
diff / schema tooling, a sorted compact index v2, an interval engine, and a
command-line tool (`vpack`) for inspecting and maintaining archives on the
host.

The core format is intentionally minimal:

- little-endian throughout
- a fixed 32-byte archive header
- a central directory of variable-sized section entries
- per-section headers followed by raw or compressed payload data
- optional metadata blocks keyed by ASCII strings

VaultPack targets resource-constrained devices where pulling in `tar`, `zip`,
or `zlib` is overkill but the developer still wants a self-describing
container with named sections, integrity checks, and optional compression.

## Repository layout

```
vaultpack/
  include/vaultpack/      public headers
    archive.h              archive open/close/directory
    section.h              section header parser + validation
    metadata.h             metadata block reader + dispatch
    index.h                flat index + slot insertion
    extract.h              section extraction
    error.h                error code table
    buffer.h               growable output buffer
    crc32.h                CRC32 + streaming accumulator
    stream.h               bounded read cursor
    writer.h               section emitter
    manifest.h             text manifest parser
    codec.h                RLE block codec
    validate.h             archive validation report
    decompress.h           LZ4X + HC01 block decoders
    section_pipeline.h     section pipeline (decode + verify + CRC)
    pack_ctx.h             streaming pack context + chunk assembler + dir emitter + abort
    range_io.h             bounded range reader + archive view + cache + pin + prefetch
    merge.h                archive merger + diff engine + offset remap
    repair.h               repair scanner + full recovery
    interval.h             reusable interval merge engine
    meta_schema.h          typed metadata schema + composer
    index2.h               sorted compact index v2
  src/                    library + CLI implementation (39 source files)
  tests/                  CTest unit tests (27 test binaries)
  examples/               create_archive / read_archive / streaming pack / repair-merge / codec round-trip / range+cache
  fuzz/                   ClusterFuzzLite harnesses (20 targets), corpus, dictionary, verify scripts
  .clusterfuzzlite/       build.sh, project.yaml, Dockerfile
  CMakeLists.txt
  LICENSE                 MIT
```

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Command-line tool

```
vpack list     <file.vpack>     list sections in an archive
vpack info     <file.vpack>     print archive header info
vpack verify   <file.vpack>     run validation checks
vpack pack     <manifest> <out> parse a text manifest and emit a .vpack
vpack extract  <file.vpack> <section>  dump a section's payload to stdout
vpack repair   <file.vpack>     scan for SECT headers and report recovery
vpack merge    <a.vpack> <b.vpack> <out.vpack>  report conflicts when merging
vpack diff     <a.vpack> <b.vpack>  show added/removed/changed sections
vpack inspect  <file.vpack>     print CRC32 of header and full archive
```

## Subsystem summary

| Subsystem              | Public headers                                  |
| ---------------------- | ----------------------------------------------- |
| Core archive            | archive.h section.h metadata.h index.h extract.h|
| Writer + manifest       | writer.h manifest.h                              |
| Stream + buffer + CRC   | stream.h buffer.h crc32.h                        |
| Validation + codec      | validate.h codec.h                              |
| Decompression pipeline  | decompress.h section_pipeline.h                 |
| Streaming pack context  | pack_ctx.h                                      |
| Range I/O + cache        | range_io.h                                      |
| Merge / diff / repair   | merge.h repair.h                                |
| Interval engine         | interval.h                                       |
| Schema + composer        | meta_schema.h                                   |
| Index v2                 | index2.h                                         |

## Fuzz targets

20 ClusterFuzzLite harnesses are wired into `.clusterfuzzlite/project.yaml`:

archive_fuzzer section_fuzzer metadata_fuzzer index_fuzzer extract_fuzzer
stream_fuzzer manifest_fuzzer codec_fuzzer writer_fuzzer validate_fuzzer
pipeline_fuzzer pack_fuzzer cache_fuzzer merge_fuzzer repair_fuzzer
schema_fuzzer index2_fuzzer range_fuzzer diff_fuzzer compose_fuzzer

This produces `libvaultpack.a` (or `.so`/`.dylib` depending on platform) and
the `vpack` CLI in `build/`.

## Quick start

```sh
# Build a tiny archive (also see examples/create_archive.c)
./build/vpack list examples/sample.vpack
```

The example `create_archive.c` writes a 64-byte file with one section named
`hello` whose payload is `world\n`.

## Binary format

```
[0]  magic       4B   0x56 0x50 0x41 0x4B  ("VPAK")
[4]  version     2B   0x01 0x00
[6]  flags       2B
[8]  entry_count 4B   number of sections in central directory
[12] dir_offset  8B   byte offset to the start of the central directory
[20] index_width 2B   columns in the flat index
[22] index_depth 2B   rows in the flat index
[24] reserved    8B   zero
[32] <section data starts here>
...
[dir_offset] <central directory: entry_count × vp_dir_entry_t>
[after dir]  <metadata blocks: zero or more vp_meta_block_t>
```

### Central directory entry — `vp_dir_entry_t` (48 bytes minimum, padded to 52)

| Field           | Size | Notes                                        |
|-----------------|------|----------------------------------------------|
| `name`          | 32B  | null-terminated section name, ASCII          |
| `section_offset`| 8B   | byte offset of section data from file start  |
| `section_size`  | 4B   | size of section data                         |
| `flags`         | 2B   |                                              |
| `type`          | 1B   | `0x01` raw, `0x02` compressed, `0x03` meta   |
| `reserved`      | 1B   | zero                                         |
| `meta_count`    | 4B   | trailing metadata entries after section data |
| `sub_count`     | 2B   | sub-entries contributed to the index         |
| `sub_stride`    | 4B   | stride between sub-entries                   |

### Section header — `vp_section_hdr_t` (20 bytes)

| Field        | Size | Notes                                         |
|--------------|------|-----------------------------------------------|
| `magic`      | 4B   | `"SECT"`                                      |
| `type`       | 1B   | section type                                  |
| `comp_method`| 1B   | 0 = none, 1 = lz4, 2 = zstd                   |
| `comp_level` | 1B   | 0–9 (validated)                               |
| `flags`      | 1B   |                                              |
| `raw_size`   | 4B   | uncompressed size                             |
| `comp_size`  | 4B   | compressed size (== raw_size when uncompressed) |
| `checksum`   | 4B   | CRC32 of raw data                             |
| `reserved`   | 4B   | zero                                          |

### Metadata block — `vp_meta_block_t`

```
tag[4]            block type tag (used for tag-based dispatch)
key_count  2B     number of entries
reserved   2B
entries[]  64B × key_count
```

Each entry is two null-terminated strings — `key[24]` and `value[40]`.

### Index

A flat 2D table of `index_width × index_depth` slots. Each `vp_index_slot_t`
is 12 bytes: `{uint32_t key; uint32_t offset; uint32_t flags;}`. The library
exposes helpers to build the table from the central directory, insert primary
and sub-granularity entries, and look up keys.

## API at a glance

| Header                           | Symbols                                                            |
|----------------------------------|--------------------------------------------------------------------|
| `vaultpack/archive.h`            | `vp_open_archive`, `vp_close_archive`, `vp_dir_iter_*`             |
| `vaultpack/section.h`            | `vp_parse_section_header`, `vp_validate_section`, `vp_fill_section_data`, `vp_section_summary`, `vp_section_cleanup` |
| `vaultpack/metadata.h`           | `vp_read_metadata_block`, `vp_metadata_lookup`, `vp_process_meta_block`, `vp_free_metadata_block` |
| `vaultpack/index.h`              | `vp_build_index`, `vp_index_insert`, `vp_index_populate`, `vp_index_lookup`, `vp_index_destroy` |
| `vaultpack/extract.h`            | `vp_extract_section`, `vp_extract_cleanup`                        |
| `vaultpack/error.h`              | `VP_OK`, `VP_ERR_*`, `vp_strerror`                                 |

## Fuzzing

The repository ships five harnesses under `fuzz/`:

- `archive_fuzzer` — exercises the full open path (header, directory, aggregate metadata)
- `section_fuzzer` — parse → validate → summary → cleanup
- `metadata_fuzzer` — block read → lookup → process (tag dispatch)
- `index_fuzzer` — build → insert loop → destroy
- `extract_fuzzer` — extract → cleanup

ClusterFuzzLite is wired up under `.clusterfuzzlite/`. To reproduce a local
smoke build:

```sh
./fuzz/verify_build.sh
./fuzz/verify_poc.sh path/to/input.bin section_fuzzer
```

To enable runtime memory checks during a local replay, set the standard
runtime knob for Clang's address-checking pass: `abort_on_error=1`,
`detect_leaks=0` (export via the upstream `ASAN_OPTIONS` variable when
invoking the harness).

## Tooling modules

Beyond the on-disk format reader and writer, VaultPack ships a few
small production modules:

| Module | Purpose |
|--------|---------|
| `stream.h` / `stream.c` | Bounded read cursor with bookmarks, scratch windows, and inline LE decoding. |
| `buffer.h` / `buffer.c` | Growable output buffer with `reserve` / `append` / `patch` APIs. |
| `crc32.h` / `crc32.c` | Table-driven CRC32 (IEEE 802.3 polynomial). |
| `writer.h` / `writer.c` | In-memory archive writer; emits sections, central directory, and metadata blocks. |
| `manifest.h` / `manifest.c` / `manifest_apply.c` | Tokenizer + parser for a text manifest that drives the writer. |
| `codec.h` / `codec.c` | Built-in codec: raw passthrough plus an RLE block encoder/decoder. |
| `validate.h` / `validate.c` | Whole-archive consistency walk with overlap detection and a structured issue report. |

The CLI (`src/vpack_cli.c`) exposes `list`, `info`, `verify`, `pack`, and
`extract` subcommands on top of these modules.

## License

MIT — see [LICENSE](LICENSE).
