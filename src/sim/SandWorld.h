#pragma once
#include "Material.h"
#include <vector>

class SandWorld {
public:
    SandWorld(uint32_t width, uint32_t height);
    void Step();
    void Paint(int cx, int cy, int radius, Material m);
    void FillTestPattern();
    void Clear();

    uint32_t Width() const;
    uint32_t Height() const;
    const uint32_t* Data() const;
    size_t ByteSize() const;

private:
    bool InBounds(int x, int y) const;
    uint32_t Index(int x, int y) const { return y * m_width + x; }
    Material MaterialAt(int x, int y) const;
    void MoveSand(int x, int y);
    bool TryMove(int x, int y, int dx, int dy);
    int CountSand() const;

    static inline uint32_t Hash(uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352d;
        x ^= x >> 15; x *= 0x846ca68b;
        x ^= x >> 16; return x;
    } // bool goLeft = Hash(Index(x, y) ^ Hash(m_tick)) & 1;

    uint32_t m_width, m_height;
    uint32_t m_tick = 0;
    uint32_t m_paintSeed = 0; // bumped per Paint call so repeated stamps get fresh shades
    std::vector<uint32_t> m_cells;
};