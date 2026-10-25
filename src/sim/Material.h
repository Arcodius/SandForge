#pragma once
#include <cstdint>

enum Material : uint32_t {
    MaterialEmpty = 0,
    MaterialSand = 1,
    MaterialWall = 2,
};

// Cell layout: bits 0-7 material, bits 8-15 shade (per-grain brightness jitter).
// grid_ps.hlsl decodes the same layout - keep them in sync.
inline uint32_t MakeCell(Material m, uint32_t shade) { return uint32_t(m) | (shade << 8); }
// Take the lowest byte
inline Material CellMaterial(uint32_t cell) { return Material(cell & 0xFF); }