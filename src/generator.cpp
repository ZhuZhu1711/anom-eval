#include "generator.hpp"
#include "casefile.hpp"
#include "oracle.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

void write_generated_coverage(const std::string& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path);

    out << "# Generated high-coverage adjacent 4-pack patterns.\n";
    out << "# Do not hand-edit unless you intend to freeze a modified grid.\n";
    out << "# Format: PAIR id width height row group0 mask0 mask1\n";
    out << "#         TRIPLE id width height row group0 mask0 mask1 mask2\n";
    out << "# Masks are 0..15 (bit3=pos0 ... bit0=pos3). Width is always a multiple of 4.\n";
    out << "#\n";
    out << "# Placement:\n";
    out << "#   pair_L  groups 0-1 on W=16 (left edge)\n";
    out << "#   pair_M  groups 1-2 on W=16 (has empty neighbors)\n";
    out << "#   pair_R  groups 2-3 on W=16 (right edge)\n";
    out << "#   triple  groups 0-1-2 on W=12\n\n";

    int n = 0;
    for (int m0 = 0; m0 < 16; ++m0) {
        for (int m1 = 0; m1 < 16; ++m1) {
            auto id = [&](const char* place) {
                return std::string("p_") + place + "_" + std::to_string(m0) + "_" +
                       std::to_string(m1);
            };
            out << format_pair_line(id("L"), 16, 3, 1, 0, static_cast<u8>(m0),
                                    static_cast<u8>(m1))
                << "\n";
            out << format_pair_line(id("M"), 16, 3, 1, 1, static_cast<u8>(m0),
                                    static_cast<u8>(m1))
                << "\n";
            out << format_pair_line(id("R"), 16, 3, 1, 2, static_cast<u8>(m0),
                                    static_cast<u8>(m1))
                << "\n";
            n += 3;
        }
    }

    for (int m0 = 0; m0 < 16; ++m0) {
        for (int m1 = 0; m1 < 16; ++m1) {
            for (int m2 = 0; m2 < 16; ++m2) {
                if (m0 == 0 && m1 == 0 && m2 == 0) continue;
                const std::string id = "t_" + std::to_string(m0) + "_" + std::to_string(m1) +
                                       "_" + std::to_string(m2);
                out << format_triple_line(id, 12, 3, 1, 0, static_cast<u8>(m0),
                                          static_cast<u8>(m1), static_cast<u8>(m2))
                    << "\n";
                ++n;
            }
        }
    }
    out << "# total_cases " << n << "\n";
}

Image random_sparse_image(int width, int height, int n_defects, std::mt19937_64& rng) {
    Image img(width, height);
    if (n_defects <= 0) return img;
    const int n = width * height;
    if (n_defects >= n) {
        std::fill(img.pixels.begin(), img.pixels.end(), 255);
        return img;
    }
    std::uniform_int_distribution<int> dist(0, n - 1);
    std::unordered_set<int> used;
    used.reserve(static_cast<size_t>(n_defects) * 2);
    while (static_cast<int>(used.size()) < n_defects) {
        const int idx = dist(rng);
        if (used.insert(idx).second) img.pixels[static_cast<size_t>(idx)] = 255;
    }
    return img;
}

Image random_bernoulli_image(int width, int height, double p, std::mt19937_64& rng) {
    Image img(width, height);
    std::bernoulli_distribution bern(p);
    for (auto& v : img.pixels)
        if (bern(rng)) v = 255;
    return img;
}
