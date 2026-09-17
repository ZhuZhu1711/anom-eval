#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Binary defect map: 255 = dead pixel (DD), 0 = good.
// Horizontal processing works on 4-pixel packs (width must be a multiple of 4).
// Pack bits: bit3=pos0, bit2=pos1, bit1=pos2, bit0=pos3.

using u8 = std::uint8_t;

struct Image {
    int width = 0;
    int height = 0;
    std::vector<u8> pixels;  // row-major, 0 or 255

    Image() = default;
    Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * h, 0) {}

    u8& at(int x, int y) { return pixels[static_cast<size_t>(y) * width + x]; }
    u8 at(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }

    void set_dd(int x, int y) {
        if (x >= 0 && y >= 0 && x < width && y < height) at(x, y) = 255;
    }

    Image clone() const { return *this; }

    std::vector<std::pair<int, int>> defect_list() const {
        std::vector<std::pair<int, int>> out;
        out.reserve(64);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (at(x, y) == 255) out.emplace_back(x, y);
            }
        }
        return out;
    }
};

struct Detection {
    int row = 0;          // pixel row
    int group = 0;        // 4-pixel group index in the row (x / 4)
    u8 type_bits = 0;     // 4-bit pack mask
    int otp_type = 0;     // 1..15
};

struct DetectResult {
    int return_code = 1;
    int dd_total_cnt = 0;
    int big_dd_cnt = 0;
    bool early_big_dd = false;
    std::array<int, 15> otp_dd_type{};  // counts for types 1..15
    std::vector<Detection> detections;  // one per non-zero type_map group
    std::vector<u8> type_map;           // height * (width/4)
    int cluster_phase_cnt = 0;
    int leftover_promoted_cnt = 0;      // experimental: leftover groups pulled in
    int vertical_cnt = 0;
};

inline int otp_type_from_bits(u8 bits) {
    // Original QMap: 0b1000->1 ... 0b1111->15, i.e. the 4-bit value itself.
    return static_cast<int>(bits);
}

inline int popcount4(u8 bits) {
    return ((bits >> 3) & 1) + ((bits >> 2) & 1) + ((bits >> 1) & 1) + (bits & 1);
}

inline u8 pack_mask(const u8* p) {
    u8 m = 0;
    if (p[0] == 255) m |= 0b1000;
    if (p[1] == 255) m |= 0b0100;
    if (p[2] == 255) m |= 0b0010;
    if (p[3] == 255) m |= 0b0001;
    return m;
}

inline bool same_channel_connect(u8 left, u8 right) {
    // Across a 4-pack boundary, same Bayer channel at distance 2:
    // left pos2 <-> right pos0, left pos3 <-> right pos1.
    return ((left & 0b0010) && (right & 0b1000)) ||
           ((left & 0b0001) && (right & 0b0100));
}

inline constexpr u8 kClusterTypeLookup[16] = {
    0,  // 0b0000 not a cluster type
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};
