/*
 * fuzz/fuzz_bridge.h
 *
 * Replay entry points used by ../bugforge/eval/verify_poc.sh on the VPS.
 * Each function mirrors the body of the corresponding `LLVMFuzzerTestOneInput`
 * in fuzz/<name>_fuzzer.cc, so the harness logic is exercisable without a
 * libFuzzer runtime.
 */
#ifndef VAULTPACK_FUZZ_BRIDGE_H
#define VAULTPACK_FUZZ_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void vp_bridge_archive(const uint8_t *data, size_t size);
void vp_bridge_section(const uint8_t *data, size_t size);
void vp_bridge_metadata(const uint8_t *data, size_t size);
void vp_bridge_index(const uint8_t *data, size_t size);
void vp_bridge_extract(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
