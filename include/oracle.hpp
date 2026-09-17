#pragma once

#include "dd_types.hpp"

// Independent expected-detection model used to score both algorithms.
// See src/oracle.cpp for the rules.

struct OracleHit {
    int row = 0;
    int group = 0;
    u8 bits = 0;
};

struct OracleResult {
    bool big_dd = false;
    std::vector<OracleHit> hits;  // leftover/cluster groups that must be in type_map
    std::vector<u8> type_map;
};

OracleResult run_oracle(const Image& src);

enum class PairKind {
    Empty,
    IsolatedSingle,          // one leftover 1-defect pack, DPC
    IsolatedAdjacent,        // leftover pack with an adjacent pair, DPC
    IntraCluster,            // phase-2 cluster (non-adjacent 2+ in one pack)
    SinglePlusCluster,       // leftover 1-defect pack connected to a cluster
    AdjacentPlusCluster,     // leftover adjacent-pair pack connected to a cluster
    AdjacentPlusAdjacent,    // two leftover adjacent-pair packs connected
    SinglePlusAdjacent,      // leftover 1-defect pack connected to leftover adjacent
    TwoSinglesConnected,     // two leftover singles, same-channel d=2 (DPC each)
    OtherComplex
};

const char* pair_kind_name(PairKind k);

struct PairClassification {
    u8 left = 0;
    u8 right = 0;
    PairKind kind = PairKind::Empty;
    bool oracle_records_left = false;
    bool oracle_records_right = false;
};

PairClassification classify_adjacent_packs(u8 left, u8 right);
bool pack_has_adjacent_pair(u8 bits);
bool pack_is_phase2_cluster(u8 bits);
