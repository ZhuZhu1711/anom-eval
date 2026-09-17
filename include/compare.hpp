#pragma once

#include "casefile.hpp"
#include "dd_types.hpp"
#include "oracle.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct CaseScore {
    std::string id;
    std::string name;
    std::string source;
    int line = 0;
    PairKind kind = PairKind::OtherComplex;
    bool control_early_big = false;
    bool experimental_early_big = false;
    bool oracle_big = false;
    bool type_map_equal = false;
    int control_hits = 0;
    int experimental_hits = 0;
    int oracle_hits = 0;
    int control_oracle_miss = 0;       // oracle group not in control
    int experimental_oracle_miss = 0;  // oracle group not in experimental
    int control_extra = 0;
    int experimental_extra = 0;
    std::string ascii;
    std::string detail;
};

struct KindStats {
    PairKind kind = PairKind::Empty;
    int cases = 0;
    int control_full_cover = 0;        // no oracle misses
    int experimental_full_cover = 0;
    int disagree = 0;
};

struct SuiteReport {
    std::string title;
    int n_cases = 0;
    int n_errors = 0;
    int n_agree = 0;
    int n_disagree = 0;
    int n_control_miss = 0;
    int n_experimental_miss = 0;
    int n_control_extra = 0;
    int n_experimental_extra = 0;
    int n_big_dd = 0;
    double control_ms = 0;
    double experimental_ms = 0;
    std::array<KindStats, 10> by_kind{};
    std::vector<CaseScore> disagreements;
    std::vector<CaseScore> control_misses;  // sample
};

struct RandomReport {
    int iterations = 0;
    int width = 0;
    int height = 0;
    int defects = 0;
    unsigned seed = 0;
    int agree = 0;
    int disagree = 0;
    int control_miss = 0;
    int experimental_miss = 0;
    int control_extra = 0;
    int experimental_extra = 0;
    int big_dd = 0;
    double control_ms = 0;
    double experimental_ms = 0;
    std::vector<CaseScore> disagreement_samples;
};

CaseScore score_case(const TestCase& tc);
SuiteReport run_suite(const std::vector<TestCase>& cases, const std::string& title,
                      int max_samples = 40);
RandomReport run_random(int iterations, int width, int height, int defects, unsigned seed,
                        int samples = 12);

std::string render_text_report(const SuiteReport& coverage, const SuiteReport& manual,
                               const RandomReport& rnd);
std::string render_html_report(const SuiteReport& coverage, const SuiteReport& manual,
                               const RandomReport& rnd);
