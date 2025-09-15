#ifndef VAULTPACK_ERROR_H
#define VAULTPACK_ERROR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VP_OK                0
#define VP_ERR_INVALID      -1
#define VP_ERR_NOMEM        -2
#define VP_ERR_TRUNCATED    -3
#define VP_ERR_INVALID_COMP -4
#define VP_ERR_FULL         -5
#define VP_ERR_OVERFLOW     -6

const char *vp_strerror(int code);

#ifdef __cplusplus
}
#endif

#endif
