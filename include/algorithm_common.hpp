#pragma once

#include "dd_types.hpp"

#include <array>

// Shared pipeline pieces. Control and experimental only differ in the
// horizontal leftover pass.

struct WorkState {
    int width = 0;
    int height = 0;
    int zwidth = 0;
    std::vector<u8> map_img;
    std::vector<u8> type_map;
    int big_dd_cnt = 0;
    int cluster_cnt = 0;
    int leftover_cnt = 0;
    int vertical_cnt = 0;
};

WorkState make_work(const Image& src);
bool find_big_dd(WorkState& w);
void cluster_phase(WorkState& w);
void vertical_phase(WorkState& w);
DetectResult collect_result(WorkState& w, bool early_big_dd);

// Original incomplete type sets (control only).
inline bool type_1000_contains(u8 t) {
    return t == 0b1010 || t == 0b1110 || t == 0b1011 || t == 0b1111;
}
inline bool type_0100_contains(u8 t) {
    return t == 0b0101 || t == 0b1101 || t == 0b0111 || t == 0b1111;
}
inline bool type_0010_contains(u8 t) {
    return t == 0b1100 || t == 0b1010 || t == 0b1001 || t == 0b1011 ||
           t == 0b1101 || t == 0b1110 || t == 0b1111;
}
inline bool type_0001_contains(u8 t) {
    return t == 0b1100 || t == 0b0110 || t == 0b0101 || t == 0b0111 ||
           t == 0b1101 || t == 0b1110 || t == 0b1111;
}
