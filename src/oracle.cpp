#include "oracle.hpp"

#include "algorithm_common.hpp"

bool pack_has_adjacent_pair(u8 bits) {
    return ((bits & 0b1100) == 0b1100) ||
           ((bits & 0b0110) == 0b0110) ||
           ((bits & 0b0011) == 0b0011);
}

bool pack_is_phase2_cluster(u8 bits) {
    // Phase 2 only keeps 2+ defects with no adjacent pair.
    return popcount4(bits) >= 2 && !pack_has_adjacent_pair(bits);
}

const char* pair_kind_name(PairKind k) {
    switch (k) {
        case PairKind::Empty: return "empty";
        case PairKind::IsolatedSingle: return "isolated_single";
        case PairKind::IsolatedAdjacent: return "isolated_adjacent";
        case PairKind::IntraCluster: return "intra_cluster";
        case PairKind::SinglePlusCluster: return "single_plus_cluster";
        case PairKind::AdjacentPlusCluster: return "adjacent_plus_cluster";
        case PairKind::AdjacentPlusAdjacent: return "adjacent_plus_adjacent";
        case PairKind::SinglePlusAdjacent: return "single_plus_adjacent";
        case PairKind::TwoSinglesConnected: return "two_singles_connected";
        case PairKind::OtherComplex: return "other_complex";
    }
    return "unknown";
}

PairClassification classify_adjacent_packs(u8 left, u8 right) {
    PairClassification c;
    c.left = left;
    c.right = right;

    const int nl = popcount4(left);
    const int nr = popcount4(right);
    if (nl == 0 && nr == 0) {
        c.kind = PairKind::Empty;
        return c;
    }

    const bool l_cluster = pack_is_phase2_cluster(left);
    const bool r_cluster = pack_is_phase2_cluster(right);
    const bool l_adj = pack_has_adjacent_pair(left);
    const bool r_adj = pack_has_adjacent_pair(right);
    const bool connected = same_channel_connect(left, right);

    auto record_if_cluster_or_promoted = [&](u8 bits, bool is_cluster, bool promote) {
        return is_cluster || (promote && bits != 0);
    };

    if (!connected) {
        // No same-channel d=2 link across the boundary.
        if (l_cluster) c.oracle_records_left = true;
        if (r_cluster) c.oracle_records_right = true;
        if (nl == 1 && nr == 0) c.kind = PairKind::IsolatedSingle;
        else if (nr == 1 && nl == 0) c.kind = PairKind::IsolatedSingle;
        else if (l_adj && nr == 0) c.kind = PairKind::IsolatedAdjacent;
        else if (r_adj && nl == 0) c.kind = PairKind::IsolatedAdjacent;
        else if (l_cluster || r_cluster) c.kind = PairKind::IntraCluster;
        else if (l_adj || r_adj) c.kind = PairKind::IsolatedAdjacent;
        else c.kind = PairKind::IsolatedSingle;
        return c;
    }

    // Connected across the boundary.
    const int union_n = nl + nr;
    const bool leftover_complex = union_n >= 3 || l_cluster || r_cluster ||
                                  (l_adj && r_adj) || (l_adj && nr >= 1) || (r_adj && nl >= 1);

    if (l_cluster && (nr == 1) && !r_cluster)
        c.kind = PairKind::SinglePlusCluster;
    else if (r_cluster && (nl == 1) && !l_cluster)
        c.kind = PairKind::SinglePlusCluster;
    else if ((l_cluster && r_adj) || (r_cluster && l_adj))
        c.kind = PairKind::AdjacentPlusCluster;
    else if (l_adj && r_adj)
        c.kind = PairKind::AdjacentPlusAdjacent;
    else if ((l_adj && nr == 1) || (r_adj && nl == 1))
        c.kind = PairKind::SinglePlusAdjacent;
    else if (nl == 1 && nr == 1)
        c.kind = PairKind::TwoSinglesConnected;
    else if (l_cluster || r_cluster)
        c.kind = PairKind::IntraCluster;
    else
        c.kind = PairKind::OtherComplex;

    // Oracle: record phase-2 clusters always; record leftovers only when they
    // form a complex pattern with a neighboring cluster (not two DPC singles).
    const bool promote_pair = leftover_complex && c.kind != PairKind::TwoSinglesConnected;
    c.oracle_records_left = record_if_cluster_or_promoted(left, l_cluster, promote_pair);
    c.oracle_records_right = record_if_cluster_or_promoted(right, r_cluster, promote_pair);
    return c;
}

OracleResult run_oracle(const Image& src) {
    OracleResult o;
    if (src.width <= 0 || src.height <= 0 || (src.width & 3) != 0) return o;

    WorkState w = make_work(src);
    if (find_big_dd(w)) {
        o.big_dd = true;
        o.type_map = w.type_map;
        return o;
    }

    const int width = src.width;
    const int height = src.height;
    const int zwidth = width >> 2;
    o.type_map.assign(static_cast<size_t>(zwidth * height), 0);

    // Build per-group masks from the original image (before any clearing).
    std::vector<u8> masks(static_cast<size_t>(zwidth * height), 0);
    for (int y = 0; y < height; ++y) {
        for (int g = 0; g < zwidth; ++g) {
            const int loca = y * width + g * 4;
            masks[static_cast<size_t>(y * zwidth + g)] = pack_mask(src.pixels.data() + loca);
        }
    }

    // Phase-2 clusters always expected.
    for (size_t i = 0; i < masks.size(); ++i) {
        if (pack_is_phase2_cluster(masks[i])) o.type_map[i] = masks[i];
    }

    // Promote leftovers that form a complex pattern with a neighbor cluster.
    for (int pass = 0; pass < 4; ++pass) {
        bool changed = false;
        for (int y = 0; y < height; ++y) {
            for (int g = 0; g < zwidth; ++g) {
                const int idx = y * zwidth + g;
                const u8 mask = masks[static_cast<size_t>(idx)];
                if (mask == 0 || o.type_map[static_cast<size_t>(idx)] != 0) continue;

                auto consider = [&](int ng, bool self_is_left) {
                    if (ng < 0 || ng >= zwidth) return;
                    const int nidx = y * zwidth + ng;
                    const u8 nmask = (o.type_map[static_cast<size_t>(nidx)] != 0)
                                         ? o.type_map[static_cast<size_t>(nidx)]
                                         : masks[static_cast<size_t>(nidx)];
                    const u8 L = self_is_left ? mask : nmask;
                    const u8 R = self_is_left ? nmask : mask;
                    if (!same_channel_connect(L, R)) return;
                    const bool n_cluster = o.type_map[static_cast<size_t>(nidx)] != 0 ||
                                           pack_is_phase2_cluster(nmask) ||
                                           popcount4(nmask) >= 2;
                    if (n_cluster || popcount4(mask) + popcount4(nmask) >= 3) {
                        o.type_map[static_cast<size_t>(idx)] = mask;
                        changed = true;
                    }
                };
                consider(g - 1, false);
                if (o.type_map[static_cast<size_t>(idx)] == 0) consider(g + 1, true);
            }
        }
        if (!changed) break;
    }

    // Same vertical pass as both algorithms, on pixels not already consumed.
    WorkState rest = make_work(src);
    for (int y = 0; y < height; ++y) {
        for (int g = 0; g < zwidth; ++g) {
            if (o.type_map[static_cast<size_t>(y * zwidth + g)] == 0) continue;
            const int loca = y * width + g * 4;
            rest.map_img[static_cast<size_t>(loca)] = 0;
            rest.map_img[static_cast<size_t>(loca + 1)] = 0;
            rest.map_img[static_cast<size_t>(loca + 2)] = 0;
            rest.map_img[static_cast<size_t>(loca + 3)] = 0;
        }
    }
    rest.type_map = o.type_map;
    vertical_phase(rest);
    o.type_map = rest.type_map;

    for (int y = 0; y < height; ++y) {
        for (int g = 0; g < zwidth; ++g) {
            const u8 bits = o.type_map[static_cast<size_t>(y * zwidth + g)];
            if (bits != 0) {
                OracleHit h;
                h.row = y;
                h.group = g;
                h.bits = bits;
                o.hits.push_back(h);
            }
        }
    }
    return o;
}
