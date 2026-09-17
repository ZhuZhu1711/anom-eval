#include "compare.hpp"

#include "algorithm_control.hpp"
#include "algorithm_experimental.hpp"
#include "generator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <sstream>

namespace {

int kind_index(PairKind k) { return static_cast<int>(k); }

PairKind classify_image_horizontal(const Image& img) {
    auto rank = [](PairKind k) {
        switch (k) {
            case PairKind::AdjacentPlusCluster: return 90;
            case PairKind::SinglePlusCluster: return 80;
            case PairKind::AdjacentPlusAdjacent: return 70;
            case PairKind::SinglePlusAdjacent: return 60;
            case PairKind::OtherComplex: return 50;
            case PairKind::IntraCluster: return 40;
            case PairKind::TwoSinglesConnected: return 30;
            case PairKind::IsolatedAdjacent: return 20;
            case PairKind::IsolatedSingle: return 10;
            case PairKind::Empty: return 0;
        }
        return 0;
    };
    PairKind best = PairKind::Empty;
    if (img.height <= 0 || img.width < 4) return best;
    const int z = img.width >> 2;
    for (int y = 0; y < img.height; ++y) {
        std::vector<u8> masks(static_cast<size_t>(z), 0);
        for (int g = 0; g < z; ++g)
            masks[static_cast<size_t>(g)] = pack_mask(img.pixels.data() + y * img.width + g * 4);
        if (z == 1) {
            const PairKind k = classify_adjacent_packs(masks[0], 0).kind;
            if (rank(k) > rank(best)) best = k;
            continue;
        }
        for (int g = 0; g + 1 < z; ++g) {
            const PairKind k =
                classify_adjacent_packs(masks[static_cast<size_t>(g)],
                                        masks[static_cast<size_t>(g + 1)])
                    .kind;
            if (rank(k) > rank(best)) best = k;
        }
    }
    return best;
}

int count_oracle_miss(const DetectResult& a, const OracleResult& o) {
    if (o.big_dd) return 0;
    int miss = 0;
    const size_t n = std::min(a.type_map.size(), o.type_map.size());
    for (size_t i = 0; i < n; ++i) {
        if (o.type_map[i] == 0) continue;
        // Covered if algorithm recorded the same pack bits (or a superset, in
        // case vertical later overwrites — we require the oracle bits present).
        if ((a.type_map[i] & o.type_map[i]) != o.type_map[i]) ++miss;
    }
    return miss;
}

int count_extra_vs_oracle(const DetectResult& a, const OracleResult& o) {
    if (o.big_dd || a.early_big_dd) return 0;
    int extra = 0;
    const size_t n = std::min(a.type_map.size(), o.type_map.size());
    for (size_t i = 0; i < n; ++i) {
        if (a.type_map[i] != 0 && o.type_map[i] == 0) ++extra;
    }
    return extra;
}

std::string bits_str(u8 b) {
    std::string s = "0000";
    for (int i = 0; i < 4; ++i) s[i] = (b & (8 >> i)) ? '1' : '0';
    return s;
}

std::string detections_str(const DetectResult& r) {
    std::ostringstream os;
    if (r.early_big_dd) {
        os << "BIG_DD big_dd_cnt=" << r.big_dd_cnt;
        return os.str();
    }
    os << "n=" << r.dd_total_cnt;
    for (const auto& d : r.detections) {
        os << " (" << d.row << ",g" << d.group << ",t" << bits_str(d.type_bits) << ")";
    }
    return os.str();
}

}  // namespace

CaseScore score_case(const TestCase& tc) {
    CaseScore s;
    s.id = tc.id;
    s.name = tc.name;
    s.source = tc.source;
    s.line = tc.line;
    s.ascii = ascii_map(tc.image);
    s.kind = classify_image_horizontal(tc.image);

    const DetectResult c = run_control(tc.image);
    const DetectResult e = run_experimental(tc.image);
    const OracleResult o = run_oracle(tc.image);

    s.control_early_big = c.early_big_dd;
    s.experimental_early_big = e.early_big_dd;
    s.oracle_big = o.big_dd;
    s.control_hits = c.dd_total_cnt;
    s.experimental_hits = e.dd_total_cnt;
    s.oracle_hits = static_cast<int>(o.hits.size());
    s.type_map_equal = (c.early_big_dd == e.early_big_dd) && (c.type_map == e.type_map) &&
                       (c.big_dd_cnt == e.big_dd_cnt);
    s.control_oracle_miss = count_oracle_miss(c, o);
    s.experimental_oracle_miss = count_oracle_miss(e, o);
    s.control_extra = count_extra_vs_oracle(c, o);
    s.experimental_extra = count_extra_vs_oracle(e, o);

    std::ostringstream d;
    d << "kind=" << pair_kind_name(s.kind) << "\n";
    d << "control:      " << detections_str(c) << "\n";
    d << "experimental: " << detections_str(e) << "\n";
    d << "oracle:       n=" << s.oracle_hits;
    for (const auto& h : o.hits)
        d << " (" << h.row << ",g" << h.group << ",t" << bits_str(h.bits) << ")";
    d << "\n";
    d << "control miss=" << s.control_oracle_miss << " extra=" << s.control_extra
      << " | experimental miss=" << s.experimental_oracle_miss
      << " extra=" << s.experimental_extra;
    s.detail = d.str();
    return s;
}

SuiteReport run_suite(const std::vector<TestCase>& cases, const std::string& title,
                      int max_samples) {
    SuiteReport r;
    r.title = title;
    r.n_cases = static_cast<int>(cases.size());
    for (int i = 0; i < 10; ++i) r.by_kind[static_cast<size_t>(i)].kind = static_cast<PairKind>(i);

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    for (const auto& tc : cases) {
        (void)run_control(tc.image);
    }
    r.control_ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();

    const auto t1 = clock::now();
    for (const auto& tc : cases) {
        (void)run_experimental(tc.image);
    }
    r.experimental_ms = std::chrono::duration<double, std::milli>(clock::now() - t1).count();

    for (const auto& tc : cases) {
        CaseScore s = score_case(tc);
        if (s.control_early_big || s.experimental_early_big || s.oracle_big) ++r.n_big_dd;
        if (s.type_map_equal) ++r.n_agree;
        else {
            ++r.n_disagree;
            if (static_cast<int>(r.disagreements.size()) < max_samples)
                r.disagreements.push_back(s);
        }
        if (s.control_oracle_miss > 0) {
            ++r.n_control_miss;
            if (static_cast<int>(r.control_misses.size()) < max_samples)
                r.control_misses.push_back(s);
        }
        if (s.experimental_oracle_miss > 0) ++r.n_experimental_miss;
        if (s.control_extra > 0) ++r.n_control_extra;
        if (s.experimental_extra > 0) ++r.n_experimental_extra;

        KindStats& ks = r.by_kind[static_cast<size_t>(kind_index(s.kind))];
        ++ks.cases;
        if (s.control_oracle_miss == 0) ++ks.control_full_cover;
        if (s.experimental_oracle_miss == 0) ++ks.experimental_full_cover;
        if (!s.type_map_equal) ++ks.disagree;
    }
    return r;
}

RandomReport run_random(int iterations, int width, int height, int defects, unsigned seed,
                        int samples) {
    RandomReport r;
    r.iterations = iterations;
    r.width = width;
    r.height = height;
    r.defects = defects;
    r.seed = seed;

    std::mt19937_64 rng(seed);
    using clock = std::chrono::steady_clock;
    std::vector<Image> images;
    images.reserve(static_cast<size_t>(iterations));
    for (int i = 0; i < iterations; ++i)
        images.push_back(random_sparse_image(width, height, defects, rng));

    const auto t0 = clock::now();
    std::vector<DetectResult> ctrl;
    ctrl.reserve(static_cast<size_t>(iterations));
    for (const auto& img : images) ctrl.push_back(run_control(img));
    r.control_ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();

    const auto t1 = clock::now();
    std::vector<DetectResult> exp;
    exp.reserve(static_cast<size_t>(iterations));
    for (const auto& img : images) exp.push_back(run_experimental(img));
    r.experimental_ms = std::chrono::duration<double, std::milli>(clock::now() - t1).count();

    for (int i = 0; i < iterations; ++i) {
        const OracleResult o = run_oracle(images[static_cast<size_t>(i)]);
        if (ctrl[static_cast<size_t>(i)].early_big_dd ||
            exp[static_cast<size_t>(i)].early_big_dd || o.big_dd)
            ++r.big_dd;

        const bool equal = ctrl[static_cast<size_t>(i)].early_big_dd ==
                               exp[static_cast<size_t>(i)].early_big_dd &&
                           ctrl[static_cast<size_t>(i)].type_map ==
                               exp[static_cast<size_t>(i)].type_map;
        if (equal) ++r.agree;
        else ++r.disagree;

        const int cm = count_oracle_miss(ctrl[static_cast<size_t>(i)], o);
        const int em = count_oracle_miss(exp[static_cast<size_t>(i)], o);
        if (cm > 0) ++r.control_miss;
        if (em > 0) ++r.experimental_miss;
        if (count_extra_vs_oracle(ctrl[static_cast<size_t>(i)], o) > 0) ++r.control_extra;
        if (count_extra_vs_oracle(exp[static_cast<size_t>(i)], o) > 0) ++r.experimental_extra;

        if (!equal && static_cast<int>(r.disagreement_samples.size()) < samples) {
            TestCase tc;
            tc.id = "rand_" + std::to_string(i);
            tc.name = tc.id;
            tc.source = "random";
            tc.image = images[static_cast<size_t>(i)];
            r.disagreement_samples.push_back(score_case(tc));
        }
    }
    return r;
}

static void append_kind_table(std::ostringstream& os, const SuiteReport& r) {
    os << "  kind                      cases  ctrl_cover  exp_cover  disagree\n";
    for (const auto& ks : r.by_kind) {
        if (ks.cases == 0) continue;
        os << "  " << pair_kind_name(ks.kind);
        const int pad = 24 - static_cast<int>(std::string(pair_kind_name(ks.kind)).size());
        os << std::string(static_cast<size_t>(std::max(1, pad)), ' ');
        os << ks.cases << "      " << ks.control_full_cover << "/" << ks.cases << "      "
           << ks.experimental_full_cover << "/" << ks.cases << "      " << ks.disagree << "\n";
    }
}

static std::string html_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            default: o += c; break;
        }
    }
    return o;
}

std::string render_text_report(const SuiteReport& coverage, const SuiteReport& manual,
                               const RandomReport& rnd) {
    std::ostringstream os;
    os << "==============================================\n";
    os << "  Horizontal cluster detection: A/B report\n";
    os << "==============================================\n\n";
    os << "Intent\n";
    os << "  Isolated singles and isolated adjacent doubles PASS (DPC).\n";
    os << "  Record leftovers that same-channel-connect (distance 2 across a\n";
    os << "  4-pack boundary) to a neighbor leftover or cluster. DPC prefers\n";
    os << "  the same-channel good pixel on the left; that source is then dead.\n";
    os << "  Two connected singles (..X.X...) are the split-pack form of 0b1010.\n\n";

    auto dump_suite = [&](const SuiteReport& r) {
        os << r.title << "\n";
        os << "  cases=" << r.n_cases << " agree=" << r.n_agree << " disagree=" << r.n_disagree
           << " big_dd=" << r.n_big_dd << "\n";
        os << "  oracle misses: control=" << r.n_control_miss
           << " experimental=" << r.n_experimental_miss
           << "  | extras: control=" << r.n_control_extra
           << " experimental=" << r.n_experimental_extra << "\n";
        os << "  time: control=" << r.control_ms << " ms  experimental=" << r.experimental_ms
           << " ms\n";
        append_kind_table(os, r);
        os << "\n";
    };
    dump_suite(manual);
    dump_suite(coverage);

    os << "Random sparse  iterations=" << rnd.iterations << "  " << rnd.width << "x" << rnd.height
       << "  defects=" << rnd.defects << "  seed=" << rnd.seed << "\n";
    os << "  agree=" << rnd.agree << " disagree=" << rnd.disagree << " big_dd=" << rnd.big_dd
       << "\n";
    os << "  oracle misses: control=" << rnd.control_miss
       << " experimental=" << rnd.experimental_miss
       << "  | extras: control=" << rnd.control_extra
       << " experimental=" << rnd.experimental_extra << "\n";
    os << "  time: control=" << rnd.control_ms << " ms  experimental=" << rnd.experimental_ms
       << " ms\n";
    if (rnd.iterations > 0) {
        os << "  per-image: control=" << (rnd.control_ms / rnd.iterations) << " ms  experimental="
           << (rnd.experimental_ms / rnd.iterations) << " ms\n";
    }
    os << "\n";

    os << "Known-bug samples (control misses oracle)\n";
    int shown = 0;
    for (const auto& s : manual.control_misses) {
        if (shown >= 12) break;
        os << "---- " << s.id << " (" << pair_kind_name(s.kind) << ")\n";
        os << s.ascii;
        os << s.detail << "\n\n";
        ++shown;
    }
    if (coverage.control_misses.size() && shown < 8) {
        for (const auto& s : coverage.control_misses) {
            if (shown >= 8) break;
            os << "---- " << s.id << " (" << pair_kind_name(s.kind) << ")\n";
            os << s.ascii;
            os << s.detail << "\n\n";
            ++shown;
        }
    }
    return os.str();
}

std::string render_html_report(const SuiteReport& coverage, const SuiteReport& manual,
                               const RandomReport& rnd) {
    std::ostringstream os;
    os << "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">";
    os << "<title>坏点簇检测 A/B 报告</title><style>";
    os << "body{font-family:ui-sans-serif,system-ui,sans-serif;margin:24px;max-width:1100px;";
    os << "background:#0f1419;color:#e7ecf1;} h1,h2{color:#fff;} ";
    os << "code,pre{font-family:ui-monospace,Menlo,Consolas,monospace;} ";
    os << "pre{background:#1a2330;padding:12px;border-radius:8px;overflow:auto;} ";
    os << "table{border-collapse:collapse;width:100%;margin:12px 0;} ";
    os << "th,td{border:1px solid #2b3a4d;padding:8px 10px;text-align:left;} ";
    os << "th{background:#1a2330;} .ok{color:#3dd68c;} .bad{color:#ff6b6b;} ";
    os << ".card{background:#151c25;border:1px solid #2b3a4d;border-radius:12px;";
    os << "padding:16px;margin:16px 0;} .muted{color:#9aa8b5;}";
    os << "</style></head><body>";
    os << "<h1>水平方向查点：对照组 vs 实验组</h1>";
    os << "<p>孤立单坏点和孤立水平相邻双坏点故意放过（DPC 可消）。";
    os << "跨组同通道、距离 2 相连时，DPC 要用的左边好点就是坏点，<strong>必须</strong>记簇。";
    os << "这包括和相邻簇拼接，也包括两个隔组单点（几何上等于组内 <code>0b1010</code>）。";
    os << "实验组按这条补水平查点，并写入完整 mask。</p>";

    auto suite_html = [&](const SuiteReport& r) {
        os << "<div class=\"card\"><h2>" << html_escape(r.title) << "</h2>";
        os << "<p>用例 " << r.n_cases << " · 两算法一致 " << r.n_agree << " · 不一致 "
           << r.n_disagree << " · 大簇早退 " << r.n_big_dd << "</p>";
        os << "<p>相对 oracle 漏检：对照 <span class=\"bad\">" << r.n_control_miss
           << "</span> · 实验 <span class=\"ok\">" << r.n_experimental_miss
           << "</span>；多检（含垂直）对照 "
           << r.n_control_extra << " · 实验 " << r.n_experimental_extra << "</p>";
        os << "<p class=\"muted\">耗时 对照 " << r.control_ms << " ms / 实验 "
           << r.experimental_ms << " ms</p>";
        os << "<table><tr><th>模式</th><th>用例</th><th>对照覆盖</th><th>实验覆盖</th>";
        os << "<th>两算法不一致</th></tr>";
        for (const auto& ks : r.by_kind) {
            if (ks.cases == 0) continue;
            os << "<tr><td>" << pair_kind_name(ks.kind) << "</td><td>" << ks.cases << "</td><td>"
               << ks.control_full_cover << "/" << ks.cases << "</td><td>"
               << ks.experimental_full_cover << "/" << ks.cases << "</td><td>" << ks.disagree
               << "</td></tr>";
        }
        os << "</table></div>";
    };
    suite_html(manual);
    suite_html(coverage);

    os << "<div class=\"card\"><h2>随机稀疏高频次</h2>";
    os << "<p>" << rnd.iterations << " 次 · " << rnd.width << "×" << rnd.height << " · 每图 "
       << rnd.defects << " 个坏点 · seed " << rnd.seed << "</p>";
    os << "<p>一致 " << rnd.agree << " · 不一致 " << rnd.disagree << " · 大簇 " << rnd.big_dd
       << "</p>";
    os << "<p>oracle 漏检：对照 <span class=\"bad\">" << rnd.control_miss
       << "</span> · 实验 <span class=\"ok\">" << rnd.experimental_miss
       << "</span>；多检 对照 " << rnd.control_extra << " · 实验 " << rnd.experimental_extra
       << "</p>";
    os << "<p class=\"muted\">总耗时 对照 " << rnd.control_ms << " ms / 实验 "
       << rnd.experimental_ms << " ms";
    if (rnd.iterations)
        os << " （每图 " << (rnd.control_ms / rnd.iterations) << " / "
           << (rnd.experimental_ms / rnd.iterations) << " ms）";
    os << "</p></div>";

    os << "<div class=\"card\"><h2>对照漏检样例（手工用例）</h2>";
    int n = 0;
    for (const auto& s : manual.control_misses) {
        if (n++ >= 10) break;
        os << "<h3>" << html_escape(s.id) << " <span class=\"muted\">"
           << pair_kind_name(s.kind) << "</span></h3>";
        os << "<pre>" << html_escape(s.ascii) << html_escape(s.detail) << "</pre>";
    }
    os << "</div>";

    os << "<div class=\"card\"><h2>随机不一致样例</h2>";
    n = 0;
    for (const auto& s : rnd.disagreement_samples) {
        if (n++ >= 8) break;
        os << "<h3>" << html_escape(s.id) << "</h3>";
        os << "<pre>" << html_escape(s.ascii) << html_escape(s.detail) << "</pre>";
    }
    if (rnd.disagreement_samples.empty())
        os << "<p class=\"muted\">本次随机种子下没有对照/实验 type_map 不一致的样本。</p>";
    os << "</div></body></html>";
    return os.str();
}
