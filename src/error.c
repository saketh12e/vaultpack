#include "vaultpack/error.h"

const char *vp_strerror(int code)
{
    switch (code) {
    case VP_OK:                return "ok";
    case VP_ERR_INVALID:       return "invalid input";
    case VP_ERR_NOMEM:         return "out of memory";
    case VP_ERR_TRUNCATED:     return "truncated input";
    case VP_ERR_INVALID_COMP:  return "invalid compression parameters";
    case VP_ERR_FULL:          return "index full";
    case VP_ERR_OVERFLOW:      return "numeric overflow";
    default:                   return "unknown error";
    }
}
