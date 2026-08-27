#include "plib/gnw/hash_fnv-1a.h"

namespace fallout {

unsigned int fnv1a_hash(const char* data, size_t len)
{
    const unsigned int prime = HASH_FNV_1A_PRIME;
    unsigned int hash = HASH_FNV_1A_HASH;

    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<unsigned int>(data[i]);
        hash *= prime;
    }
    return hash;
}

// Пример использования:
// const char* str = "hello";
// uint32_t hash = fnv1a_hash(str, 5);

}