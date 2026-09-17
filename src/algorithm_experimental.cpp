#include "algorithm_experimental.hpp"
#include "algorithm_common.hpp"

DetectResult run_experimental(const Image& src) {
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

    // Horizontal leftover pass (the fix).
    //
    // Design:
    // - Isolated singles pass (DPC).
    // - Isolated adjacent doubles pass (DPC) — that skip is intentional.
    // - A leftover 4-pack is recorded only when it same-channel-connects
    //   (distance 2 across the pack boundary) to a neighboring *cluster*,
    //   forming a more complex multi-defect pattern that DPC cannot remove.
    //   Neighbor cluster = phase-2 type_map entry, or leftover pack with
    //   2+ defects (the passed adjacent-double "cluster").
    // - Two connected singles (union=2) stay unrecorded: each is still DPC.
    // - The full leftover mask is written, so adjacent doubles next to a
    //   cluster keep their real type (0b1100 / 0b0110 / 0b0011 / triples).
    //
    // Original bugs this replaces:
    // 1. Only sum==255 was inspected, so leftover adjacent doubles next to
    //    a cluster were never considered.
    // 2. type_0100 omitted 0b1001, so a single at pos1 next to cluster
    //    0b1001 was missed.
    // 3. Matching used incomplete type sets instead of the partner bit.

    const int width = w.width;
    const int height = w.height;
    u8* map_img = w.map_img.data();
    int k = 0;
    for (int i = 0; i != height; ++i) {
        for (int j = 0; j != width; j += 4, ++k) {
            const int loca = i * width + j;
            const u8 mask = pack_mask(map_img + loca);
            if (mask == 0) continue;

            auto neighbor_pattern = [&](int nk, int nloca) -> u8 {
                if (w.type_map[static_cast<size_t>(nk)] != 0)
                    return w.type_map[static_cast<size_t>(nk)];
                return pack_mask(map_img + nloca);
            };

            bool promote = false;

            if (j >= 4) {
                const u8 prev = neighbor_pattern(k - 1, loca - 4);
                if (same_channel_connect(prev, mask)) {
                    const bool prev_cluster =
                        w.type_map[static_cast<size_t>(k - 1)] != 0 || popcount4(prev) >= 2;
                    if (prev_cluster || popcount4(mask) + popcount4(prev) >= 3)
                        promote = true;
                }
            }
            if (!promote && j + 4 < width) {
                const u8 next = neighbor_pattern(k + 1, loca + 4);
                if (same_channel_connect(mask, next)) {
                    const bool next_cluster =
                        w.type_map[static_cast<size_t>(k + 1)] != 0 || popcount4(next) >= 2;
                    if (next_cluster || popcount4(mask) + popcount4(next) >= 3)
                        promote = true;
                }
            }

            if (promote) {
                w.type_map[static_cast<size_t>(k)] = mask;
                ++w.leftover_cnt;
                map_img[loca] = map_img[loca + 1] = map_img[loca + 2] = map_img[loca + 3] = 0;
            }
        }
    }

    vertical_phase(w);
    return collect_result(w, false);
}
