#ifndef FALLOUT_PLIB_GNW__HASH_FNV_1A_H_
#define FALLOUT_PLIB_GNW__HASH_FNV_1A_H_

#include "platform_compat.h"
#include <stdint.h>

namespace fallout {

#define HASH_FNV_1A_PRIME 0x01000193
#define HASH_FNV_1A_HASH 0x811C9DC5

unsigned int fnv1a_hash(const char* data, size_t len);
}

#endif // FALLOUT_PLIB_GNW__HASH_FNV_1A_H_
