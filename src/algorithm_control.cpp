#include "algorithm_control.hpp"
#include "algorithm_common.hpp"

DetectResult run_control(const Image& src) {
    if (src.width <= 0 || src.height <= 0 || (src.width & 3) != 0) {
        DetectResult r;
        r.return_code = -1;
        return r;
    }

    WorkState w = make_work(src);
    if (find_big_dd(w)) {
        return collect_result(w, true);
    }
    cluster_phase(w);

    // Horizontal singles: original pass. Isolated singles are DPC-eligible;
    // only those that match an incomplete neighbor-cluster type set are kept.
    // Groups with remaining adjacent doubles (sum > 255) are never inspected.
    const int width = w.width;
    const int height = w.height;
    u8* map_img = w.map_img.data();
    int k = 0;
    for (int i = 0; i != height; ++i) {
        for (int j = 0; j != width; j += 4, ++k) {
            const int loca = i * width + j;
            const int sum = map_img[loca] + map_img[loca + 1] + map_img[loca + 2] + map_img[loca + 3];
            if (sum == 255) {
                if (map_img[loca] == 255 && j >= 4) {
                    if (type_1000_contains(w.type_map[static_cast<size_t>(k - 1)]))
                        w.type_map[static_cast<size_t>(k)] = 0b1000;
                } else if (map_img[loca + 1] == 255 && j >= 4) {
                    if (type_0100_contains(w.type_map[static_cast<size_t>(k - 1)]))
                        w.type_map[static_cast<size_t>(k)] = 0b0100;
                } else if (map_img[loca + 2] == 255 && j + 4 < width) {
                    if (map_img[loca + 4] == 255 ||
                        type_0010_contains(w.type_map[static_cast<size_t>(k + 1)])) {
                        w.type_map[static_cast<size_t>(k)] = 0b0010;
                    }
                } else if (map_img[loca + 3] == 255 && j + 4 < width) {
                    if (map_img[loca + 5] == 255 ||
                        type_0001_contains(w.type_map[static_cast<size_t>(k + 1)]))
                        w.type_map[static_cast<size_t>(k)] = 0b0001;
                }

                if (w.type_map[static_cast<size_t>(k)] != 0) {
                    ++w.leftover_cnt;
                    map_img[loca] = map_img[loca + 1] = map_img[loca + 2] = map_img[loca + 3] = 0;
                }
            }
        }
    }

    vertical_phase(w);
    return collect_result(w, false);
}
