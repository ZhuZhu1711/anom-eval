#pragma once

#include "dd_types.hpp"

#include <string>
#include <vector>

struct TestCase {
    std::string id;
    std::string name;
    std::string source;  // file path or "random"
    int line = 0;
    Image image;
    std::string note;
};

struct LoadResult {
    std::vector<TestCase> cases;
    std::vector<std::string> errors;
};

LoadResult load_case_file(const std::string& path);

// Compact writers used by the coverage generator.
std::string format_pair_line(const std::string& id, int w, int h, int y, int g0, u8 m0, u8 m1);
std::string format_triple_line(const std::string& id, int w, int h, int y, int g0, u8 m0, u8 m1, u8 m2);

Image image_from_packs(int w, int h, int y, int g0, const u8* masks, int nmasks);
std::string ascii_map(const Image& img);
