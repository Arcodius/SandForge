#include "SandWorld.h"

#include <cassert>

SandWorld::SandWorld(uint32_t width, uint32_t height)
    : m_width(width), m_height(height)
{
    m_cells.assign(width * height, MaterialEmpty);
}

uint32_t SandWorld::Width() const { return m_width; }
uint32_t SandWorld::Height() const { return m_height; }
const uint32_t* SandWorld::Data() const { return m_cells.data(); }
size_t SandWorld::ByteSize() const { return size_t(m_width) * m_height * sizeof(uint32_t); }

bool SandWorld::InBounds(int x, int y) const { return (0 <= x && x < int(m_width)) && (0 <= y && y < int(m_height)); }

Material SandWorld::MaterialAt(int x, int y) const {
    return InBounds(x, y) ? CellMaterial(m_cells[Index(x, y)]) : MaterialWall;
}

void SandWorld::Step() {
    int num1 = CountSand();
    for (int y = m_height - 2; y >= 0; --y) { // No need to scan last line
        if (m_tick & 1) {
            for (int x = m_width - 1; x >= 0; --x) {
                if (MaterialAt(x, y) == MaterialSand) {
                    MoveSand(x, y);
                }
            }
        } else {
            for (int x = 0; x < m_width; ++x) {
                if (MaterialAt(x, y) == MaterialSand) {
                    MoveSand(x, y);
                }
            }
        }
    }
    ++m_tick;
    int num2 = CountSand();
    assert(num1 == num2 && "Sand count does not equal!"); // assert takes one expression only
}

void SandWorld::MoveSand(int x, int y) {
    if (!TryMove(x, y, x, y+1)) { // bottom down
        bool goLeft = Hash(Index(x, y) ^ Hash(m_tick)) & 1;
        int dest = goLeft ? x-1 : x+1; // x-1 may be -1: MaterialAt treats it as wall
        if (!TryMove(x, y, dest, y+1)) { // one side
            TryMove(x, y, 2*x-dest, y+1); // the other way
        }
    }
}

bool SandWorld::TryMove(int x, int y, int dx, int dy) {
    if (MaterialAt(x, y) == MaterialSand && MaterialAt(dx, dy) == MaterialEmpty) {
        std::swap(m_cells[Index(x, y)], m_cells[Index(dx, dy)]);
        return true;
    }
    return false;
}

void SandWorld::Paint(int cx, int cy, int radius, Material m) {
    ++m_paintSeed;
    // r*r + r instead of r*r: rounder discs at small radii (radius 1 becomes a plus sign, not a single cell)
    const int radiusSq = radius * radius + radius;
    for (int y = cy - radius; y <= cy + radius; ++y) {
        for (int x = cx - radius; x <= cx + radius; ++x) {
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy > radiusSq || !InBounds(x, y)) {
                continue;
            }
            const Material current = MaterialAt(x, y);
            // Skip same material so a held brush doesn't re-roll shades every tick;
            // sand only pours into empty space instead of replacing walls
            if (current == m || (m == MaterialSand && current != MaterialEmpty)) {
                continue;
            }
            const uint32_t shade = m == MaterialEmpty ? 0 : Hash(Index(x, y) ^ Hash(m_paintSeed)) & 0xFF;
            m_cells[Index(x, y)] = MakeCell(m, shade);
        }
    }
}

void SandWorld::Clear() {
    m_cells.assign(m_width * m_height, MaterialEmpty);
}

void SandWorld::FillTestPattern()
{
    // Wall border around an 8x8 sand/empty checkerboard.
    constexpr uint32_t checkerSize = 8;
    for (uint32_t y = 0; y < m_height; ++y)
    {
        for (uint32_t x = 0; x < m_width; ++x)
        {
            const bool isBorder = x == 0 || y == 0 || x == m_width - 1 || y == m_height - 1;
            const bool isChecker = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
            const Material m = isBorder ? MaterialWall
                : isChecker ? MaterialSand
                : MaterialEmpty;
            const uint32_t shade = m == MaterialEmpty ? 0 : Hash(Index(x, y)) & 0xFF;
            m_cells[Index(x, y)] = MakeCell(m, shade);
        }
    }
}

int SandWorld::CountSand() const {
    int count = 0;
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            if (MaterialAt(x, y) == MaterialSand) {
                ++count;
            }
        }
    }
    return count;
}