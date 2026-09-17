#include "algorithm_common.hpp"

WorkState make_work(const Image& src) {
    WorkState w;
    w.width = src.width;
    w.height = src.height;
    w.zwidth = src.width >> 2;
    w.map_img = src.pixels;
    const int groups = w.zwidth * w.height;
    w.type_map.assign(static_cast<size_t>(groups), 0);
    return w;
}

bool find_big_dd(WorkState& w) {
    const int width = w.width;
    const int height = w.height;
    u8* map_img = w.map_img.data();
    bool bType_bigDD = false;

    // 5x5 window: >=16 defects around a center defect.
    for (int i = 2; i < height - 2; ++i) {
        for (int j = 2; j < width - 2; ++j) {
            if (map_img[i * width + j] == 255) {
                int cnt = 0;
                for (int ii = i - 2; ii <= i + 2; ++ii)
                    for (int jj = j - 2; jj <= j + 2; ++jj)
                        if (map_img[width * ii + jj] == 255) ++cnt;
                if (cnt >= 16) {
                    ++w.big_dd_cnt;
                    bType_bigDD = true;
                }
            }
        }
    }

    // Same-channel 4 consecutive (step 2).
    for (int i = 0; i != height; ++i) {
        for (int j = 6; j != width; ++j) {
            if (map_img[i * width + j] == 255 &&
                map_img[i * width + j - 2] == 255 &&
                map_img[i * width + j - 4] == 255 &&
                map_img[i * width + j - 6] == 255) {
                ++w.big_dd_cnt;
                bType_bigDD = true;
            }
        }
    }
    return bType_bigDD;
}

void cluster_phase(WorkState& w) {
    const int n = w.width * w.height;
    u8* map_img = w.map_img.data();
    for (int i = 0, j = 0; i != n; i += 4, ++j) {
        const int sum = map_img[i] + map_img[i + 1] + map_img[i + 2] + map_img[i + 3];
        if (sum > 255) {
            // Adjacent doubles are skipped on purpose (DPC can remove them).
            if ((map_img[i] == 255 && map_img[i + 1] == 255) ||
                (map_img[i + 1] == 255 && map_img[i + 2] == 255) ||
                (map_img[i + 2] == 255 && map_img[i + 3] == 255)) {
                continue;
            }
            ++w.cluster_cnt;
            w.type_map[static_cast<size_t>(j)] =
                static_cast<u8>((map_img[i] & 0b1000) | (map_img[i + 1] & 0b0100) |
                                (map_img[i + 2] & 0b0010) | (map_img[i + 3] & 0b0001));
            map_img[i] = map_img[i + 1] = map_img[i + 2] = map_img[i + 3] = 0;
        }
    }
}

void vertical_phase(WorkState& w) {
    const int width = w.width;
    const int height = w.height;
    u8* map_img = w.map_img.data();

    // single_map: loca%4 -> type bit
    const u8 single_map[4] = {0b1000, 0b0100, 0b0010, 0b0001};

    for (int i = 0; i != height; ++i) {
        for (int j = 0; j != width; ++j) {
            if (map_img[i * width + j] == 255) {
                bool need_burn = false;
                if (i + 1 != height) {
                    if ((j - 1 >= 0 && map_img[(i + 1) * width + j - 1] == 255) ||
                        (j + 1 != width && map_img[(i + 1) * width + j + 1] == 255))
                        need_burn = true;
                }
                if (!need_burn && i + 2 != height) {
                    if (map_img[(i + 2) * width + j] == 255 ||
                        (j - 2 >= 0 && map_img[(i + 2) * width + j - 2] == 255) ||
                        (j + 2 != width && map_img[(i + 2) * width + j + 2] == 255))
                        need_burn = true;
                }
                if (need_burn) {
                    ++w.vertical_cnt;
                    const int loca = i * width + j;
                    map_img[loca] = 0;
                    w.type_map[static_cast<size_t>(loca / 4)] = single_map[loca % 4];
                }
            }
        }
    }
}

DetectResult collect_result(WorkState& w, bool early_big_dd) {
    DetectResult r;
    r.return_code = 1;
    r.big_dd_cnt = w.big_dd_cnt;
    r.early_big_dd = early_big_dd;
    r.cluster_phase_cnt = w.cluster_cnt;
    r.leftover_promoted_cnt = w.leftover_cnt;
    r.vertical_cnt = w.vertical_cnt;
    r.type_map = w.type_map;

    if (early_big_dd) {
        r.dd_total_cnt = 0;
        return r;
    }

    const int zwidth = w.zwidth;
    const int height = w.height;
    r.detections.reserve(static_cast<size_t>(w.cluster_cnt + w.leftover_cnt + w.vertical_cnt));
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < zwidth; ++j) {
            const u8 type_ = w.type_map[static_cast<size_t>(i * zwidth + j)];
            if (type_ != 0) {
                Detection d;
                d.row = i;
                d.group = j;
                d.type_bits = type_;
                d.otp_type = otp_type_from_bits(type_);
                r.otp_dd_type[static_cast<size_t>(d.otp_type - 1)]++;
                r.detections.push_back(d);
            }
        }
    }
    r.dd_total_cnt = static_cast<int>(r.detections.size());
    return r;
}
