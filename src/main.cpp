#include "algorithm_control.hpp"
#include "algorithm_experimental.hpp"
#include "casefile.hpp"
#include "compare.hpp"
#include "generator.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void print_help() {
    std::cout
        << "dd_ab — dead-pixel cluster A/B harness\n"
        << "\n"
        << "Usage:\n"
        << "  dd_ab [options]\n"
        << "\n"
        << "Options:\n"
        << "  --data-dir DIR          directory with case files (default: testdata)\n"
        << "  --manual FILE           extra/override manual case file\n"
        << "  --coverage FILE         generated coverage file\n"
        << "  --write-coverage FILE   regenerate coverage grid and exit\n"
        << "  --random N              sparse random iterations (default 8000)\n"
        << "  --width W --height H    random image size, W multiple of 4 (default 192 96)\n"
        << "  --defects K             defects per random image (default 24)\n"
        << "  --seed S                RNG seed (default 20260917)\n"
        << "  --report-dir DIR        write report.txt and report.html (default reports)\n"
        << "  --no-random             skip random suite\n"
        << "  --dump ID               print one loaded case (ascii + both algorithms)\n"
        << "  -h, --help\n";
}

static std::vector<TestCase> load_required(const std::string& path, int& errors) {
    if (path.empty() || !fs::exists(path)) return {};
    LoadResult lr = load_case_file(path);
    for (const auto& e : lr.errors) {
        std::cerr << e << "\n";
        ++errors;
    }
    return std::move(lr.cases);
}

int main(int argc, char** argv) {
    std::string data_dir = "testdata";
    std::string manual_path;
    std::string coverage_path;
    std::string write_coverage;
    std::string report_dir = "reports";
    std::string dump_id;
    int random_n = 8000;
    int width = 192;
    int height = 96;
    int defects = 24;
    unsigned seed = 20260917;
    bool no_random = false;

    for (int i = 1; i < argc; ++i) {
        auto need = [&](int more) {
            if (i + more >= argc) {
                std::cerr << "missing value for " << argv[i] << "\n";
                std::exit(2);
            }
        };
        if (!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help")) {
            print_help();
            return 0;
        } else if (!std::strcmp(argv[i], "--data-dir")) {
            need(1);
            data_dir = argv[++i];
        } else if (!std::strcmp(argv[i], "--manual")) {
            need(1);
            manual_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--coverage")) {
            need(1);
            coverage_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--write-coverage")) {
            need(1);
            write_coverage = argv[++i];
        } else if (!std::strcmp(argv[i], "--random")) {
            need(1);
            random_n = std::atoi(argv[++i]);
        } else if (!std::strcmp(argv[i], "--width")) {
            need(1);
            width = std::atoi(argv[++i]);
        } else if (!std::strcmp(argv[i], "--height")) {
            need(1);
            height = std::atoi(argv[++i]);
        } else if (!std::strcmp(argv[i], "--defects")) {
            need(1);
            defects = std::atoi(argv[++i]);
        } else if (!std::strcmp(argv[i], "--seed")) {
            need(1);
            seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
        } else if (!std::strcmp(argv[i], "--report-dir")) {
            need(1);
            report_dir = argv[++i];
        } else if (!std::strcmp(argv[i], "--no-random")) {
            no_random = true;
        } else if (!std::strcmp(argv[i], "--dump")) {
            need(1);
            dump_id = argv[++i];
        } else {
            std::cerr << "unknown arg " << argv[i] << "\n";
            print_help();
            return 2;
        }
    }

    if (!write_coverage.empty()) {
        fs::create_directories(fs::path(write_coverage).parent_path());
        write_generated_coverage(write_coverage);
        std::cout << "wrote " << write_coverage << "\n";
        return 0;
    }

    if ((width & 3) != 0) {
        std::cerr << "width must be a multiple of 4\n";
        return 2;
    }

    if (manual_path.empty()) manual_path = data_dir + "/manual_cases.txt";
    if (coverage_path.empty()) coverage_path = data_dir + "/generated_coverage.txt";

    if (!fs::exists(coverage_path)) {
        std::cerr << "coverage file missing, generating " << coverage_path << "\n";
        fs::create_directories(fs::path(coverage_path).parent_path());
        write_generated_coverage(coverage_path);
    }

    int load_errors = 0;
    std::vector<TestCase> manual = load_required(manual_path, load_errors);
    std::vector<TestCase> coverage = load_required(coverage_path, load_errors);
    if (load_errors) {
        std::cerr << load_errors << " parse error(s)\n";
        return 1;
    }

    if (!dump_id.empty()) {
        const TestCase* found = nullptr;
        for (const auto& c : manual)
            if (c.id == dump_id) found = &c;
        if (!found)
            for (const auto& c : coverage)
                if (c.id == dump_id) found = &c;
        if (!found) {
            std::cerr << "case not found: " << dump_id << "\n";
            return 1;
        }
        CaseScore s = score_case(*found);
        std::cout << s.ascii << s.detail << "\n";
        return 0;
    }

    std::cout << "loaded manual=" << manual.size() << " coverage=" << coverage.size() << "\n";

    SuiteReport man = run_suite(manual, "Manual cases (editable)", 40);
    SuiteReport cov = run_suite(coverage, "Generated coverage grid", 20);
    RandomReport rnd{};
    if (!no_random)
        rnd = run_random(random_n, width, height, defects, seed, 12);

    const std::string text = render_text_report(cov, man, rnd);
    std::cout << text;

    fs::create_directories(report_dir);
    const std::string txt_path = report_dir + "/report.txt";
    const std::string html_path = report_dir + "/report.html";
    {
        std::ofstream f(txt_path);
        f << text;
    }
    {
        std::ofstream f(html_path);
        f << render_html_report(cov, man, rnd);
    }
    std::cout << "wrote " << txt_path << "\n";
    std::cout << "wrote " << html_path << "\n";

    // Non-zero if experimental still misses oracle on the frozen suites.
    if (man.n_experimental_miss > 0 || cov.n_experimental_miss > 0) return 1;
    return 0;
}
