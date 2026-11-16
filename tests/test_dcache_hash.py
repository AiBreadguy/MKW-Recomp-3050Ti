"""Compile the Vulkan-free draw-cache hash and check descriptor-key stability and separation.

Why: the draw cache's key can be hashed more cheaply, but exact equality (memcmp) must
remain the final test for cache hits. This checks different layouts, lengths, order,
and individual descriptor words against a deterministic synthetic workload.

Usage: uv run pytest tests/test_dcache_hash.py
"""
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
HDR = ROOT / "runtime/gfx/vk/exec/vk_dcache_hash.h"

HARNESS = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "vk_dcache_hash.h"

typedef struct { uint64_t w[3]; } Desc;

static uint64_t hash(uint64_t layout, const Desc *d, uint32_t n) {
    uint64_t h = vk_dcache_hash_start(layout, n);
    for (uint32_t i = 0; i < n; i++)
        h = vk_dcache_hash_step(h, d[i].w[0], d[i].w[1], d[i].w[2]);
    return vk_dcache_hash_finish(h);
}

static uint64_t seed = 0x92afd311229bd121ULL;
static uint64_t rnd(void) {
    seed ^= seed >> 12; seed ^= seed << 25; seed ^= seed >> 27;
    return seed * UINT64_C(2685821657736338717);
}

int main(void) {
    Desc d[16] = {0}, other[16];
    uint64_t base = hash(0x112233, d, 16);
    if (base != hash(0x112233, d, 16)) return 1;
    if (base == hash(0x112234, d, 16) || base == hash(0x112233, d, 15)) return 2;
    for (int i = 0; i < 16; i++) for (int j = 0; j < 3; j++) {
        memcpy(other, d, sizeof d);
        other[i].w[j] = 1;
        if (hash(0x112233, other, 16) == base) return 3;
    }
    // A zero-descriptor key must remain well-defined.
    if (hash(0, NULL, 0) != hash(0, NULL, 0)) return 4;
    // A bounded synthetic collision check. Real correctness does not depend on
    // collision avoidance: vk_dcache_get checks the complete descriptor array.
    enum { N = 20000, CAP = 65536 };
    static uint64_t keys[CAP];
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < 16; j++) for (int k = 0; k < 3; k++) d[j].w[k] = rnd();
        uint64_t h = hash(0x112233, d, 16);
        uint32_t slot = (uint32_t)h & (CAP - 1);
        while (keys[slot] && keys[slot] != h) slot = (slot + 1) & (CAP - 1);
        if (keys[slot] == h) return 5;
        keys[slot] = h;
    }
    puts("dcache hash: layout, length, 48 individual words and 20000 synthetic keys OK");
    return 0;
}
'''


def test_draw_cache_hash(tmp_path: Path) -> None:
    cc = shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")
    if not cc:
        pytest.skip("no C compiler")
    source = tmp_path / "hash.c"
    source.write_text(HARNESS)
    exe = tmp_path / "hash.exe"
    subprocess.run([cc, "-O2", "-Wall", "-Wextra", "-I", str(HDR.parent), str(source), "-o", str(exe)], check=True)
    result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
    assert "20000 synthetic keys OK" in result.stdout
