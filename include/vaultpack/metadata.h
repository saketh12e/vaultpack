#ifndef VAULTPACK_METADATA_H
#define VAULTPACK_METADATA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vp_meta_entry {
    char key[24];
    char value[40];
} vp_meta_entry_t;

#ifdef __cplusplus
}
#endif

#endif
