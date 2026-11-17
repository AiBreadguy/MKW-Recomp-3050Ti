// A fast 64-bit hash for the three-word descriptors cached by vk_exec_dcache.c.
// Hash collisions are always resolved by an exact VkDescSlot comparison in the caller.
// This header has no Vulkan dependencies so the algorithm can be unit-tested without the game or GPU.
#ifndef VK_DCACHE_HASH_H
#define VK_DCACHE_HASH_H

#include <stdint.h>

static inline uint64_t vk_dcache_rotl64(uint64_t v, int n) { return (v << n) | (v >> (64 - n)); }

static inline uint64_t vk_dcache_hash_start(uint64_t layout, uint32_t n) {
    return layout * UINT64_C(0x9E3779B97F4A7C15) ^ (uint64_t)n * UINT64_C(0xD6E8FEB86659FD93);
}

// One dependent multiplication per descriptor instead of three. This is an experimental cache-key optimization,
// not a change to descriptor equality. The pointer values and per-slot words are included without reading padding.
static inline uint64_t vk_dcache_hash_step(uint64_t h, uint64_t a, uint64_t b, uint64_t c) {
    uint64_t x = a ^ vk_dcache_rotl64(b, 21) ^ vk_dcache_rotl64(c, 42);
    h = (h ^ x) * UINT64_C(0x9E3779B185EBCA87);
    return h ^ (h >> 29);
}

static inline uint64_t vk_dcache_hash_finish(uint64_t h) {
    h ^= h >> 32;
    h *= UINT64_C(0xD6E8FEB86659FD93);
    return h ^ (h >> 32);
}

#endif
