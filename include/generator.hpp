#pragma once

#include "dd_types.hpp"

#include <random>
#include <string>
#include <vector>

struct GenConfig {
    int width = 16;
    int height = 4;
    unsigned seed = 1;
};

void write_generated_coverage(const std::string& path);

Image random_sparse_image(int width, int height, int n_defects, std::mt19937_64& rng);
Image random_bernoulli_image(int width, int height, double p, std::mt19937_64& rng);
