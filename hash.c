#include <stdint.h>
#include <string.h>

size_t hash(const char *key, int table_size) {
    uint32_t value = 2166136261u;
    size_t key_len = strlen(key);
    for (size_t i = 0; i < key_len; ++i) {
        value ^= (uint8_t)key[i];
        value *= 16777619u;
    }
    return (size_t)(value % table_size);
}