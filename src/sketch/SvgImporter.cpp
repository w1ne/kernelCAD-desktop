#include "SvgImporter.h"
#include <fstream>
#include <sstream>
#include <string>
#include <regex>

namespace sketch {

namespace {

/// Extract the value of an attribute from an SVG tag string.
/// Returns empty string if the attribute is not found.
std::string extractAttr(const std::string& tag, const std::string& attr) {
    // Match: attr="value" or attr='value'
    std::regex re(attr + R"(\s*=\s*["']([^"']*)["'])");
    std::smatch match;
    if (std::regex_search(tag, match, re) && match.size() > 1)
        return match[1].str();
    return {};
}

/// Try to parse a double from a string. Returns 0.0 on failure.
double parseDouble(const std::string& s) {
    if (s.empty()) return 0.0;
    try { return std::stod(s); } catch (...) { return 0.0; }
}

} // anonymous namespace

SvgImportResult SvgImporter::importFile(const std::string& path) {
    SvgImportResult result;

    std::ifstream file(path);
    if (!file.is_open())
        return result;

    // Read the entire file into a string
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // Find <line .../> tags
    {
        std::regex lineRe(R"(<line\s[^>]*/>)", std::regex::icase);
        auto begin = std::sregex_iterator(content.begin(), content.end(), lineRe);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            std::string tag = it->str();
            double lx1 = parseDouble(extractAttr(tag, "x1"));
            double ly1 = parseDouble(extractAttr(tag, "y1"));
            double lx2 = parseDouble(extractAttr(tag, "x2"));
            double ly2 = parseDouble(extractAttr(tag, "y2"));
            result.lines.push_back({lx1, ly1, lx2, ly2});
        }
    }

    // Find <circle .../> tags
    {
        std::regex circleRe(R"(<circle\s[^>]*/>)", std::regex::icase);
        auto begin = std::sregex_iterator(content.begin(), content.end(), circleRe);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            std::string tag = it->str();
            double ccx = parseDouble(extractAttr(tag, "cx"));
            double ccy = parseDouble(extractAttr(tag, "cy"));
            double r = parseDouble(extractAttr(tag, "r"));
            if (r > 0)
                result.circles.push_back({ccx, ccy, r});
        }
    }

    // Find <rect .../> tags
    {
        std::regex rectRe(R"(<rect\s[^>]*/>)", std::regex::icase);
        auto begin = std::sregex_iterator(content.begin(), content.end(), rectRe);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            std::string tag = it->str();
            double rx = parseDouble(extractAttr(tag, "x"));
            double ry = parseDouble(extractAttr(tag, "y"));
            double rw = parseDouble(extractAttr(tag, "width"));
            double rh = parseDouble(extractAttr(tag, "height"));
            if (rw > 0 && rh > 0)
                result.rects.push_back({rx, ry, rw, rh});
        }
    }

    return result;
}

} // namespace sketch
