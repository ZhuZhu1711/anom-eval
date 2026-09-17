#include "casefile.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

static bool starts_with(const std::string& s, const char* pfx) {
    const size_t n = std::char_traits<char>::length(pfx);
    return s.size() >= n && s.compare(0, n, pfx) == 0;
}

Image image_from_packs(int w, int h, int y, int g0, const u8* masks, int nmasks) {
    Image img(w, h);
    for (int i = 0; i < nmasks; ++i) {
        const int x0 = (g0 + i) * 4;
        const u8 m = masks[i];
        if (m & 0b1000) img.set_dd(x0 + 0, y);
        if (m & 0b0100) img.set_dd(x0 + 1, y);
        if (m & 0b0010) img.set_dd(x0 + 2, y);
        if (m & 0b0001) img.set_dd(x0 + 3, y);
    }
    return img;
}

std::string ascii_map(const Image& img) {
    std::string s;
    s.reserve(static_cast<size_t>(img.height) * (img.width + 1));
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) s.push_back(img.at(x, y) == 255 ? 'X' : '.');
        s.push_back('\n');
    }
    return s;
}

std::string format_pair_line(const std::string& id, int w, int h, int y, int g0, u8 m0, u8 m1) {
    std::ostringstream os;
    os << "PAIR " << id << ' ' << w << ' ' << h << ' ' << y << ' ' << g0 << ' '
       << int(m0) << ' ' << int(m1);
    return os.str();
}

std::string format_triple_line(const std::string& id, int w, int h, int y, int g0, u8 m0, u8 m1,
                               u8 m2) {
    std::ostringstream os;
    os << "TRIPLE " << id << ' ' << w << ' ' << h << ' ' << y << ' ' << g0 << ' '
       << int(m0) << ' ' << int(m1) << ' ' << int(m2);
    return os.str();
}

static Image parse_pair_like(const std::vector<std::string>& tok, bool triple, std::string& err) {
    // PAIR id w h y g0 m0 m1
    // TRIPLE id w h y g0 m0 m1 m2
    const size_t need = triple ? 9 : 8;
    if (tok.size() < need) {
        err = "not enough fields";
        return {};
    }
    const int w = std::stoi(tok[2]);
    const int h = std::stoi(tok[3]);
    const int y = std::stoi(tok[4]);
    const int g0 = std::stoi(tok[5]);
    u8 masks[3];
    masks[0] = static_cast<u8>(std::stoi(tok[6]));
    masks[1] = static_cast<u8>(std::stoi(tok[7]));
    int n = 2;
    if (triple) {
        masks[2] = static_cast<u8>(std::stoi(tok[8]));
        n = 3;
    }
    if (w <= 0 || h <= 0 || (w & 3) || y < 0 || y >= h || g0 < 0) {
        err = "invalid geometry";
        return {};
    }
    if ((g0 + n) * 4 > w) {
        err = "packs exceed width";
        return {};
    }
    return image_from_packs(w, h, y, g0, masks, n);
}

LoadResult load_case_file(const std::string& path) {
    LoadResult out;
    std::ifstream in(path);
    if (!in) {
        out.errors.push_back("cannot open " + path);
        return out;
    }

    std::string line;
    int lineno = 0;
    TestCase cur;
    bool in_case = false;
    bool in_map = false;
    std::vector<std::string> map_rows;
    int expect_w = 0, expect_h = 0;

    auto flush_case = [&](int at_line) {
        if (!in_case) return;
        if (expect_w <= 0 || expect_h <= 0) {
            out.errors.push_back(path + ":" + std::to_string(at_line) + " missing W/H");
        } else if (!map_rows.empty()) {
            if (static_cast<int>(map_rows.size()) != expect_h) {
                out.errors.push_back(path + ":" + std::to_string(at_line) +
                                     " MAP height mismatch");
            } else {
                Image img(expect_w, expect_h);
                for (int y = 0; y < expect_h; ++y) {
                    if (static_cast<int>(map_rows[static_cast<size_t>(y)].size()) != expect_w) {
                        out.errors.push_back(path + ":" + std::to_string(at_line) +
                                             " MAP width mismatch at row " + std::to_string(y));
                        cur = TestCase{};
                        in_case = false;
                        in_map = false;
                        map_rows.clear();
                        return;
                    }
                    for (int x = 0; x < expect_w; ++x) {
                        const char ch = map_rows[static_cast<size_t>(y)][static_cast<size_t>(x)];
                        if (ch == 'X' || ch == 'x' || ch == '1') img.at(x, y) = 255;
                        else img.at(x, y) = 0;
                    }
                }
                cur.image = std::move(img);
            }
        }
        if (cur.image.width > 0) out.cases.push_back(std::move(cur));
        cur = TestCase{};
        in_case = false;
        in_map = false;
        map_rows.clear();
        expect_w = expect_h = 0;
    };

    while (std::getline(in, line)) {
        ++lineno;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        std::istringstream ls(t);
        std::vector<std::string> tok;
        for (std::string w; ls >> w;) tok.push_back(w);

        if (starts_with(t, "PAIR ") || starts_with(t, "TRIPLE ")) {
            flush_case(lineno);
            std::string err;
            const bool triple = starts_with(t, "TRIPLE ");
            Image img = parse_pair_like(tok, triple, err);
            if (!err.empty() || img.width == 0) {
                out.errors.push_back(path + ":" + std::to_string(lineno) + " " + err);
                continue;
            }
            TestCase c;
            c.id = tok[1];
            c.name = tok[1];
            c.source = path;
            c.line = lineno;
            c.image = std::move(img);
            out.cases.push_back(std::move(c));
            continue;
        }

        if (tok[0] == "CASE") {
            flush_case(lineno);
            in_case = true;
            cur.source = path;
            cur.line = lineno;
            cur.id = tok.size() > 1 ? tok[1] : ("case_" + std::to_string(lineno));
            cur.name = cur.id;
            if (tok.size() > 2) {
                std::ostringstream ns;
                for (size_t i = 2; i < tok.size(); ++i) {
                    if (i > 2) ns << ' ';
                    ns << tok[i];
                }
                cur.name = ns.str();
            }
            continue;
        }
        if (!in_case) {
            out.errors.push_back(path + ":" + std::to_string(lineno) + " stray line");
            continue;
        }
        if (tok[0] == "W" && tok.size() >= 2) {
            expect_w = std::stoi(tok[1]);
            continue;
        }
        if (tok[0] == "H" && tok.size() >= 2) {
            expect_h = std::stoi(tok[1]);
            continue;
        }
        if (tok[0] == "NOTE") {
            cur.note = t.size() > 5 ? trim(t.substr(5)) : "";
            continue;
        }
        if (tok[0] == "MAP") {
            in_map = true;
            continue;
        }
        if (tok[0] == "X" && tok.size() >= 3) {
            if (expect_w <= 0 || expect_h <= 0) {
                out.errors.push_back(path + ":" + std::to_string(lineno) + " X before W/H");
                continue;
            }
            if (cur.image.width == 0) cur.image = Image(expect_w, expect_h);
            cur.image.set_dd(std::stoi(tok[1]), std::stoi(tok[2]));
            continue;
        }
        if (tok[0] == "END") {
            in_map = false;
            flush_case(lineno);
            continue;
        }
        if (in_map) {
            map_rows.push_back(t);
            continue;
        }
        out.errors.push_back(path + ":" + std::to_string(lineno) + " unknown directive " + tok[0]);
    }
    flush_case(lineno);
    return out;
}
