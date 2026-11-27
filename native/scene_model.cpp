#include "scene_model.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace marila {

std::string strip_visual_comments(const std::string& source) {
    std::string result = source;
    char quote = '\0';
    bool escaped = false;
    bool line_comment = false;
    bool block_comment = false;
    bool line_has_code = false;

    for (size_t index = 0; index < result.size(); ++index) {
        char& character = result[index];
        if (line_comment) {
            if (character == '\n' || character == '\r') {
                line_comment = false;
                line_has_code = false;
            } else {
                character = ' ';
            }
            continue;
        }
        if (block_comment) {
            if (character == '*' && index + 1 < result.size() && result[index + 1] == '/') {
                character = ' ';
                result[++index] = ' ';
                block_comment = false;
            } else if (character != '\n' && character != '\r') {
                character = ' ';
            } else {
                line_has_code = false;
            }
            continue;
        }
        if (quote != '\0') {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == quote) quote = '\0';
            if (!std::isspace(static_cast<unsigned char>(character))) line_has_code = true;
            continue;
        }
        if (character == '"' || character == '\'') {
            quote = character;
            line_has_code = true;
            continue;
        }
        if (character == '/' && index + 1 < result.size() && result[index + 1] == '/') {
            character = ' ';
            result[++index] = ' ';
            line_comment = true;
            continue;
        }
        if (character == '/' && index + 1 < result.size() && result[index + 1] == '*') {
            character = ' ';
            result[++index] = ' ';
            block_comment = true;
            continue;
        }
        if ((character == '#' || character == '%') && !line_has_code) {
            character = ' ';
            line_comment = true;
            continue;
        }
        if (character == '\n' || character == '\r') line_has_code = false;
        else if (!std::isspace(static_cast<unsigned char>(character))) line_has_code = true;
    }
    return result;
}

double domain_value(const std::string& text) {
    class Parser {
    public:
        explicit Parser(const std::string& input) : input_(input) {}

        double parse() {
            const double result = expression();
            skip_space();
            if (position_ != input_.size()) throw std::invalid_argument("invalid graph domain endpoint: " + input_);
            return result;
        }

    private:
        const std::string& input_;
        size_t position_{0};

        void skip_space() {
            while (position_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[position_]))) ++position_;
        }

        bool take(char expected) {
            skip_space();
            if (position_ >= input_.size() || input_[position_] != expected) return false;
            ++position_;
            return true;
        }

        double expression() {
            double result = term();
            while (true) {
                if (take('+')) result += term();
                else if (take('-')) result -= term();
                else return result;
            }
        }

        double term() {
            double result = factor();
            while (true) {
                if (take('*')) result *= factor();
                else if (take('/')) result /= factor();
                else return result;
            }
        }

        double factor() {
            skip_space();
            if (take('+')) return factor();
            if (take('-')) return -factor();
            if (take('(')) {
                const double result = expression();
                if (!take(')')) throw std::invalid_argument("unclosed graph domain endpoint: " + input_);
                return result;
            }
            if (position_ + 2 <= input_.size() && input_.compare(position_, 2, "pi") == 0) {
                position_ += 2;
                return 3.14159265358979323846;
            }
            if (position_ + 1 <= input_.size() && input_[position_] == 'e') {
                ++position_;
                return 2.71828182845904523536;
            }
            const char* start = input_.c_str() + position_;
            char* end = nullptr;
            const double result = std::strtod(start, &end);
            if (end == start) throw std::invalid_argument("invalid graph domain endpoint: " + input_);
            position_ += static_cast<size_t>(end - start);
            return result;
        }
    } parser(text);
    return parser.parse();
}

std::vector<SceneNode> parse_visual_source(const std::string& raw_source) {
    const std::string source = strip_visual_comments(raw_source);
    std::vector<SceneNode> nodes;
    std::smatch match;

    std::regex point(R"MARILA(point\s+([A-Za-z_]\w*)\s+at\s*\(\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*,\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*\)(?:\s+label\s+"([^"]*)")?)MARILA");
    for (std::sregex_iterator it(source.begin(), source.end(), point), end; it != end; ++it) {
        const size_t line_start = source.rfind('\n', it->position()) == std::string::npos ? 0 : source.rfind('\n', it->position()) + 1;
        const size_t line_end = source.find('\n', it->position());
        if (source.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start).find("moves to") != std::string::npos) continue;
        SceneNode node{SceneNode::Kind::Point}; node.id = (*it)[1]; node.x = std::stod((*it)[2]); node.y = std::stod((*it)[3]); node.label = (*it)[4].str(); nodes.push_back(node);
    }
    std::regex move(R"(point\s+([A-Za-z_]\w*)\s+at\s*\(\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*,\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*\)\s+moves\s+to\s*\(\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*,\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*\)(?:\s+over\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s)?(?:\s+ease\s+(linear|in-out|in|out))?(?:\s+at\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s)?(?:\s+repeat\s+([1-9][0-9]*)(?:\s+(yoyo))?)?)");
    for (std::sregex_iterator it(source.begin(), source.end(), move), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Move}; node.id = (*it)[1]; node.first = (*it)[1]; node.from_x = std::stod((*it)[2]); node.from_y = std::stod((*it)[3]);
        node.x = std::stod((*it)[4]); node.y = std::stod((*it)[5]); if ((*it)[6].matched) node.duration = std::stod((*it)[6]); if ((*it)[7].matched) node.easing = (*it)[7]; if ((*it)[8].matched) node.start_time = std::stod((*it)[8]); if ((*it)[9].matched) node.repeat = std::stoi((*it)[9]); node.yoyo = (*it)[10].matched; nodes.push_back(node);
    }

    std::regex fade(R"(fade\s+(in|out)\s+([A-Za-z_]\w*)(?:\s+over\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s)?(?:\s+ease\s+(linear|in-out|in|out))?(?:\s+at\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s)?(?:\s+repeat\s+([1-9][0-9]*)(?:\s+(yoyo))?)?)");
    for (std::sregex_iterator it(source.begin(), source.end(), fade), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Fade}; node.fade_in = (*it)[1] == "in"; node.first = (*it)[2]; node.id = node.first;
        if ((*it)[3].matched) node.duration = std::stod((*it)[3]); if ((*it)[4].matched) node.easing = (*it)[4]; if ((*it)[5].matched) node.start_time = std::stod((*it)[5]); if ((*it)[6].matched) node.repeat = std::stoi((*it)[6]); node.yoyo = (*it)[7].matched; nodes.push_back(node);
    }

    std::regex wait(R"(wait\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s(?:\s+at\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s)?)");
    for (std::sregex_iterator it(source.begin(), source.end(), wait), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Wait}; node.duration = std::stod((*it)[1]); if ((*it)[2].matched) node.start_time = std::stod((*it)[2]); nodes.push_back(node);
    }

    std::regex line(R"(line\s+from\s+([A-Za-z_]\w*)\s+to\s+([A-Za-z_]\w*))");
    for (std::sregex_iterator it(source.begin(), source.end(), line), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Line}; node.first = (*it)[1]; node.second = (*it)[2]; nodes.push_back(node);
    }

    std::regex triangle(R"(triangle\s+([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s+([A-Za-z_]\w*)([^\r\n\]]*))");
    for (std::sregex_iterator it(source.begin(), source.end(), triangle), end; it != end; ++it) {
        if ((*it)[2] == "with" || (*it)[3] == "color") continue;
        SceneNode node{SceneNode::Kind::Triangle}; node.first = (*it)[1]; node.second = (*it)[2]; node.third = (*it)[3];
        const std::string options = (*it)[4]; node.show_angles = options.find("show angles") != std::string::npos;
        std::smatch color; if (std::regex_search(options, color, std::regex(R"MARILA(color\s+"([^"]+)")MARILA"))) node.color = color[1];
        nodes.push_back(node);
    }
    std::regex compact_triangle(R"(triangle\s+([A-Za-z_])([A-Za-z_])([A-Za-z_])([^\r\n\]]*))");
    for (std::sregex_iterator it(source.begin(), source.end(), compact_triangle), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Triangle}; node.first = (*it)[1]; node.second = (*it)[2]; node.third = (*it)[3];
        const std::string options = (*it)[4]; node.show_angles = options.find("show angles") != std::string::npos;
        std::smatch color; if (std::regex_search(options, color, std::regex(R"MARILA(color\s+"([^"]+)")MARILA"))) node.color = color[1];
        nodes.push_back(node);
    }

    std::regex circle(R"(circle(?:\s+at)?\s+([A-Za-z_]\w*)\s+radius\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)([^\r\n\]]*))");
    for (std::sregex_iterator it(source.begin(), source.end(), circle), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Circle}; node.first = (*it)[1]; node.radius = std::stod((*it)[2]); node.dashed = it->str(3).find("dash") != std::string::npos; nodes.push_back(node);
    }

    std::regex parametric_line(R"(plot2d\s+parametric\s*(.*))");
    auto extract_title = [](const std::string& line) {
        std::smatch title; return std::regex_search(line, title, std::regex(R"MARILA(title\s+"([^"]+)")MARILA")) ? title[1].str() : std::string();
    };
    auto extract_color = [](const std::string& line) {
        std::smatch color; return std::regex_search(line, color, std::regex(R"MARILA(color\s+"([^"]+)")MARILA")) ? color[1].str() : std::string();
    };
    auto extract_trace_animation = [](SceneNode& node, const std::string& line) {
        std::smatch animation;
        const std::regex pattern(R"MARILA(animate\s+trace(?:\s+over\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s)?(?:\s+ease\s+(linear|in-out|in|out))?(?:\s+at\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s)?(?:\s+repeat\s+([1-9][0-9]*)(?:\s+(yoyo))?)?)MARILA");
        if (!std::regex_search(line, animation, pattern)) return;
        node.animate_trace = true;
        if (animation[1].matched) node.duration = std::stod(animation[1]);
        if (animation[2].matched) node.easing = animation[2];
        if (animation[3].matched) node.start_time = std::stod(animation[3]);
        if (animation[4].matched) node.repeat = std::stoi(animation[4]);
        node.yoyo = animation[5].matched;
    };
    std::stringstream source_lines(source); std::string source_line;
    while (std::getline(source_lines, source_line)) {
        if (!std::regex_match(source_line, match, parametric_line)) continue;
        const std::string tail = match[1]; const size_t open = tail.find('('); if (open == std::string::npos) continue;
        int depth = 0; size_t comma = std::string::npos, close = std::string::npos;
        for (size_t index = open; index < tail.size(); ++index) {
            if (tail[index] == '(') ++depth;
            else if (tail[index] == ')') { --depth; if (depth == 0) { close = index; break; } }
            else if (tail[index] == ',' && depth == 1) comma = index;
        }
        if (comma == std::string::npos || close == std::string::npos) continue;
        auto trim = [](std::string value) { const size_t first = value.find_first_not_of(" \t"); const size_t last = value.find_last_not_of(" \t"); return first == std::string::npos ? std::string() : value.substr(first, last - first + 1); };
        SceneNode node{SceneNode::Kind::Parametric2D}; node.first = trim(tail.substr(open + 1, comma - open - 1)); node.second = trim(tail.substr(comma + 1, close - comma - 1));
        node.title = extract_title(source_line);
        node.color = extract_color(source_line);
        extract_trace_animation(node, source_line);
        const std::string domain = tail.substr(close + 1);
        if (std::regex_search(domain, match, std::regex(R"(\s+over\s+t\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\](?:\s+y\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\])?)"))) {
            node.xmin = domain_value(match[1]); node.xmax = domain_value(match[2]);
            if (match[3].matched) { node.ymin = domain_value(match[3]); node.ymax = domain_value(match[4]); }
        }
        nodes.push_back(node);
    }

    std::regex polar_line(R"(plot2d\s+polar\s*(.*))");
    std::stringstream polar_lines(source);
    while (std::getline(polar_lines, source_line)) {
        if (!std::regex_match(source_line, match, polar_line)) continue;
        const std::string tail = match[1]; const size_t open = tail.find('('); if (open == std::string::npos) continue;
        int depth = 0; size_t close = std::string::npos;
        for (size_t index = open; index < tail.size(); ++index) { if (tail[index] == '(') ++depth; else if (tail[index] == ')' && --depth == 0) { close = index; break; } }
        if (close == std::string::npos) continue;
        SceneNode node{SceneNode::Kind::Parametric2D}; const std::string radius = tail.substr(open + 1, close - open - 1);
        node.title = extract_title(source_line);
        node.color = extract_color(source_line);
        extract_trace_animation(node, source_line);
        node.first = "(" + radius + ") * cos(t)"; node.second = "(" + radius + ") * sin(t)";
        const std::string domain = tail.substr(close + 1);
        if (std::regex_search(domain, match, std::regex(R"(\s+over\s+t\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\](?:\s+y\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\])?)"))) {
            node.xmin = domain_value(match[1]); node.xmax = domain_value(match[2]);
            if (match[3].matched) { node.ymin = domain_value(match[3]); node.ymax = domain_value(match[4]); }
        }
        nodes.push_back(node);
    }

    std::regex vectorfield_line(R"(plot2d\s+vectorfield\s*(.*))");
    std::stringstream vectorfield_lines(source); 
    while (std::getline(vectorfield_lines, source_line)) {
        if (!std::regex_match(source_line, match, vectorfield_line)) continue;
        const std::string tail = match[1]; const size_t open = tail.find('('); if (open == std::string::npos) continue;
        int depth = 0; size_t comma = std::string::npos, close = std::string::npos;
        for (size_t index = open; index < tail.size(); ++index) {
            if (tail[index] == '(') ++depth;
            else if (tail[index] == ')' && --depth == 0) { close = index; break; }
            else if (tail[index] == ',' && depth == 1) comma = index;
        }
        if (comma == std::string::npos || close == std::string::npos) continue;
        auto trim = [](std::string value) { const size_t first = value.find_first_not_of(" \t"); const size_t last = value.find_last_not_of(" \t"); return first == std::string::npos ? std::string() : value.substr(first, last - first + 1); };
        SceneNode node{SceneNode::Kind::VectorField2D}; node.first = trim(tail.substr(open + 1, comma - open - 1)); node.second = trim(tail.substr(comma + 1, close - comma - 1));
        node.title = extract_title(source_line); node.color = extract_color(source_line);
        const std::string domain = tail.substr(close + 1);
        if (std::regex_search(domain, match, std::regex(R"(\s+over\s+x\s*,\s*y\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\])"))) { node.xmin = node.ymin = domain_value(match[1]); node.xmax = node.ymax = domain_value(match[2]); }
        nodes.push_back(node);
    }

    std::regex data_line(R"(plot2d\s+data\s+([^\r\n&]+))");
    const std::regex data_point(R"(\(\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*,\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*\))");
    for (std::sregex_iterator it(source.begin(), source.end(), data_line), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Data2D};
        const std::string tail = (*it)[1];
        for (std::sregex_iterator point(tail.begin(), tail.end(), data_point), point_end; point != point_end; ++point)
            node.samples.emplace_back(std::stod((*point)[1]), std::stod((*point)[2]));
        const size_t line_start = source.rfind('\n', it->position()) == std::string::npos ? 0 : source.rfind('\n', it->position()) + 1;
        const size_t line_end = source.find('\n', it->position());
        const std::string line = source.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
        node.title = extract_title(line); node.color = extract_color(line); extract_trace_animation(node, line);
        if (!node.samples.empty()) nodes.push_back(node);
    }

    std::regex implicit_line(R"(plot2d\s+implicit\s+([^\r\n&]+))");
    for (std::sregex_iterator it(source.begin(), source.end(), implicit_line), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Implicit2D}; node.expression = (*it)[1];
        if (std::regex_search(node.expression, match, std::regex(R"(\s+over\s+x\s*,\s*y\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\])"))) {
            node.xmin = node.ymin = domain_value(match[1]); node.xmax = node.ymax = domain_value(match[2]);
            node.expression = node.expression.substr(0, static_cast<size_t>(match.position()));
        }
        node.expression.erase(node.expression.find_last_not_of(" \t\r\n") + 1);
        const size_t title_start = source.rfind('\n', it->position()) == std::string::npos ? 0 : source.rfind('\n', it->position()) + 1;
        const size_t title_end = source.find('\n', it->position()); node.title = extract_title(source.substr(title_start, title_end == std::string::npos ? std::string::npos : title_end - title_start));
        node.color = extract_color(source.substr(title_start, title_end == std::string::npos ? std::string::npos : title_end - title_start));
        nodes.push_back(node);
    }

    std::regex plot2d(R"((?:plot2d|plot\s+)([^\r\n&]+))");
    for (std::sregex_iterator it(source.begin(), source.end(), plot2d), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Plot2D}; node.expression = (*it)[1];
        const size_t line_start = source.rfind('\n', it->position()) == std::string::npos ? 0 : source.rfind('\n', it->position()) + 1;
        const size_t line_end = source.find('\n', it->position());
        extract_trace_animation(node, source.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start));
        if (std::regex_search(node.expression, match, std::regex(R"(\s+over\s+x\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\](?:\s+y\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\])?)"))) {
            node.xmin = domain_value(match[1]); node.xmax = domain_value(match[2]);
            if (match[3].matched) { node.ymin = domain_value(match[3]); node.ymax = domain_value(match[4]); }
            node.expression = node.expression.substr(0, static_cast<size_t>(match.position()));
        }
        node.expression.erase(node.expression.find_last_not_of(" \t\r\n") + 1);
        const size_t title_start = source.rfind('\n', it->position()) == std::string::npos ? 0 : source.rfind('\n', it->position()) + 1;
        const size_t title_end = source.find('\n', it->position()); node.title = extract_title(source.substr(title_start, title_end == std::string::npos ? std::string::npos : title_end - title_start));
        node.color = extract_color(source.substr(title_start, title_end == std::string::npos ? std::string::npos : title_end - title_start));
        const size_t first = node.expression.find_first_not_of(" \t");
        const std::string trimmed = first == std::string::npos ? std::string() : node.expression.substr(first);
        if (trimmed.rfind("parametric", 0) != 0 && trimmed.rfind("vectorfield", 0) != 0 && trimmed.rfind("implicit", 0) != 0 && trimmed.rfind("polar", 0) != 0 && trimmed.rfind("data", 0) != 0) nodes.push_back(node);
    }

    std::regex plot3d(R"(plot3d\s+([^\r\n&]+))");
    for (std::sregex_iterator it(source.begin(), source.end(), plot3d), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Plot3D}; node.expression = (*it)[1];
        const size_t line_start = source.rfind('\n', it->position()) == std::string::npos ? 0 : source.rfind('\n', it->position()) + 1;
        const size_t line_end = source.find('\n', it->position());
        const std::string line = source.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
        node.title = extract_title(line);
        node.color = extract_color(line);
        extract_trace_animation(node, line);
        if (std::regex_search(node.expression, match, std::regex(R"(\s+over\s+x\s*,\s*y\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\])"))) {
            node.xmin = node.ymin = domain_value(match[1]); node.xmax = node.ymax = domain_value(match[2]);
            node.expression = node.expression.substr(0, static_cast<size_t>(match.position()));
        }
        node.expression.erase(node.expression.find_last_not_of(" \t\r\n") + 1);
        if (node.expression.rfind("data", 0) != 0) nodes.push_back(node);
    }

    std::regex data3d_line(R"(plot3d\s+data\s+([^\r\n&]+))");
    const std::regex data3d_row(R"(\[([0-9eE+.,\s-]+)\])");
    const std::regex data3d_number(R"(-?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
    for (std::sregex_iterator it(source.begin(), source.end(), data3d_line), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Data3D}; const std::string tail = (*it)[1];
        for (std::sregex_iterator row(tail.begin(), tail.end(), data3d_row), row_end; row != row_end; ++row) {
            std::vector<double> values; const std::string row_text = (*row)[1];
            for (std::sregex_iterator value(row_text.begin(), row_text.end(), data3d_number), value_end; value != value_end; ++value) values.push_back(std::stod(value->str()));
            if (!values.empty()) node.grid.push_back(std::move(values));
        }
        const size_t line_start = source.rfind('\n', it->position()) == std::string::npos ? 0 : source.rfind('\n', it->position()) + 1;
        const size_t line_end = source.find('\n', it->position());
        const std::string line = source.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
        node.title = extract_title(line); node.color = extract_color(line); extract_trace_animation(node, line);
        if (!node.grid.empty()) nodes.push_back(node);
    }

    // A 4D parametric curve is specified by four coordinate functions of t.
    // Its 3D projection is interactive; the fourth coordinate is retained and
    // participates in a rotatable 4D projection in the scene renderer.
    std::regex parametric4d_line(R"(plot4d\s+parametric\s*(.*))");
    std::stringstream four_lines(source);
    while (std::getline(four_lines, source_line)) {
        if (!std::regex_match(source_line, match, parametric4d_line)) continue;
        const std::string tail = match[1];
        const size_t open = tail.find('(');
        if (open == std::string::npos) continue;
        std::vector<size_t> commas;
        size_t close = std::string::npos; int depth = 0;
        for (size_t i = open; i < tail.size(); ++i) {
            if (tail[i] == '(') ++depth;
            else if (tail[i] == ')' && --depth == 0) { close = i; break; }
            else if (tail[i] == ',' && depth == 1) commas.push_back(i);
        }
        if (commas.size() != 3 || close == std::string::npos) continue;
        auto trim4 = [](std::string value) {
            const size_t first = value.find_first_not_of(" \t");
            const size_t last = value.find_last_not_of(" \t");
            return first == std::string::npos ? std::string() : value.substr(first, last - first + 1);
        };
        SceneNode node{SceneNode::Kind::Parametric4D};
        const size_t starts[] = {open + 1, commas[0] + 1, commas[1] + 1, commas[2] + 1};
        const size_t ends[] = {commas[0], commas[1], commas[2], close};
        node.first = trim4(tail.substr(starts[0], ends[0] - starts[0]));
        node.second = trim4(tail.substr(starts[1], ends[1] - starts[1]));
        node.third = trim4(tail.substr(starts[2], ends[2] - starts[2]));
        node.fourth = trim4(tail.substr(starts[3], ends[3] - starts[3]));
        node.title = extract_title(source_line); node.color = extract_color(source_line);
        extract_trace_animation(node, source_line);
        const std::string domain = tail.substr(close + 1);
        if (std::regex_search(domain, match, std::regex(R"(\s+over\s+t\s+in\s*\[\s*([A-Za-z0-9_.*+()/ \t-]+)\s*,\s*([A-Za-z0-9_.*+()/ \t-]+)\s*\])"))) {
            node.xmin = domain_value(match[1]); node.xmax = domain_value(match[2]);
        }
        nodes.push_back(node);
    }

    // Legacy `plot4d` is still accepted as a tesseract demonstration.
    if (source.find("plot4d") != std::string::npos &&
        std::none_of(nodes.begin(), nodes.end(), [](const SceneNode& node) { return node.kind == SceneNode::Kind::Parametric4D; }))
        nodes.push_back(SceneNode{SceneNode::Kind::Plot4D});

    std::regex sphere(R"(sphere\s+at\s*\(\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*,\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*,\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*\)\s+radius\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?))");
    for (std::sregex_iterator it(source.begin(), source.end(), sphere), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Sphere}; node.x = std::stod((*it)[1]); node.y = std::stod((*it)[2]); node.z = std::stod((*it)[3]); node.radius = std::stod((*it)[4]); nodes.push_back(node);
    }

    std::regex cube(R"(cube\s+at\s*\(\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*,\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*,\s*([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*\)\s+size\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?))");
    for (std::sregex_iterator it(source.begin(), source.end(), cube), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Cube}; node.x = std::stod((*it)[1]); node.y = std::stod((*it)[2]); node.z = std::stod((*it)[3]); node.size = std::stod((*it)[4]); nodes.push_back(node);
    }

    std::regex morph(R"((square|circle)\s*\(\s*size\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*\)\s+morphs\s+to\s+(square|circle)\s*\(\s*size\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)\s*\)(?:\s+over\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s)?(?:\s+ease\s+(linear|in-out|in|out))?(?:\s+at\s+([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)s)?(?:\s+repeat\s+([1-9][0-9]*)(?:\s+(yoyo))?)?)");
    for (std::sregex_iterator it(source.begin(), source.end(), morph), end; it != end; ++it) {
        SceneNode node{SceneNode::Kind::Morph}; node.first = (*it)[1]; node.x = std::stod((*it)[2]); node.second = (*it)[3]; node.y = std::stod((*it)[4]); if ((*it)[5].matched) node.duration = std::stod((*it)[5]); if ((*it)[6].matched) node.easing = (*it)[6]; if ((*it)[7].matched) node.start_time = std::stod((*it)[7]); if ((*it)[8].matched) node.repeat = std::stoi((*it)[8]); node.yoyo = (*it)[9].matched; nodes.push_back(node);
    }
    return nodes;
}

}  // namespace marila
