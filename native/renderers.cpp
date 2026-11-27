#include "renderers.hpp"
#include "scene_model.hpp"

#include <fstream>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <array>
#include <vector>

namespace marila {
namespace {

std::string html_escape(std::string value) {
    size_t position = 0;
    while ((position = value.find('&', position)) != std::string::npos) { value.replace(position, 1, "&amp;"); position += 5; }
    position = 0;
    while ((position = value.find('<', position)) != std::string::npos) { value.replace(position, 1, "&lt;"); position += 4; }
    position = 0;
    while ((position = value.find('>', position)) != std::string::npos) { value.replace(position, 1, "&gt;"); position += 4; }
    position = 0;
    while ((position = value.find('"', position)) != std::string::npos) { value.replace(position, 1, "&quot;"); position += 6; }
    return value;
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("unable to write output '" + path + "'");
    output << content;
}

std::string render_image(const std::string& source);

std::string svg_color(const std::string& name, const std::string& fallback) {
    if (name == "amber") return "#f59e0b";
    if (name == "blue") return "#3b82f6";
    if (name == "green") return "#22c55e";
    if (name == "red") return "#ef4444";
    if (name == "purple") return "#a855f7";
    if (name == "cyan") return "#06b6d4";
    return name.empty() ? fallback : name;
}

std::vector<std::vector<std::string>> document_table_rows(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    std::string expanded = text;
    size_t escape = 0;
    while ((escape = expanded.find("\\n", escape)) != std::string::npos) { expanded.replace(escape, 2, "\n"); ++escape; }
    std::stringstream lines(expanded); std::string line;
    while (std::getline(lines, line)) {
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line.substr(first).find_first_not_of('|') == std::string::npos) continue;
        std::vector<std::string> cells; size_t start = line.find('|');
        while (start != std::string::npos) {
            const size_t end = line.find('|', start + 1); if (end == std::string::npos) break;
            std::string cell = line.substr(start + 1, end - start - 1);
            const size_t left = cell.find_first_not_of(" \t"), right = cell.find_last_not_of(" \t");
            cells.push_back(left == std::string::npos ? std::string() : cell.substr(left, right - left + 1)); start = end;
        }
        if (!cells.empty() && std::all_of(cells.front().begin(), cells.front().end(), [](char value) { return value == '-' || value == ':' || std::isspace(static_cast<unsigned char>(value)); })) continue;
        if (!cells.empty()) rows.push_back(std::move(cells));
    }
    return rows;
}

std::string render_document(const std::string& source) {
    std::stringstream body;
    std::regex item(R"MARILA((\w+)\s*-\s*"((?:\\.|[^"\\])*)"(?:\s*::\s*(?:"((?:\\.|[^"\\])*)"|([^\r\n\]]+)))?)MARILA");
    auto decode = [](std::string value) {
        std::string result;
        for (size_t index = 0; index < value.size(); ++index) {
            if (value[index] == '\\' && index + 1 < value.size()) {
                const char escaped = value[++index];
                if (escaped == 'n') result += '\n';
                else if (escaped == 'r') result += '\r';
                else if (escaped == 't') result += '\t';
                else result += escaped;
            } else result += value[index];
        }
        return result;
    };
    std::string active_list;
    for (std::sregex_iterator it(source.begin(), source.end(), item), end; it != end; ++it) {
        const std::string id = (*it)[1], text = html_escape(decode((*it)[2]));
        const std::string subtitle = html_escape(decode((*it)[3].matched ? (*it)[3].str() : (*it)[4].str()));
        const std::string list_type = id.rfind("bullet", 0) == 0 || id.rfind("item", 0) == 0 ? "ul" : (id.rfind("number", 0) == 0 || id.rfind("ordered", 0) == 0 ? "ol" : "");
        if (!list_type.empty()) {
            if (active_list != list_type) { if (!active_list.empty()) body << "</" << active_list << ">\n"; active_list = list_type; body << "<" << active_list << ">"; }
            body << "<li>" << text << "</li>";
            continue;
        }
        if (!active_list.empty()) { body << "</" << active_list << ">\n"; active_list.clear(); }
        if (id.rfind("table", 0) == 0) {
            body << "<table class=\"document-table\"><tbody>";
            const auto rows = document_table_rows((*it)[2]);
            for (size_t row = 0; row < rows.size(); ++row) { body << "<tr>"; for (const auto& cell : rows[row]) body << (row == 0 ? "<th>" : "<td>") << html_escape(cell) << (row == 0 ? "</th>" : "</td>"); body << "</tr>\n"; }
            body << "</tbody></table>\n";
        } else if (id.rfind("bullet", 0) == 0 || id.rfind("item", 0) == 0) {
            body << "<ul><li>" << text << "</li></ul>\n";
        } else if (id.rfind("number", 0) == 0 || id.rfind("ordered", 0) == 0) {
            body << "<ol><li>" << text << "</li></ol>\n";
        } else if (id.rfind("code", 0) == 0) {
            body << "<pre><code>" << text << "</code></pre>\n";
        } else if (id.rfind("theorem", 0) == 0 || id.rfind("definition", 0) == 0 ||
            id.rfind("proof", 0) == 0 || id.rfind("prop", 0) == 0) {
            body << "<section class=\"callout\"><h3>" << html_escape(id) << "</h3><p>" << text << "</p>";
            if (!subtitle.empty()) body << "<p>" << subtitle << "</p>";
            body << "</section>\n";
        } else if (id.rfind("equation", 0) == 0 || id.rfind("math", 0) == 0) {
            body << "<div class=\"equation\" id=\"" << html_escape(id) << "\"><code>" << text << "</code></div>\n";
        } else if (id.rfind("section", 0) == 0) {
            body << "<h2 id=\"" << html_escape(id) << "\">" << text << "</h2>\n";
        } else if (id.rfind("subsection", 0) == 0) {
            body << "<h3 id=\"" << html_escape(id) << "\">" << text << "</h3>\n";
        } else if (id.rfind("quote", 0) == 0) {
            body << "<blockquote>" << text << (subtitle.empty() ? "" : "<cite>" + subtitle + "</cite>") << "</blockquote>\n";
        } else if (id.rfind("link", 0) == 0 && !subtitle.empty()) {
            body << "<p><a href=\"" << subtitle << "\">" << text << "</a></p>\n";
        } else if (!subtitle.empty()) {
            body << "<header><h1>" << text << "</h1><p>" << subtitle << "</p></header>\n";
        } else {
            body << "<p>" << text << "</p>\n";
        }
    }
    if (!active_list.empty()) body << "</" << active_list << ">\n";
    const auto visual_nodes = parse_visual_source(source);
    if (!visual_nodes.empty()) body << "<figure><figcaption>Marila visual</figcaption>" << render_image(source) << "</figure>\n";
    return "<!doctype html><meta charset=\"utf-8\"><title>Marila document</title>"
           "<style>body{max-width:800px;margin:40px auto;font:18px Georgia;line-height:1.6}"
           ".callout{border-left:4px solid #6366f1;background:#f1f5f9;padding:12px;margin:20px 0}.equation{font:22px Georgia;text-align:center;padding:18px;margin:18px 0;background:#e2e8f0;color:#0f172a}.document-table{border-collapse:collapse;margin:20px 0}.document-table th,.document-table td{border:1px solid #94a3b8;padding:6px 10px}.document-table th{background:#e2e8f0}"
           "header{border-bottom:2px solid #334155}</style><main>" + body.str() + "</main>";
}

std::string render_markdown_impl(const std::string& source) {
    std::ostringstream output;
    std::regex item(R"MARILA((\w+)\s*-\s*"((?:\\.|[^"\\])*)"(?:\s*::\s*(?:"((?:\\.|[^"\\])*)"|([^\r\n\]]+)))?)MARILA");
    auto decode = [](std::string value) {
        std::string result;
        for (size_t index = 0; index < value.size(); ++index) {
            if (value[index] == '\\' && index + 1 < value.size()) { const char escaped = value[++index]; result += escaped == 'n' ? '\n' : escaped == 'r' ? '\r' : escaped == 't' ? '\t' : escaped; }
            else result += value[index];
        }
        return result;
    };
    auto markdown_text = [](std::string value) { size_t position = 0; while ((position = value.find('`', position)) != std::string::npos) { value.insert(position, "\\"); position += 2; } return value; };
    std::string active_list;
    for (std::sregex_iterator it(source.begin(), source.end(), item), end; it != end; ++it) {
        const std::string id = (*it)[1], text = markdown_text(decode((*it)[2]));
        const std::string subtitle = markdown_text(decode((*it)[3].matched ? (*it)[3].str() : (*it)[4].str()));
        const std::string list_type = id.rfind("bullet", 0) == 0 || id.rfind("item", 0) == 0 ? "ul" : (id.rfind("number", 0) == 0 || id.rfind("ordered", 0) == 0 ? "ol" : "");
        if (!list_type.empty()) { output << (list_type == "ul" ? "- " : "1. ") << text << '\n'; active_list = list_type; continue; }
        if (!active_list.empty()) { output << '\n'; active_list.clear(); }
        if (id.rfind("table", 0) == 0) {
            const auto rows = document_table_rows(decode((*it)[2]));
            for (size_t row = 0; row < rows.size(); ++row) { output << '|'; for (const auto& cell : rows[row]) output << ' ' << cell << " |"; output << '\n'; if (row == 0) { output << '|'; for (size_t column = 0; column < rows[row].size(); ++column) output << " --- |"; output << '\n'; } }
        } else if (id.rfind("code", 0) == 0) output << "```\n" << decode((*it)[2]) << "\n```\n";
        else if (id.rfind("section", 0) == 0) output << "## " << text << "\n";
        else if (id.rfind("subsection", 0) == 0) output << "### " << text << "\n";
        else if (id.rfind("equation", 0) == 0 || id.rfind("math", 0) == 0) output << "$$\n" << text << "\n$$\n";
        else if (id.rfind("quote", 0) == 0) output << "> " << text << (subtitle.empty() ? "" : "\n> — " + subtitle) << "\n";
        else if (id.rfind("link", 0) == 0 && !subtitle.empty()) output << '[' << text << "](" << subtitle << ")\n";
        else if (id.rfind("theorem", 0) == 0 || id.rfind("definition", 0) == 0 || id.rfind("proof", 0) == 0 || id.rfind("prop", 0) == 0) output << "**" << id << "**: " << text << (subtitle.empty() ? "" : " — " + subtitle) << "\n";
        else if (!subtitle.empty()) output << "# " << text << "\n\n" << subtitle << "\n";
        else output << text << "\n";
    }
    return output.str();
}

std::string latex_escape(std::string value) {
    std::string result;
    for (char character : value) {
        switch (character) {
        case '&': case '%': case '$': case '#': case '_': case '{': case '}':
            result += '\\'; result += character; break;
        case '\\': result += "\\textbackslash{}"; break;
        default: result += character;
        }
    }
    return result;
}

std::string latex_inline(std::string value) {
    std::string result; size_t start = 0;
    while (true) {
        size_t open = value.find('<', start);
        if (open == std::string::npos) { result += latex_escape(value.substr(start)); break; }
        result += latex_escape(value.substr(start, open - start));
        size_t close = value.find('>', open + 1);
        if (close == std::string::npos) { result += latex_escape(value.substr(open)); break; }
        std::string math = value.substr(open + 1, close - open - 1);
        std::smatch integral;
        if (std::regex_match(math, integral, std::regex(R"(integral\s*\|([^|]*)\|\|([^|]*)\|\|\s*(.*))")))
            result += "\\(\\int_{" + integral[1].str() + "}^{" + integral[2].str() + "} " + integral[3].str() + "\\)";
        else result += "\\(" + math + "\\)";
        start = close + 1;
    }
    return result;
}

std::string render_latex(const std::string& source) {
    std::stringstream body;
    bool has_link = false;
    std::regex item(R"MARILA((\w+)\s*-\s*"((?:\\.|[^"\\])*)"(?:\s*::\s*(?:"((?:\\.|[^"\\])*)"|([^\r\n\]]+)))?)MARILA");
    auto decode = [](std::string value) {
        std::string result;
        for (size_t index = 0; index < value.size(); ++index) {
            if (value[index] == '\\' && index + 1 < value.size()) {
                const char escaped = value[++index];
                if (escaped == 'n') result += '\n';
                else if (escaped == 'r') result += '\r';
                else if (escaped == 't') result += '\t';
                else result += escaped;
            } else result += value[index];
        }
        return result;
    };
    std::string active_list;
    for (std::sregex_iterator it(source.begin(), source.end(), item), end; it != end; ++it) {
        const std::string id = (*it)[1], text = decode((*it)[2]), subtitle = decode((*it)[3].matched ? (*it)[3].str() : (*it)[4].str());
        const std::string list_type = id.rfind("bullet", 0) == 0 || id.rfind("item", 0) == 0 ? "itemize" : (id.rfind("number", 0) == 0 || id.rfind("ordered", 0) == 0 ? "enumerate" : "");
        if (!list_type.empty()) {
            if (active_list != list_type) { if (!active_list.empty()) body << "\\end{" << active_list << "}\n"; active_list = list_type; body << "\\begin{" << active_list << "}\n"; }
            body << "\\item " << latex_inline(text) << "\n";
            continue;
        }
        if (!active_list.empty()) { body << "\\end{" << active_list << "}\n"; active_list.clear(); }
        if (id.rfind("table", 0) == 0) {
            const auto rows = document_table_rows(text);
            if (!rows.empty()) {
                body << "\\begin{center}\\begin{tabular}{" << std::string(rows.front().size(), 'l') << "}\\hline\n";
                for (size_t row = 0; row < rows.size(); ++row) {
                    for (size_t column = 0; column < rows[row].size(); ++column) { if (column) body << " & "; body << latex_escape(rows[row][column]); }
                    body << " \\\\ \\hline\n";
                }
                body << "\\end{tabular}\\end{center}\n";
            }
        } else if (id.rfind("bullet", 0) == 0 || id.rfind("item", 0) == 0) {
            body << "\\begin{itemize}\\item " << latex_inline(text) << "\\end{itemize}\n";
        } else if (id.rfind("number", 0) == 0 || id.rfind("ordered", 0) == 0) {
            body << "\\begin{enumerate}\\item " << latex_inline(text) << "\\end{enumerate}\n";
        } else if (id.rfind("code", 0) == 0) {
            body << "\\begin{verbatim}\n" << text << "\n\\end{verbatim}\n";
        } else if (id.rfind("theorem", 0) == 0 || id.rfind("definition", 0) == 0 ||
            id.rfind("proof", 0) == 0 || id.rfind("prop", 0) == 0) {
            body << "\\begin{quote}\\textbf{" << latex_escape(id) << "}. "
                 << latex_inline(text); if (!subtitle.empty()) body << "\\par " << latex_inline(subtitle); body << "\\end{quote}\n";
        } else if (id.rfind("equation", 0) == 0 || id.rfind("math", 0) == 0) {
            body << "\\[" << text << "\\]\n";
        } else if (id.rfind("section", 0) == 0) {
            body << "\\section{" << latex_escape(text) << "}\n";
        } else if (id.rfind("subsection", 0) == 0) {
            body << "\\subsection{" << latex_escape(text) << "}\n";
        } else if (id.rfind("quote", 0) == 0) {
            body << "\\begin{quote}" << latex_inline(text); if (!subtitle.empty()) body << "\\par\\emph{" << latex_escape(subtitle) << "}"; body << "\\end{quote}\n";
        } else if (id.rfind("link", 0) == 0 && !subtitle.empty()) {
            has_link = true; body << "\\href{" << latex_escape(subtitle) << "}{" << latex_inline(text) << "}\n\n";
        } else if (!subtitle.empty()) {
            body << "\\section{" << latex_escape(text) << "}\n"
                 << latex_inline(subtitle) << "\n";
        } else {
            body << latex_inline(text) << "\n\n";
        }
    }
    if (!active_list.empty()) body << "\\end{" << active_list << "}\n";
    std::string visual;
    const auto nodes = parse_visual_source(source);
    const bool has_3d = std::any_of(nodes.begin(), nodes.end(), [](const SceneNode& node) {
        return node.kind == SceneNode::Kind::Plot3D || node.kind == SceneNode::Kind::Data3D || node.kind == SceneNode::Kind::Sphere || node.kind == SceneNode::Kind::Cube;
    });
    if (!nodes.empty()) {
        std::map<std::string, std::pair<double, double>> points;
        visual = "\\begin{figure}[h]\n\\centering\n\\begin{tikzpicture}\n";
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Point) {
            points[node.id] = {node.x, node.y};
            visual += "\\coordinate (" + node.id + ") at (" + std::to_string(node.x) + "," + std::to_string(node.y) + ");\n";
            if (!node.label.empty()) visual += "\\node[below] at (" + node.id + ") {" + latex_escape(node.label) + "};\n";
        }
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Line)
            visual += "\\draw (" + node.first + ") -- (" + node.second + ");\n";
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Triangle)
            visual += "\\draw (" + node.first + ") -- (" + node.second + ") -- (" + node.third + ") -- cycle;\n";
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Circle) {
            const auto center = points.find(node.first);
            if (center != points.end()) visual += "\\draw" + std::string(node.dashed ? "[dashed]" : "") + " (" + std::to_string(center->second.first) + "," + std::to_string(center->second.second) + ") circle (" + std::to_string(node.radius) + ");\n";
        }
        auto latex_plot_expression = [](std::string expression, const std::string& variable) {
            expression = std::regex_replace(expression, std::regex(variable == "x" ? R"(\bx\b)" : R"(\bt\b)"), "\\" + variable);
            expression = std::regex_replace(expression, std::regex(R"(sin\(([^()]*)\))"), "sin($1 r)");
            expression = std::regex_replace(expression, std::regex(R"(cos\(([^()]*)\))"), "cos($1 r)");
            expression = std::regex_replace(expression, std::regex(R"(tan\(([^()]*)\))"), "tan($1 r)");
            return expression;
        };
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Plot2D) {
            visual += "\\draw[domain=" + std::to_string(node.xmin) + ":" + std::to_string(node.xmax) + ",samples=200,variable=\\x" + (node.color.empty() ? "" : ",color=" + node.color) + "] plot (\\x,{" + latex_plot_expression(node.expression, "x") + "});\n";
            if (!node.title.empty()) visual += "\\node[above] at (0,0) {" + latex_escape(node.title) + "};\n";
        } else if (node.kind == SceneNode::Kind::Data2D) {
            visual += "\\draw[mark=*,color=" + (node.color.empty() ? std::string("orange") : node.color) + "] plot coordinates {";
            for (const auto& sample : node.samples) visual += "(" + std::to_string(sample.first) + "," + std::to_string(sample.second) + ")";
            visual += "};\n";
        } else if (node.kind == SceneNode::Kind::VectorField2D) {
            auto vector_expression = [](std::string expression, const std::string& variable, const std::string& replacement) {
                return std::regex_replace(expression, std::regex("\\b" + variable + "\\b"), replacement);
            };
            visual += "\\foreach \\x in {";
            for (int index = 0; index <= 6; ++index) { if (index) visual += ","; visual += std::to_string(node.xmin + (node.xmax - node.xmin) * index / 6.0); }
            visual += "} \\foreach \\y in {";
            for (int index = 0; index <= 6; ++index) { if (index) visual += ","; visual += std::to_string(node.ymin + (node.ymax - node.ymin) * index / 6.0); }
            visual += "} \\draw[->] (\\x,\\y) -- ++({" + vector_expression(node.first, "x", "\\x") + "},{" + vector_expression(node.second, "y", "\\y") + "});\n";
        } else if (node.kind == SceneNode::Kind::Parametric2D) {
            visual += "\\draw[domain=" + std::to_string(node.xmin) + ":" + std::to_string(node.xmax) + ",samples=200,variable=\\t" + (node.color.empty() ? "" : ",color=" + node.color) + "] plot ({" + latex_plot_expression(node.first, "t") + "},{" + latex_plot_expression(node.second, "t") + "});\n";
            if (!node.title.empty()) visual += "\\node[above] at (0,0) {" + latex_escape(node.title) + "};\n";
        }
        if (has_3d) {
            visual += "\\begin{axis}[view={45}{30},axis lines=none,grid=major]\n";
            for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Plot3D) {
                std::string expression = node.expression;
                expression = std::regex_replace(expression, std::regex(R"(sin\(([^()]*)\))"), "sin(deg($1))");
                expression = std::regex_replace(expression, std::regex(R"(cos\(([^()]*)\))"), "cos(deg($1))");
                expression = std::regex_replace(expression, std::regex(R"(tan\(([^()]*)\))"), "tan(deg($1))");
                visual += "\\addplot3[surf,mesh,domain=" + std::to_string(node.xmin) + ":" + std::to_string(node.xmax) +
                    ",y domain=" + std::to_string(node.ymin) + ":" + std::to_string(node.ymax) + ",samples=25,samples y=25";
                if (!node.color.empty()) visual += ",color=" + node.color;
                if (!node.title.empty()) visual += ",title={" + latex_escape(node.title) + "}";
                visual += "] ({x},{y},{" + expression + "});\n";
            }
            for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Data3D) {
                visual += "\\addplot3[surf,mesh";
                if (!node.color.empty()) visual += ",color=" + node.color;
                if (!node.title.empty()) visual += ",title={" + latex_escape(node.title) + "}";
                visual += "] coordinates {";
                for (size_t row = 0; row < node.grid.size(); ++row) for (size_t column = 0; column < node.grid[row].size(); ++column)
                    visual += "(" + std::to_string(column) + "," + std::to_string(row) + "," + std::to_string(node.grid[row][column]) + ")";
                visual += "};\n";
            }
            for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Sphere || node.kind == SceneNode::Kind::Cube)
                visual += "\\addplot3[only marks,mark=*] coordinates {( " + std::to_string(node.x) + "," + std::to_string(node.y) + "," + std::to_string(node.z) + ")};\n";
            visual += "\\end{axis}\n";
        }
        visual += "\\end{tikzpicture}\n\\end{figure}\n";
    }
    return std::string("\\documentclass{article}\n\\usepackage{amsmath,amssymb}") + (has_link ? "\n\\usepackage{hyperref}" : "") + (visual.empty() ? "" : "\n\\usepackage{tikz}") +
           (has_3d ? "\n\\usepackage{pgfplots}\n\\pgfplotsset{compat=1.18}" : "") + "\n\\begin{document}\n" +
           body.str() + visual + "\\end{document}\n";
}

std::string render_tikz(const std::string& source) {
    std::stringstream body;
    const auto nodes = parse_visual_source(source);
    const bool has_3d = std::any_of(nodes.begin(), nodes.end(), [](const SceneNode& node) {
        return node.kind == SceneNode::Kind::Plot3D || node.kind == SceneNode::Kind::Data3D || node.kind == SceneNode::Kind::Sphere || node.kind == SceneNode::Kind::Cube;
    });
    std::map<std::string, std::pair<double, double>> points;
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Point) {
        points[node.id] = {node.x, node.y};
        body << "\\coordinate (" << node.id << ") at (" << node.x << "," << node.y << ");\n";
        if (!node.label.empty()) body << "\\node[below] at (" << node.id << ") {" << latex_escape(node.label) << "};\n";
    }
    auto point = [&](const std::string& id) { auto found = points.find(id); return found == points.end() ? std::pair<double, double>{0, 0} : found->second; };
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Line) body << "\\draw (" << node.first << ") -- (" << node.second << ");\n";
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Triangle) {
        body << "\\draw";
        if (!node.color.empty()) body << "[draw=" << node.color << "]";
        body << " (" << node.first << ") -- (" << node.second << ") -- (" << node.third << ") -- cycle;\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Circle) {
        const auto center = point(node.first); body << "\\draw" << (node.dashed ? "[dashed]" : "") << " (" << center.first << "," << center.second << ") circle (" << node.radius << ");\n";
    }
    auto tikz_expression = [](std::string expression, const std::string& variable) {
        expression = std::regex_replace(expression, std::regex(variable == "x" ? R"(\bx\b)" : R"(\bt\b)"), "\\" + variable);
        expression = std::regex_replace(expression, std::regex(R"(sin\(([^()]*)\))"), "sin($1 r)");
        expression = std::regex_replace(expression, std::regex(R"(cos\(([^()]*)\))"), "cos($1 r)");
        expression = std::regex_replace(expression, std::regex(R"(tan\(([^()]*)\))"), "tan($1 r)");
        return expression;
    };
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Plot2D) {
        body << "\\draw[domain=" << node.xmin << ":" << node.xmax << ",samples=200,variable=\\x" << (node.color.empty() ? "" : ",color=" + node.color) << "] plot (\\x,{" << tikz_expression(node.expression, "x") << "});\n";
        if (!node.title.empty()) body << "\\node[above] at (0,0) {" << latex_escape(node.title) << "};\n";
    } else if (node.kind == SceneNode::Kind::Data2D) {
        body << "\\draw[mark=*,color=" << (node.color.empty() ? "orange" : node.color) << "] plot coordinates {";
        for (const auto& sample : node.samples) body << "(" << sample.first << "," << sample.second << ")";
        body << "};\n";
    } else if (node.kind == SceneNode::Kind::VectorField2D) {
        auto vector_expression = [](std::string expression, const std::string& variable, const std::string& replacement) {
            return std::regex_replace(expression, std::regex("\\b" + variable + "\\b"), replacement);
        };
        body << "\\foreach \\x in {";
        for (int index = 0; index <= 6; ++index) { if (index) body << ","; body << node.xmin + (node.xmax - node.xmin) * index / 6.0; }
        body << "} \\foreach \\y in {";
        for (int index = 0; index <= 6; ++index) { if (index) body << ","; body << node.ymin + (node.ymax - node.ymin) * index / 6.0; }
        body << "} \\draw[->] (\\x,\\y) -- ++({" << vector_expression(node.first, "x", "\\x") << "},{" << vector_expression(node.second, "y", "\\y") << "});\n";
    } else if (node.kind == SceneNode::Kind::Parametric2D) {
        body << "\\draw[domain=" << node.xmin << ":" << node.xmax << ",samples=200,variable=\\t" << (node.color.empty() ? "" : ",color=" + node.color) << "] plot ({" << tikz_expression(node.first, "t") << "},{" << tikz_expression(node.second, "t") << "});\n";
        if (!node.title.empty()) body << "\\node[above] at (0,0) {" << latex_escape(node.title) << "};\n";
    }
    if (has_3d) {
        body << "\\begin{axis}[view={45}{30},axis lines=none,grid=major]\n";
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Plot3D) {
            std::string expression = node.expression;
            expression = std::regex_replace(expression, std::regex(R"(sin\(([^()]*)\))"), "sin(deg($1))");
            expression = std::regex_replace(expression, std::regex(R"(cos\(([^()]*)\))"), "cos(deg($1))");
            expression = std::regex_replace(expression, std::regex(R"(tan\(([^()]*)\))"), "tan(deg($1))");
            body << "\\addplot3[surf,mesh,domain=" << node.xmin << ":" << node.xmax
                 << ",y domain=" << node.ymin << ":" << node.ymax << ",samples=25,samples y=25";
            if (!node.color.empty()) body << ",color=" << node.color;
            if (!node.title.empty()) body << ",title={" << latex_escape(node.title) << "}";
            body << "] ({x},{y},{" << expression << "});\n";
        }
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Data3D) {
            body << "\\addplot3[surf,mesh";
            if (!node.color.empty()) body << ",color=" << node.color;
            if (!node.title.empty()) body << ",title={" << latex_escape(node.title) << "}";
            body << "] coordinates {";
            for (size_t row = 0; row < node.grid.size(); ++row) for (size_t column = 0; column < node.grid[row].size(); ++column)
                body << "(" << column << "," << row << "," << node.grid[row][column] << ")";
            body << "};\n";
        }
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Sphere || node.kind == SceneNode::Kind::Cube) {
            body << "\\addplot3[only marks,mark=*] coordinates {(" << node.x << "," << node.y << "," << node.z << ")};\n";
        }
        body << "\\end{axis}\n";
    }
    std::regex segment(R"(draw\s*\(\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*\)\s*--\s*\(\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*\))");
    for (std::sregex_iterator it(source.begin(), source.end(), segment), end; it != end; ++it) {
        body << "\\draw (" << (*it)[1] << "," << (*it)[2] << ") -- ("
             << (*it)[3] << "," << (*it)[4] << ");\n";
    }
    std::regex circle(R"(circle\s+at\s*\(\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*\)\s+radius\s+(-?[0-9.]+))");
    for (std::sregex_iterator it(source.begin(), source.end(), circle), end; it != end; ++it)
        body << "\\draw (" << (*it)[1] << "," << (*it)[2] << ") circle (" << (*it)[3] << ");\n";
    return std::string("\\documentclass{article}\n\\usepackage{tikz}\n\\pagestyle{empty}\n") +
           (has_3d ? "\\usepackage{pgfplots}\n\\pgfplotsset{compat=1.18}\n" : "") +
           "\\begin{document}\n\\begin{tikzpicture}\n" +
           body.str() + "\\end{tikzpicture}\n\\end{document}\n";
}

int spreadsheet_column(std::string name) {
    int result = 0;
    for (char character : name) result = result * 26 + (character - 'A' + 1);
    return result;
}

std::string render_sheet(const std::string& source) {
    std::map<std::string, std::string> cells;
    std::regex assignment(R"(^\s*([A-Z]+[0-9]+)\s*=\s*(.*?)\s*$)");
    std::stringstream lines(source);
    std::string line;
    while (std::getline(lines, line)) {
        std::smatch match;
        if (std::regex_match(line, match, assignment)) cells[match[1]] = match[2];
    }

    auto get_literal_cell_text = [&](const std::string& reference, std::string& result) {
        const auto found = cells.find(reference);
        if (found == cells.end()) return false;
        std::string value = found->second;
        const size_t first = value.find_first_not_of(" \t");
        value = first == std::string::npos ? std::string() : value.substr(first);
        const size_t last = value.find_last_not_of(" \t\r\n");
        if (last != std::string::npos) value.resize(last + 1);
        if (value.size() < 2 || !((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) return false;
        const char quote = value.front(); value = value.substr(1, value.size() - 2); result.clear();
        for (size_t index = 0; index < value.size(); ++index) {
            if (value[index] == quote && index + 1 < value.size() && value[index + 1] == quote) ++index;
            result += value[index];
        }
        return true;
    };

    auto range_references = [](const std::string& first, const std::string& last) {
        std::vector<std::string> references; std::smatch first_parts, last_parts;
        if (!std::regex_match(first, first_parts, std::regex(R"(([A-Z]+)([0-9]+))")) || !std::regex_match(last, last_parts, std::regex(R"(([A-Z]+)([0-9]+))"))) return references;
        const int left = spreadsheet_column(first_parts[1]), right = spreadsheet_column(last_parts[1]);
        const int top = std::stoi(first_parts[2]), bottom = std::stoi(last_parts[2]);
        for (int row = std::min(top, bottom); row <= std::max(top, bottom); ++row) for (int column = std::min(left, right); column <= std::max(left, right); ++column) {
            int value = column; std::string name;
            while (value) { name.insert(name.begin(), static_cast<char>('A' + (value - 1) % 26)); value = (value - 1) / 26; }
            references.push_back(name + std::to_string(row));
        }
        return references;
    };

    auto formula_arguments = [](const std::string& text) {
        std::vector<std::string> arguments;
        size_t start = 0;
        int depth = 0;
        char quote = '\0';
        bool escaped = false;
        for (size_t index = 0; index < text.size(); ++index) {
            const char character = text[index];
            if (quote != '\0') {
                if (escaped) escaped = false;
                else if (character == '\\') escaped = true;
                else if (character == quote) quote = '\0';
                continue;
            }
            if (character == '"' || character == '\'') { quote = character; continue; }
            if (character == '(') ++depth;
            else if (character == ')') --depth;
            else if (character == ',' && depth == 0) { arguments.push_back(text.substr(start, index - start)); start = index + 1; }
        }
        arguments.push_back(text.substr(start));
        return arguments;
    };

    std::function<double(const std::string&, std::vector<std::string>&)> evaluate;
    evaluate = [&](const std::string& input, std::vector<std::string>& stack) -> double {
        std::string expression = input;
        expression.erase(0, expression.find_first_not_of(" \t"));
        if (!expression.empty()) expression.erase(expression.find_last_not_of(" \t\r\n") + 1);
        if (!expression.empty() && expression.front() == '=') expression.erase(0, 1);
        expression.erase(std::remove(expression.begin(), expression.end(), '$'), expression.end());
        if (expression.empty()) return 0;
        while (expression.size() >= 2 && expression.front() == '(' && expression.back() == ')') {
            int depth = 0; bool wraps = true;
            for (size_t index = 0; index < expression.size(); ++index) {
                if (expression[index] == '(') ++depth;
                else if (expression[index] == ')' && --depth == 0 && index + 1 < expression.size()) { wraps = false; break; }
            }
            if (!wraps) break;
            expression = expression.substr(1, expression.size() - 2);
            expression.erase(0, expression.find_first_not_of(" \t"));
            if (!expression.empty()) expression.erase(expression.find_last_not_of(" \t\r\n") + 1);
        }
        std::smatch match;
        if (std::regex_match(expression, match, std::regex(R"(([A-Z]+[0-9]+))"))) {
            const std::string ref = match[1];
            if (std::find(stack.begin(), stack.end(), ref) != stack.end()) throw std::runtime_error("spreadsheet circular reference at " + ref);
            auto cell = cells.find(ref);
            if (cell == cells.end()) return 0;
            stack.push_back(ref); double result = evaluate(cell->second, stack); stack.pop_back(); return result;
        }
        if (std::regex_match(expression, match, std::regex(R"((INDEX|MATCH)\((.*)\))"))) {
            const auto arguments = formula_arguments(match[2].str());
            const std::string function = match[1].str();
            if (function == "INDEX") {
                if (arguments.size() < 2 || arguments.size() > 3) throw std::runtime_error("INDEX expects a range, row, and optional column");
                std::smatch range_match;
                if (!std::regex_match(arguments[0], range_match, std::regex(R"(\s*([A-Z]+[0-9]+)\s*:\s*([A-Z]+[0-9]+)\s*)"))) throw std::runtime_error("INDEX expects a cell range");
                std::smatch first_parts, last_parts;
                const std::string first_reference = range_match[1].str(), last_reference = range_match[2].str();
                std::regex_match(first_reference, first_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
                std::regex_match(last_reference, last_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
                const int first_column = spreadsheet_column(first_parts[1]), last_column = spreadsheet_column(last_parts[1]);
                const int first_row = std::stoi(first_parts[2]), last_row = std::stoi(last_parts[2]);
                const int rows = std::abs(last_row - first_row) + 1, columns = std::abs(last_column - first_column) + 1;
                const int row = static_cast<int>(std::llround(evaluate(arguments[1], stack)));
                const int column = arguments.size() == 3 ? static_cast<int>(std::llround(evaluate(arguments[2], stack))) : 1;
                if (row < 1 || row > rows || column < 1 || column > columns) throw std::runtime_error("INDEX position is outside the range");
                const auto references = range_references(range_match[1], range_match[2]);
                return evaluate(references[static_cast<size_t>((row - 1) * columns + column - 1)], stack);
            }
            if (arguments.size() < 2 || arguments.size() > 3) throw std::runtime_error("MATCH expects a value, range, and optional match type");
            std::smatch range_match;
            if (!std::regex_match(arguments[1], range_match, std::regex(R"(\s*([A-Z]+[0-9]+)\s*:\s*([A-Z]+[0-9]+)\s*)"))) throw std::runtime_error("MATCH expects a cell range");
            const int match_type = arguments.size() == 3 ? static_cast<int>(std::llround(evaluate(arguments[2], stack))) : 0;
            if (match_type != -1 && match_type != 0 && match_type != 1) throw std::runtime_error("MATCH type must be -1, 0, or 1");
            const double needle = evaluate(arguments[0], stack);
            const auto references = range_references(range_match[1], range_match[2]);
            int result = -1;
            for (size_t index = 0; index < references.size(); ++index) {
                const double candidate = evaluate(references[index], stack);
                const bool found = match_type == 0 ? std::abs(candidate - needle) <= 1e-12 : (match_type == 1 ? candidate <= needle : candidate >= needle);
                if (found) result = static_cast<int>(index) + 1;
                if (match_type != 0 && ((match_type == 1 && candidate > needle) || (match_type == -1 && candidate < needle))) break;
                if (match_type == 0 && found) break;
            }
            return result;
        }
        if (std::regex_match(expression, match, std::regex(R"(VLOOKUP\((.*)\))"))) {
            const auto arguments = formula_arguments(match[1].str());
            if (arguments.size() < 3 || arguments.size() > 4) throw std::runtime_error("VLOOKUP expects a value, range, column, and optional match mode");
            std::smatch range_match;
            if (!std::regex_match(arguments[1], range_match, std::regex(R"(\s*([A-Z]+[0-9]+)\s*:\s*([A-Z]+[0-9]+)\s*)"))) throw std::runtime_error("VLOOKUP expects a cell range");
            std::smatch first_parts, last_parts;
            const std::string first_reference = range_match[1].str(), last_reference = range_match[2].str();
            std::regex_match(first_reference, first_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
            std::regex_match(last_reference, last_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
            const int first_column = spreadsheet_column(first_parts[1]), last_column = spreadsheet_column(last_parts[1]);
            const int first_row = std::stoi(first_parts[2]), last_row = std::stoi(last_parts[2]);
            const int columns = std::abs(last_column - first_column) + 1, rows = std::abs(last_row - first_row) + 1;
            const int column = static_cast<int>(std::llround(evaluate(arguments[2], stack)));
            if (column < 1 || column > columns) throw std::runtime_error("VLOOKUP column is outside the range");
            const int mode = arguments.size() == 4 ? static_cast<int>(std::llround(evaluate(arguments[3], stack))) : 0;
            if (mode != 0 && mode != 1) throw std::runtime_error("VLOOKUP match mode must be 0 or 1");
            const double needle = evaluate(arguments[0], stack);
            const auto references = range_references(range_match[1], range_match[2]);
            int selected = -1;
            for (int row = 0; row < rows; ++row) {
                const double candidate = evaluate(references[static_cast<size_t>(row * columns)], stack);
                if (mode == 0 && std::abs(candidate - needle) <= 1e-12) { selected = row; break; }
                if (mode == 1) {
                    if (candidate > needle) break;
                    selected = row;
                }
            }
            if (selected < 0) throw std::runtime_error("VLOOKUP value was not found");
            return evaluate(references[static_cast<size_t>(selected * columns + column - 1)], stack);
        }
        if (std::regex_match(expression, match, std::regex(R"(HLOOKUP\((.*)\))"))) {
            const auto arguments = formula_arguments(match[1].str());
            if (arguments.size() < 3 || arguments.size() > 4) throw std::runtime_error("HLOOKUP expects a value, range, row, and optional match mode");
            std::smatch range_match;
            if (!std::regex_match(arguments[1], range_match, std::regex(R"(\s*([A-Z]+[0-9]+)\s*:\s*([A-Z]+[0-9]+)\s*)"))) throw std::runtime_error("HLOOKUP expects a cell range");
            std::smatch first_parts, last_parts;
            const std::string first_reference = range_match[1].str(), last_reference = range_match[2].str();
            std::regex_match(first_reference, first_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
            std::regex_match(last_reference, last_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
            const int first_column = spreadsheet_column(first_parts[1]), last_column = spreadsheet_column(last_parts[1]);
            const int first_row = std::stoi(first_parts[2]), last_row = std::stoi(last_parts[2]);
            const int columns = std::abs(last_column - first_column) + 1, rows = std::abs(last_row - first_row) + 1;
            const int row = static_cast<int>(std::llround(evaluate(arguments[2], stack)));
            if (row < 1 || row > rows) throw std::runtime_error("HLOOKUP row is outside the range");
            const int mode = arguments.size() == 4 ? static_cast<int>(std::llround(evaluate(arguments[3], stack))) : 0;
            if (mode != 0 && mode != 1) throw std::runtime_error("HLOOKUP match mode must be 0 or 1");
            const double needle = evaluate(arguments[0], stack);
            const auto references = range_references(range_match[1], range_match[2]);
            int selected = -1;
            for (int column = 0; column < columns; ++column) {
                const double candidate = evaluate(references[static_cast<size_t>(column)], stack);
                if (mode == 0 && std::abs(candidate - needle) <= 1e-12) { selected = column; break; }
                if (mode == 1) {
                    if (candidate > needle) break;
                    selected = column;
                }
            }
            if (selected < 0) throw std::runtime_error("HLOOKUP value was not found");
            return evaluate(references[static_cast<size_t>((row - 1) * columns + selected)], stack);
        }
        if (std::regex_match(expression, match, std::regex(R"((SUM|PRODUCT|AVERAGE|COUNT|COUNTA|COUNTBLANK|MIN|MAX|MEDIAN|STDEV)\(([A-Z]+[0-9]+):([A-Z]+[0-9]+)\))"))) {
            const std::string first = match[2], last = match[3];
            std::smatch first_parts, last_parts;
            std::regex_match(first, first_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
            std::regex_match(last, last_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
            int left = spreadsheet_column(first_parts[1]), right = spreadsheet_column(last_parts[1]);
            int top = std::stoi(first_parts[2]), bottom = std::stoi(last_parts[2]);
            double total = 0; double product = 1; int count = 0; int nonblank = 0; int blank = 0; double minimum = 0; double maximum = 0; std::vector<double> samples;
            for (int row = std::min(top, bottom); row <= std::max(top, bottom); ++row)
                for (int column = std::min(left, right); column <= std::max(left, right); ++column) {
                    int column_value = column; std::string name;
                    while (column_value) { name.insert(name.begin(), static_cast<char>('A' + (column_value - 1) % 26)); column_value = (column_value - 1) / 26; }
                    const std::string reference = name + std::to_string(row); const auto cell = cells.find(reference);
                    const bool is_blank = cell == cells.end() || cell->second.find_first_not_of(" \t\r\n") == std::string::npos;
                    if (is_blank) { ++blank; continue; }
                    ++nonblank;
                    const bool is_text = get_literal_cell_text(reference, name);
                    if (is_text) continue;
                    const double cell_value = evaluate(reference, stack); samples.push_back(cell_value); total += cell_value; product *= cell_value; if (count == 0) minimum = maximum = cell_value; else { minimum = std::min(minimum, cell_value); maximum = std::max(maximum, cell_value); } ++count;
                }
            if (match[1] == "PRODUCT" && count) return product;
            if (match[1] == "AVERAGE" && count) return total / count;
            if (match[1] == "COUNT") return static_cast<double>(count);
            if (match[1] == "COUNTA") return static_cast<double>(nonblank);
            if (match[1] == "COUNTBLANK") return static_cast<double>(blank);
            if (match[1] == "MIN") return minimum;
            if (match[1] == "MAX") return maximum;
            if (match[1] == "MEDIAN" && count) { std::sort(samples.begin(), samples.end()); const size_t middle = samples.size() / 2; return samples.size() % 2 ? samples[middle] : (samples[middle - 1] + samples[middle]) / 2.0; }
            if (match[1] == "STDEV" && count) { const double average = total / count; double squared = 0; for (double value : samples) squared += (value - average) * (value - average); return std::sqrt(squared / count); }
            return total;
        }
        if (expression.rfind("IFERROR(", 0) == 0 && expression.back() == ')') {
            const auto arguments = formula_arguments(expression.substr(8, expression.size() - 9));
            if (arguments.size() != 2) throw std::runtime_error("IFERROR expects two arguments");
            try { return evaluate(arguments[0], stack); } catch (...) { return evaluate(arguments[1], stack); }
        }
        if (expression.rfind("IF(", 0) == 0 && expression.back() == ')') {
            const auto arguments = formula_arguments(expression.substr(3, expression.size() - 4));
            if (arguments.size() != 3) throw std::runtime_error("IF expects a condition and two result arguments");
            std::smatch condition;
            const std::string condition_text = arguments[0];
            bool result = false;
            if (std::regex_match(condition_text, condition, std::regex(R"(\s*([^<>=]+?)\s*(==|>=|<=|>|<)\s*([^<>=]+?)\s*)"))) {
                const double left = evaluate(condition[1], stack), right = evaluate(condition[3], stack); const std::string operation = condition[2];
                result = operation == "==" ? left == right : operation == ">=" ? left >= right : operation == "<=" ? left <= right : operation == ">" ? left > right : left < right;
            } else {
                result = evaluate(condition_text, stack) != 0;
            }
            return evaluate(result ? arguments[1] : arguments[2], stack);
        }
        if (std::regex_match(expression, match, std::regex(R"((ISBLANK|ISNUMBER|ISTEXT)\(([A-Z]+[0-9]+)\))"))) {
            const std::string reference = match[2]; const auto cell = cells.find(reference);
            const bool blank = cell == cells.end() || cell->second.find_first_not_of(" \t\r\n") == std::string::npos;
            std::string text;
            const bool text_value = !blank && get_literal_cell_text(reference, text);
            if (match[1] == "ISBLANK") return blank ? 1.0 : 0.0;
            if (match[1] == "ISTEXT") return text_value ? 1.0 : 0.0;
            return (!blank && !text_value) ? 1.0 : 0.0;
        }
        if (std::regex_match(expression, match, std::regex(R"((SUMIFS|COUNTIFS|AVERAGEIFS|MINIFS|MAXIFS)\((.*)\))"))) {
            const auto arguments = formula_arguments(match[2].str());
            const bool countifs = match[1] == "COUNTIFS";
            if ((countifs && (arguments.size() < 2 || arguments.size() % 2 != 0)) || (!countifs && (arguments.size() < 3 || arguments.size() % 2 == 0))) throw std::runtime_error(match[1].str() + " expects a result range and criteria range/criteria pairs");
            auto parse_range = [&](const std::string& text) {
                std::smatch range_match;
                if (!std::regex_match(text, range_match, std::regex(R"(\s*([A-Z]+[0-9]+)\s*:\s*([A-Z]+[0-9]+)\s*)"))) throw std::runtime_error("conditional aggregate expects cell ranges");
                return range_references(range_match[1].str(), range_match[2].str());
            };
            const auto result_cells = parse_range(arguments[0]);
            std::vector<std::vector<std::string>> criteria_cells;
            for (size_t index = countifs ? 0 : 1; index < arguments.size(); index += 2) criteria_cells.push_back(parse_range(arguments[index]));
            for (const auto& range : criteria_cells) if (range.size() != result_cells.size()) throw std::runtime_error("conditional aggregate ranges must have equal sizes");
            auto trim = [](std::string value) {
                const size_t first = value.find_first_not_of(" \t");
                const size_t last = value.find_last_not_of(" \t\r\n");
                return first == std::string::npos ? std::string() : value.substr(first, last - first + 1);
            };
            auto matches = [&](const std::string& reference, std::string criteria) {
                criteria = trim(criteria);
                if (criteria.size() >= 2 && ((criteria.front() == '"' && criteria.back() == '"') || (criteria.front() == '\'' && criteria.back() == '\''))) criteria = criteria.substr(1, criteria.size() - 2);
                std::string operation = "==", expected_text = criteria;
                for (const std::string& candidate : {std::string(">="), std::string("<="), std::string("=="), std::string(">"), std::string("<")}) if (criteria.rfind(candidate, 0) == 0) { operation = candidate; expected_text = criteria.substr(candidate.size()); break; }
                double expected = 0; bool numeric_criteria = true;
                try { size_t parsed = 0; expected = std::stod(expected_text, &parsed); numeric_criteria = parsed == expected_text.size(); } catch (...) { numeric_criteria = false; }
                if (!numeric_criteria && operation == "==") { std::string text; return get_literal_cell_text(reference, text) && text == expected_text; }
                std::vector<std::string> nested; const double value = evaluate(reference, nested);
                if (operation == ">=") return value >= expected; if (operation == "<=") return value <= expected; if (operation == ">") return value > expected; if (operation == "<") return value < expected; return value == expected;
            };
            double total = 0; double extreme = 0; int count = 0;
            for (size_t row = 0; row < result_cells.size(); ++row) {
                bool included = true;
                for (size_t criterion = 0; criterion < criteria_cells.size(); ++criterion) if (!matches(criteria_cells[criterion][row], arguments[criterion * 2 + (countifs ? 1 : 2)])) { included = false; break; }
                if (!included) continue;
                ++count;
                if (match[1] != "COUNTIFS") {
                    std::vector<std::string> nested; const double value = evaluate(result_cells[row], nested);
                    total += value;
                    if (count == 1 || (match[1] == "MINIFS" && value < extreme) || (match[1] == "MAXIFS" && value > extreme)) extreme = value;
                }
            }
            if (match[1] == "COUNTIFS") return static_cast<double>(count);
            if (match[1] == "MINIFS" || match[1] == "MAXIFS") return count == 0 ? 0.0 : extreme;
            return match[1] == "AVERAGEIFS" ? (count == 0 ? 0.0 : total / count) : total;
        }
        if (std::regex_match(expression, match, std::regex(R"((SUMIF|COUNTIF|AVERAGEIF)\(([A-Z]+[0-9]+):([A-Z]+[0-9]+),\s*([^,]+),\s*([A-Z]+[0-9]+):([A-Z]+[0-9]+)\))"))) {
            const auto criteria_cells = range_references(match[2].str(), match[3].str());
            const auto result_cells = range_references(match[5].str(), match[6].str());
            if (criteria_cells.size() != result_cells.size()) throw std::runtime_error("conditional aggregate ranges must have equal sizes");
            std::string criteria = match[4].str(); const size_t first = criteria.find_first_not_of(" \t"); const size_t last = criteria.find_last_not_of(" \t");
            if (first != std::string::npos) criteria = criteria.substr(first, last - first + 1);
            if (criteria.size() >= 2 && criteria.front() == '"' && criteria.back() == '"') criteria = criteria.substr(1, criteria.size() - 2);
            std::string operation = "==", expected_text = criteria;
            for (const std::string& candidate : {std::string(">="), std::string("<="), std::string("=="), std::string(">"), std::string("<")}) if (criteria.rfind(candidate, 0) == 0) { operation = candidate; expected_text = criteria.substr(candidate.size()); break; }
            double expected = 0; bool numeric_criteria = true; try { size_t parsed = 0; expected = std::stod(expected_text, &parsed); numeric_criteria = parsed == expected_text.size(); } catch (...) { numeric_criteria = false; }
            auto matches = [&](const std::string& reference) {
                if (!numeric_criteria && operation == "==") { std::string text; return get_literal_cell_text(reference, text) && text == expected_text; }
                std::vector<std::string> nested; const double value = evaluate(reference, nested);
                if (operation == ">=") return value >= expected; if (operation == "<=") return value <= expected; if (operation == ">") return value > expected; if (operation == "<") return value < expected; return value == expected;
            };
            double total = 0; int count = 0;
            for (size_t index = 0; index < criteria_cells.size(); ++index) if (matches(criteria_cells[index])) { ++count; if (match[1] == "SUMIF" || match[1] == "AVERAGEIF") { std::vector<std::string> nested; total += evaluate(result_cells[index], nested); } }
            if (match[1] == "COUNTIF") return static_cast<double>(count);
            return match[1] == "AVERAGEIF" ? (count == 0 ? 0.0 : total / count) : total;
        }
        if (std::regex_match(expression, match, std::regex(R"((AND|OR|NOT)\(([^()]*)\))"))) {
            std::stringstream arguments(match[2].str());
            std::string argument;
            std::vector<double> values;
            while (std::getline(arguments, argument, ',')) values.push_back(evaluate(argument, stack));
            if (match[1] == "NOT") return values.size() == 1 && values.front() == 0 ? 1 : 0;
            if (match[1] == "AND") {
                for (double value : values) if (value == 0) return 0;
                return values.empty() ? 0 : 1;
            }
            for (double value : values) if (value != 0) return 1;
            return 0;
        }
        if (std::regex_match(expression, match, std::regex(R"(^(.+?)(==|>=|<=|>|<)(.+)$)"))) {
            const double left = evaluate(match[1].str(), stack), right = evaluate(match[3].str(), stack); const std::string operation = match[2].str();
            if (operation == "==") return left == right;
            if (operation == ">=") return left >= right;
            if (operation == "<=") return left <= right;
            if (operation == ">") return left > right;
            return left < right;
        }
        if (std::regex_match(expression, match, std::regex(R"(SUMPRODUCT\(([A-Z]+[0-9]+):([A-Z]+[0-9]+),\s*([A-Z]+[0-9]+):([A-Z]+[0-9]+)\))"))) {
            const auto left = range_references(match[1].str(), match[2].str());
            const auto right = range_references(match[3].str(), match[4].str());
            if (left.size() != right.size()) throw std::runtime_error("SUMPRODUCT ranges must have equal sizes");
            double result = 0; for (size_t index = 0; index < left.size(); ++index) { std::vector<std::string> nested; result += evaluate(left[index], nested) * evaluate(right[index], nested); }
            return result;
        }
        if (std::regex_match(expression, match, std::regex(R"((SUM|PRODUCT|AVERAGE|COUNT|COUNTA|COUNTBLANK|MIN|MAX|MEDIAN|STDEV)\(([^()]*)\))"))) {
            std::stringstream arguments(match[2].str());
            std::string argument;
            std::vector<double> values;
            int nonblank = 0, blank = 0;
            auto append_reference = [&](const std::string& reference) {
                const auto cell = cells.find(reference);
                const bool is_blank = cell == cells.end() || cell->second.find_first_not_of(" \t\r\n") == std::string::npos;
                if (is_blank) { ++blank; return; }
                ++nonblank;
                std::string text;
                if (get_literal_cell_text(reference, text)) return;
                values.push_back(evaluate(reference, stack));
            };
            while (std::getline(arguments, argument, ',')) {
                std::smatch range_parts;
                if (std::regex_match(argument, range_parts, std::regex(R"(\s*([A-Z]+[0-9]+)\s*:\s*([A-Z]+[0-9]+)\s*)"))) {
                    for (const auto& reference : range_references(range_parts[1].str(), range_parts[2].str())) append_reference(reference);
                } else {
                    const size_t first = argument.find_first_not_of(" \t"), last = argument.find_last_not_of(" \t");
                    const std::string trimmed = first == std::string::npos ? std::string() : argument.substr(first, last - first + 1);
                    if (trimmed.size() >= 2 && ((trimmed.front() == '"' && trimmed.back() == '"') || (trimmed.front() == '\'' && trimmed.back() == '\''))) ++nonblank;
                    else if (trimmed.empty()) ++blank;
                    else { ++nonblank; values.push_back(evaluate(trimmed, stack)); }
                }
            }
            if (match[1] == "COUNTA") return static_cast<double>(nonblank);
            if (match[1] == "COUNTBLANK") return static_cast<double>(blank);
            if (values.empty()) return 0;
            double total = 0, product = 1;
            for (double value : values) total += value;
            for (double value : values) product *= value;
            if (match[1] == "SUM") return total;
            if (match[1] == "PRODUCT") return product;
            if (match[1] == "AVERAGE") return total / values.size();
            if (match[1] == "COUNT") return static_cast<double>(values.size());
            if (match[1] == "MIN") return *std::min_element(values.begin(), values.end());
            if (match[1] == "MAX") return *std::max_element(values.begin(), values.end());
            if (match[1] == "MEDIAN") { std::sort(values.begin(), values.end()); const size_t middle = values.size() / 2; return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0; }
            const double average = total / values.size(); double squared = 0; for (double value : values) squared += (value - average) * (value - average);
            return std::sqrt(squared / values.size());
        }
        if (std::regex_match(expression, match, std::regex(R"((ABS|SQRT|SIN|COS|TAN|EXP|LN|LOG2|LOG10|ROUND|MOD)\(([^()]*)\))"))) {
            std::stringstream arguments(match[2].str()); std::string first_argument, second_argument;
            std::getline(arguments, first_argument, ','); std::getline(arguments, second_argument, ',');
            const double value = evaluate(first_argument, stack);
            const std::string function = match[1].str();
            if (function == "ABS") return std::abs(value);
            if (function == "SQRT") return std::sqrt(value);
            if (function == "SIN") return std::sin(value);
            if (function == "COS") return std::cos(value);
            if (function == "TAN") return std::tan(value);
            if (function == "EXP") return std::exp(value);
            if (function == "LN") return std::log(value);
            if (function == "LOG2") return std::log2(value);
            if (function == "LOG10") return std::log10(value);
            if (function == "ROUND" && !second_argument.empty()) { const double digits = evaluate(second_argument, stack); const double scale = std::pow(10, digits); return std::round(value * scale) / scale; }
            if (function == "MOD" && !second_argument.empty()) {
                const double divisor = evaluate(second_argument, stack);
                if (divisor == 0) throw std::runtime_error("spreadsheet modulo by zero");
                return std::fmod(value, divisor);
            }
            return std::round(value);
        }
        if (std::regex_match(expression, match, std::regex(R"(POWER\(([^,]+),([^\)]+)\))")))
            return std::pow(evaluate(match[1].str(), stack), evaluate(match[2].str(), stack));
        int depth = 0;
        for (size_t index = expression.size(); index-- > 0;) {
            char operation = expression[index];
            if (operation == ')') { ++depth; continue; }
            if (operation == '(') { --depth; continue; }
            size_t previous = index; while (previous > 0 && std::isspace(static_cast<unsigned char>(expression[previous - 1]))) --previous;
            const char previous_character = previous > 0 ? expression[previous - 1] : '\0';
            if (depth == 0 && (operation == '+' || operation == '-') && index > 0 && previous_character != '+' && previous_character != '-' && previous_character != '*' && previous_character != '/' && previous_character != '(') {
                return operation == '+' ? evaluate(expression.substr(0, index), stack) + evaluate(expression.substr(index + 1), stack)
                                        : evaluate(expression.substr(0, index), stack) - evaluate(expression.substr(index + 1), stack);
            }
        }
        depth = 0;
        for (size_t index = expression.size(); index-- > 0;) {
            char operation = expression[index];
            if (operation == ')') { ++depth; continue; }
            if (operation == '(') { --depth; continue; }
            if (depth == 0 && (operation == '*' || operation == '/') && index > 0 && expression[index - 1] != '(') {
                double right = evaluate(expression.substr(index + 1), stack);
                if (operation == '/' && right == 0) throw std::runtime_error("spreadsheet division by zero");
                return operation == '*' ? evaluate(expression.substr(0, index), stack) * right
                                        : evaluate(expression.substr(0, index), stack) / right;
            }
        }
        try { return std::stod(expression); }
        catch (...) {
            if (std::regex_search(expression, std::regex(R"(^[A-Za-z_][A-Za-z0-9_]*\s*\()")))
                throw std::runtime_error("unsupported spreadsheet formula '" + expression + "'");
            if (std::regex_match(expression, std::regex(R"([A-Za-z_][A-Za-z0-9_]*)")))
                throw std::runtime_error("unknown spreadsheet name '" + expression + "'");
            return 0;
        }
    };

    int max_column = 1, max_row = 1;
    for (const auto& entry : cells) {
        std::smatch match;
        if (std::regex_match(entry.first, match, std::regex(R"(([A-Z]+)([0-9]+))"))) {
            max_column = std::max(max_column, spreadsheet_column(match[1]));
            max_row = std::max(max_row, std::stoi(match[2]));
        }
    }
    std::stringstream table;
    auto literal_cell_text = [](std::string value, bool& is_literal) {
        const size_t first = value.find_first_not_of(" \t");
        value = first == std::string::npos ? std::string() : value.substr(first);
        is_literal = value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''));
        if (is_literal) {
            const char quote = value.front(); value = value.substr(1, value.size() - 2);
            std::string result;
            for (size_t index = 0; index < value.size(); ++index) {
                if (value[index] == quote && index + 1 < value.size() && value[index + 1] == quote) ++index;
                result += value[index];
            }
            return result;
        }
        return std::string();
    };
    table << "<table><thead><tr><th></th>";
    for (int column = 1; column <= max_column; ++column) {
        int value = column; std::string name;
        while (value) { name.insert(name.begin(), static_cast<char>('A' + (value - 1) % 26)); value = (value - 1) / 26; }
        table << "<th>" << name << "</th>";
    }
    table << "</tr></thead><tbody>";
    for (int row = 1; row <= max_row; ++row) {
        table << "<tr><th>" << row << "</th>";
        for (int column = 1; column <= max_column; ++column) {
            int value = column; std::string name;
            while (value) { name.insert(name.begin(), static_cast<char>('A' + (value - 1) % 26)); value = (value - 1) / 26; }
            const std::string ref = name + std::to_string(row);
            auto cell = cells.find(ref); std::string display;
            if (cell != cells.end()) {
                bool is_literal = false; const std::string literal = literal_cell_text(cell->second, is_literal);
                if (is_literal) display = literal;
                try { if (!is_literal) { std::vector<std::string> stack; display = std::to_string(evaluate(ref, stack)); while (!display.empty() && display.back() == '0') display.pop_back(); if (!display.empty() && display.back() == '.') display.pop_back(); } }
                catch (const std::exception& error) { display = std::string("#ERROR: ") + error.what(); }
            }
            table << "<td data-cell=\"" << ref << "\">" << html_escape(display) << "</td>";
        }
        table << "</tr>";
    }
    table << "</tbody></table>";
    std::stringstream charts;
    const std::regex chart_pattern(R"MARILA(chart\s+(bar|line|scatter)\s+from\s+([A-Z]+[0-9]+)\s*:\s*([A-Z]+[0-9]+)(?:\s+title\s+"([^"]+)")?)MARILA");
    for (std::sregex_iterator chart_it(source.begin(), source.end(), chart_pattern), chart_end; chart_it != chart_end; ++chart_it) {
        const std::smatch& chart_match = *chart_it;
        std::smatch first_parts, last_parts;
        const std::string chart_type = chart_match[1].str();
        const std::string first_reference = chart_match[2].str(), last_reference = chart_match[3].str();
        const std::string chart_title = chart_match[4].str();
        std::regex_match(first_reference, first_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
        std::regex_match(last_reference, last_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
        const int left = spreadsheet_column(first_parts[1]), right = spreadsheet_column(last_parts[1]);
        const int top = std::stoi(first_parts[2]), bottom = std::stoi(last_parts[2]);
        std::vector<double> values, scatter_x;
        const bool paired_scatter = chart_type == "scatter" && right - left >= 1;
        for (int row = std::min(top, bottom); row <= std::max(top, bottom); ++row) {
            if (paired_scatter) {
                int x_column = left; std::string x_name;
                while (x_column) { x_name.insert(x_name.begin(), static_cast<char>('A' + (x_column - 1) % 26)); x_column = (x_column - 1) / 26; }
                int y_column = left + 1; std::string y_name;
                while (y_column) { y_name.insert(y_name.begin(), static_cast<char>('A' + (y_column - 1) % 26)); y_column = (y_column - 1) / 26; }
                std::vector<std::string> stack; scatter_x.push_back(evaluate(x_name + std::to_string(row), stack));
                stack.clear(); values.push_back(evaluate(y_name + std::to_string(row), stack));
                continue;
            }
            for (int column = std::min(left, right); column <= std::max(left, right); ++column) {
                int value = column; std::string name;
                while (value) { name.insert(name.begin(), static_cast<char>('A' + (value - 1) % 26)); value = (value - 1) / 26; }
                std::vector<std::string> stack; values.push_back(evaluate(name + std::to_string(row), stack));
            }
        }
        const double maximum = values.empty() ? 1 : std::max(1.0, *std::max_element(values.begin(), values.end()));
        const double minimum = values.empty() ? 0 : std::min(0.0, *std::min_element(values.begin(), values.end()));
        charts << "<svg class=\"chart\" viewBox=\"0 0 560 260\" width=\"560\" height=\"260\">";
        if (!chart_title.empty()) charts << "<text x=\"40\" y=\"24\" fill=\"#e2e8f0\" font-size=\"18\">" << html_escape(chart_title) << "</text>";
        charts << "<line x1=\"40\" y1=\"220\" x2=\"540\" y2=\"220\" stroke=\"#94a3b8\"/>";
        if (chart_type == "line") {
            charts << "<polyline fill=\"none\" stroke=\"#38bdf8\" stroke-width=\"3\" points=\"";
            for (size_t index = 0; index < values.size(); ++index) {
                const double x = 50 + index * (490.0 / std::max<size_t>(1, values.size() - 1));
                const double y = 220 - 180 * (values[index] - minimum) / std::max(1e-12, maximum - minimum);
                charts << x << "," << y << " ";
            }
            charts << "\"/>";
            for (size_t index = 0; index < values.size(); ++index) {
                const double x = 50 + index * (490.0 / std::max<size_t>(1, values.size() - 1));
                const double y = 220 - 180 * (values[index] - minimum) / std::max(1e-12, maximum - minimum);
                charts << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"4\" fill=\"#f8fafc\"><title>" << values[index] << "</title></circle>";
            }
        } else if (chart_type == "scatter") {
            const double x_min = scatter_x.empty() ? 0 : *std::min_element(scatter_x.begin(), scatter_x.end());
            const double x_max = scatter_x.empty() ? std::max(1.0, static_cast<double>(values.size() - 1)) : *std::max_element(scatter_x.begin(), scatter_x.end());
            for (size_t index = 0; index < values.size(); ++index) {
                const double sample_x = scatter_x.empty() ? static_cast<double>(index) : scatter_x[index];
                const double x = 50 + 490 * (sample_x - x_min) / std::max(1e-12, x_max - x_min);
                const double y = 220 - 180 * (values[index] - minimum) / std::max(1e-12, maximum - minimum);
                charts << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"5\" fill=\"#f59e0b\"><title>" << values[index] << "</title></circle>";
            }
        } else for (size_t index = 0; index < values.size(); ++index) {
            const double height = 180 * values[index] / maximum; const double x = 50 + index * (490.0 / std::max<size_t>(1, values.size()));
            charts << "<rect x=\"" << x << "\" y=\"" << 220 - height << "\" width=\"" << std::max(8.0, 400.0 / std::max<size_t>(1, values.size())) << "\" height=\"" << height << "\" fill=\"#38bdf8\"><title>" << values[index] << "</title></rect>";
        }
        charts << "</svg>";
    }
    return "<!doctype html><meta charset=\"utf-8\"><title>Marila sheet</title><style>body{font:16px sans-serif;margin:32px;background:#0f172a;color:#e2e8f0}table{border-collapse:collapse;background:#1e293b}th,td{border:1px solid #475569;padding:8px 14px;min-width:70px}th{background:#334155}.chart{display:block;margin-top:24px;background:#1e293b;border:1px solid #475569}</style>" + table.str() + charts.str();
}

std::string render_image(const std::string& source) {
    struct ScalarExpression {
        std::string text; size_t position{0}; double variable{0}; double secondary{0}; double tertiary{0};
        void skip() { while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) ++position; }
        bool match(char value) { skip(); if (position < text.size() && text[position] == value) { ++position; return true; } return false; }
        double primary() {
            skip();
            if (match('(')) { double value = add(); match(')'); return value; }
            if (position < text.size() && (std::isdigit(static_cast<unsigned char>(text[position])) || text[position] == '.')) {
                const char* start = text.c_str() + position; char* end = nullptr; const double value = std::strtod(start, &end); position += static_cast<size_t>(end - start); return value;
            }
            if (position < text.size() && (std::isalpha(static_cast<unsigned char>(text[position])) || text[position] == '_')) {
                size_t end = position; while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_')) ++end;
                const std::string name = text.substr(position, end - position); position = end; skip();
                if (match('(')) { double first = add(); double second = 0; double third = 0; bool two = match(','); if (two) { second = add(); if (match(',')) third = add(); } match(')');
                    if (name == "sin") return std::sin(first); if (name == "cos") return std::cos(first); if (name == "tan") return std::tan(first); if (name == "sinh") return std::sinh(first); if (name == "cosh") return std::cosh(first); if (name == "tanh") return std::tanh(first); if (name == "asinh") return std::asinh(first); if (name == "acosh") return std::acosh(first); if (name == "atanh") return std::atanh(first);
                    if (name == "asin") return std::asin(first); if (name == "acos") return std::acos(first); if (name == "sqrt") return std::sqrt(first);
                    if (name == "atan") return std::atan(first); if (name == "exp") return std::exp(first); if (name == "log") return std::log(first); if (name == "log2") return std::log2(first); if (name == "log10") return std::log10(first);
                    if (name == "abs") return std::abs(first); if (name == "floor") return std::floor(first); if (name == "ceil") return std::ceil(first); if (name == "min") return std::min(first, second); if (name == "max") return std::max(first, second); if (name == "clamp") return std::max(second, std::min(third, first)); if (name == "atan2") return std::atan2(first, second); if (name == "hypot") return std::hypot(first, second);
                    if (name == "pow") return std::pow(first, second); if (name == "sign") return first > 0 ? 1.0 : (first < 0 ? -1.0 : 0.0);
                    return 0;
                }
                if (name == "x" || name == "t") return variable; if (name == "y") return secondary; if (name == "pi") return 3.141592653589793; if (name == "e") return 2.718281828459045; return 0;
            }
            return 0;
        }
        double unary() { skip(); if (match('+')) return unary(); if (match('-')) return -unary(); return primary(); }
        double power() { double value = unary(); if (match('^')) value = std::pow(value, power()); return value; }
        double multiply() { double value = power(); while (true) { if (match('*')) value *= power(); else if (match('/')) value /= power(); else break; } return value; }
        double add() { double value = multiply(); while (true) { if (match('+')) value += multiply(); else if (match('-')) value -= multiply(); else break; } return value; }
        double evaluate(double input) { variable = input; position = 0; return add(); }
        double evaluate2(double first, double second) { variable = first; secondary = second; position = 0; return add(); }
    };
    struct Point { std::string id; double x; double y; };
    std::vector<Point> points;
    std::stringstream elements;
    const auto nodes = parse_visual_source(source);
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Point) {
        points.push_back({node.id, node.x, node.y});
        elements << "<circle cx=\"" << node.x << "\" cy=\"" << -node.y
                  << "\" r=\"4\" fill=\"#06b6d4\"/><text x=\"" << node.x + 8
                  << "\" y=\"" << -node.y - 8 << "\" fill=\"white\">" << html_escape(node.label.empty() ? node.id : node.label) << "</text>\n";
    }
    auto find = [&](const std::string& id) { for (const auto& point : points) if (point.id == id) return point; return Point{id, 0, 0}; };
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Line) {
        Point first = find(node.first), second = find(node.second);
        elements << "<line x1=\"" << first.x << "\" y1=\"" << -first.y << "\" x2=\"" << second.x
                  << "\" y2=\"" << -second.y << "\" stroke=\"#6366f1\" stroke-width=\"3\"/>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Triangle) {
        Point first = find(node.first), second = find(node.second), third = find(node.third);
        const std::string color = svg_color(node.color, "#f59e0b");
        elements << "<polygon points=\"" << first.x << "," << -first.y << " "
                  << second.x << "," << -second.y << " "
                  << third.x << "," << -third.y
                  << "\" fill=\"" << color << "22\" stroke=\"" << color << "\" stroke-width=\"2\"/>\n";
        if (node.show_angles) {
            auto angle = [](Point vertex, Point first_side, Point second_side) {
                const double ax = first_side.x - vertex.x, ay = first_side.y - vertex.y, bx = second_side.x - vertex.x, by = second_side.y - vertex.y;
                const double lengths = std::sqrt(ax * ax + ay * ay) * std::sqrt(bx * bx + by * by);
                return lengths == 0 ? 0.0 : std::acos(std::max(-1.0, std::min(1.0, (ax * bx + ay * by) / lengths))) * 180.0 / 3.141592653589793;
            };
            elements << "<text x=\"" << first.x + 6 << "\" y=\"" << -first.y - 6 << "\" fill=\"white\">" << std::round(angle(first, second, third)) << "°</text>"
                      << "<text x=\"" << second.x + 6 << "\" y=\"" << -second.y - 6 << "\" fill=\"white\">" << std::round(angle(second, first, third)) << "°</text>"
                      << "<text x=\"" << third.x + 6 << "\" y=\"" << -third.y - 6 << "\" fill=\"white\">" << std::round(angle(third, first, second)) << "°</text>\n";
        }
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Circle) {
        Point center = find(node.first);
        elements << "<circle cx=\"" << center.x << "\" cy=\"" << -center.y
                  << "\" r=\"" << node.radius << "\" fill=\"none\" stroke=\"#22d3ee\" stroke-width=\"2\""
                  << (node.dashed ? " stroke-dasharray=\"6 4\"" : "") << "/>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Data2D) {
        std::ostringstream path;
        for (size_t index = 0; index < node.samples.size(); ++index)
            path << (index ? " L " : "M ") << node.samples[index].first << "," << -node.samples[index].second;
        elements << "<g data-plot=\"data2d\" fill=\"none\" stroke=\"" << svg_color(node.color, "#f59e0b") << "\" stroke-width=\"2\">";
        if (!node.title.empty()) elements << "<title>" << html_escape(node.title) << "</title>";
        elements << "<path d=\"" << path.str() << "\"/>\n</g>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Plot2D || node.kind == SceneNode::Kind::Parametric2D) {
        ScalarExpression first{node.kind == SceneNode::Kind::Plot2D ? node.expression : node.first};
        ScalarExpression second{node.kind == SceneNode::Kind::Plot2D ? "" : node.second};
        std::ostringstream path; bool started = false;
        const int samples = 400;
        for (int index = 0; index <= samples; ++index) {
            const double parameter = node.xmin + (node.xmax - node.xmin) * index / samples;
            const double x = node.kind == SceneNode::Kind::Plot2D ? parameter : first.evaluate(parameter);
            const double y = node.kind == SceneNode::Kind::Plot2D ? first.evaluate(parameter) : second.evaluate(parameter);
            if (!std::isfinite(x) || !std::isfinite(y)) { started = false; continue; }
            path << (started ? " L " : "M ") << x << "," << -y; started = true;
        }
        elements << "<g data-plot=\"2d\" fill=\"none\" stroke=\"" << svg_color(node.color, "#38bdf8") << "\" stroke-width=\"2\">";
        if (!node.title.empty()) elements << "<title>" << html_escape(node.title) << "</title>";
        elements << "<path d=\"" << path.str() << "\"/>\n</g>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Implicit2D) {
        ScalarExpression expression{node.expression};
        const std::string color = svg_color(node.color, "#fb7185");
        elements << "<g data-plot=\"implicit\" fill=\"" << color << "\">";
        if (!node.title.empty()) elements << "<title>" << html_escape(node.title) << "</title>";
        for (int x_index = 0; x_index <= 120; ++x_index) {
            const double x = node.xmin + (node.xmax - node.xmin) * x_index / 120.0;
            for (int y_index = 0; y_index <= 120; ++y_index) {
                const double y = node.ymin + (node.ymax - node.ymin) * y_index / 120.0;
                const double value = expression.evaluate2(x, y);
                if (std::isfinite(value) && std::abs(value) < 0.08) elements << "<circle cx=\"" << x << "\" cy=\"" << -y << "\" r=\"0.04\"/>";
            }
        }
        elements << "</g>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::VectorField2D) {
        const std::string color = svg_color(node.color, "#22d3ee");
        ScalarExpression horizontal{node.first}, vertical{node.second};
        elements << "<g data-plot=\"vectorfield\" stroke=\"" << color << "\" stroke-width=\"1.2\">";
        if (!node.title.empty()) elements << "<title>" << html_escape(node.title) << "</title>";
        for (int row = 0; row <= 10; ++row) for (int column = 0; column <= 10; ++column) {
            const double px = node.xmin + (node.xmax - node.xmin) * column / 10.0;
            const double py = node.ymin + (node.ymax - node.ymin) * row / 10.0;
            const double dx = horizontal.evaluate2(px, py), dy = vertical.evaluate2(px, py);
            if (!std::isfinite(dx) || !std::isfinite(dy)) continue;
            elements << "<line x1=\"" << px << "\" y1=\"" << -py << "\" x2=\"" << px + dx * .18 << "\" y2=\"" << -(py + dy * .18) << "\"/>";
        }
        elements << "</g>\n";
    }
    auto project_3d = [](double x, double y, double z) {
        const double yaw = 0.65, pitch = 0.45;
        const double cy = std::cos(yaw), sy = std::sin(yaw);
        const double cp = std::cos(pitch), sp = std::sin(pitch);
        const double rotated_x = x * cy - z * sy;
        const double rotated_z = x * sy + z * cy;
        const double rotated_y = y * cp - rotated_z * sp;
        const double depth = y * sp + rotated_z * cp;
        return std::pair<double, double>{(rotated_x - rotated_y) * 28.0, -(rotated_x + rotated_y) * 14.0 - depth * 20.0};
    };
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Parametric4D) {
        ScalarExpression expressions[4] = {ScalarExpression{node.first}, ScalarExpression{node.second}, ScalarExpression{node.third}, ScalarExpression{node.fourth}};
        const std::string color = svg_color(node.color, "#c084fc"); std::ostringstream path; bool started = false;
        for (int index = 0; index <= 400; ++index) {
            const double parameter = node.xmin + (node.xmax - node.xmin) * index / 400.0; double values[4]; bool valid = true;
            for (auto& expression : expressions) { const double value = expression.evaluate(parameter); if (!std::isfinite(value)) valid = false; values[&expression - expressions] = value; }
            if (!valid) { started = false; continue; }
            const double angle = .5, x = values[0] * std::cos(angle) - values[3] * std::sin(angle), z = values[0] * std::sin(angle) + values[3] * std::cos(angle);
            const auto point = project_3d(x, values[1], values[2] + z * .15);
            path << (started ? " L " : "M ") << point.first << ',' << point.second; started = true;
        }
        elements << "<g data-plot=\"4d\" fill=\"none\" stroke=\"" << color << "\" stroke-width=\"1.8\">";
        if (!node.title.empty()) elements << "<title>" << html_escape(node.title) << "</title>";
        elements << "<path d=\"" << path.str() << "\"/></g>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Plot4D) {
        const std::array<std::pair<int, int>, 32> edges{{{0,1},{0,2},{0,4},{0,8},{1,3},{1,5},{1,9},{2,3},{2,6},{2,10},{3,7},{3,11},{4,5},{4,6},{4,12},{5,7},{5,13},{6,7},{6,14},{7,15},{8,9},{8,10},{8,12},{9,11},{9,13},{10,11},{10,14},{11,15},{12,13},{12,14},{13,15},{14,15}}};
        elements << "<g data-plot=\"4d-wireframe\" fill=\"none\" stroke=\"#a855f7\" stroke-width=\"1.2\">";
        auto corner = [&](int index) { return project_3d((index & 1) ? 1.5 : -1.5, (index & 2) ? 1.5 : -1.5, (index & 4) ? 1.5 : -1.5 + (index & 8 ? .35 : -.35)); };
        for (const auto& edge : edges) { const auto first = corner(edge.first), second = corner(edge.second); elements << "<line x1=\"" << first.first << "\" y1=\"" << first.second << "\" x2=\"" << second.first << "\" y2=\"" << second.second << "\"/>"; }
        elements << "</g>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Plot3D) {
        ScalarExpression expression{node.expression};
        const std::string color = svg_color(node.color, "#10b981");
        elements << "<g data-plot=\"3d\" fill=\"none\" stroke=\"" << color << "\" stroke-width=\"1.2\">";
        if (!node.title.empty()) elements << "<title>" << html_escape(node.title) << "</title>";
        const int samples = 28;
        for (int fixed = 0; fixed <= samples; ++fixed) {
            const double fixed_value = node.xmin + (node.xmax - node.xmin) * fixed / samples;
            std::ostringstream x_path, y_path;
            bool x_started = false, y_started = false;
            for (int varying = 0; varying <= samples; ++varying) {
                const double varying_value = node.ymin + (node.ymax - node.ymin) * varying / samples;
                const double z_for_x = expression.evaluate2(fixed_value, varying_value);
                const auto projected_x = project_3d(fixed_value, varying_value, z_for_x);
                if (std::isfinite(z_for_x)) {
                    x_path << (x_started ? " L " : "M ") << projected_x.first << "," << projected_x.second;
                    x_started = true;
                }
                const double z_for_y = expression.evaluate2(varying_value, fixed_value);
                const auto projected_y = project_3d(varying_value, fixed_value, z_for_y);
                if (std::isfinite(z_for_y)) {
                    y_path << (y_started ? " L " : "M ") << projected_y.first << "," << projected_y.second;
                    y_started = true;
                }
            }
            if (x_started) elements << "<path d=\"" << x_path.str() << "\"/>";
            if (y_started) elements << "<path d=\"" << y_path.str() << "\"/>";
        }
        elements << "</g>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Data3D) {
        const std::string color = svg_color(node.color, "#a855f7");
        elements << "<g data-plot=\"data3d\" fill=\"none\" stroke=\"" << color << "\" stroke-width=\"1.2\">";
        if (!node.title.empty()) elements << "<title>" << html_escape(node.title) << "</title>";
        if (!node.grid.empty()) {
            const size_t rows = node.grid.size(), columns = node.grid.front().size();
            for (const auto& row : node.grid) if (row.size() == columns) {
                std::ostringstream path;
                for (size_t column = 0; column < columns; ++column) {
                    const auto point = project_3d(static_cast<double>(column) - (columns - 1) / 2.0, static_cast<double>(rows - 1) / 2.0 - (&row - &node.grid.front()), row[column]);
                    path << (column ? " L " : "M ") << point.first << "," << point.second;
                }
                elements << "<path d=\"" << path.str() << "\"/>";
            }
            for (size_t column = 0; column < columns; ++column) {
                std::ostringstream path; bool started = false;
                for (size_t row = 0; row < rows; ++row) if (node.grid[row].size() == columns) {
                    const auto point = project_3d(static_cast<double>(column) - (columns - 1) / 2.0, static_cast<double>(rows - 1) / 2.0 - row, node.grid[row][column]);
                    path << (started ? " L " : "M ") << point.first << "," << point.second; started = true;
                }
                if (started) elements << "<path d=\"" << path.str() << "\"/>";
            }
        }
        elements << "</g>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Sphere) {
        const auto center = project_3d(node.x, node.y, node.z);
        const auto edge = project_3d(node.x + node.radius, node.y, node.z);
        const double radius = std::max(1.0, std::hypot(edge.first - center.first, edge.second - center.second));
        elements << "<g data-solid=\"sphere\" stroke=\"#38bdf8\" fill=\"none\"><ellipse cx=\"" << center.first
                  << "\" cy=\"" << center.second << "\" rx=\"" << radius << "\" ry=\"" << radius * .62
                  << "\"/><ellipse cx=\"" << center.first << "\" cy=\"" << center.second << "\" rx=\"" << radius * .62
                  << "\" ry=\"" << radius << "\"/></g>\n";
    }
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Cube) {
        const double half = node.size / 2.0;
        const std::vector<std::array<double, 3>> corners = {
            {-half, -half, -half}, {half, -half, -half}, {half, half, -half}, {-half, half, -half},
            {-half, -half, half}, {half, -half, half}, {half, half, half}, {-half, half, half}
        };
        const std::vector<std::pair<int, int>> edges = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        elements << "<g data-solid=\"cube\" stroke=\"#f97316\" fill=\"none\">";
        for (const auto& edge : edges) {
            const auto first = project_3d(node.x + corners[edge.first][0], node.y + corners[edge.first][1], node.z + corners[edge.first][2]);
            const auto second = project_3d(node.x + corners[edge.second][0], node.y + corners[edge.second][1], node.z + corners[edge.second][2]);
            elements << "<line x1=\"" << first.first << "\" y1=\"" << first.second << "\" x2=\"" << second.first << "\" y2=\"" << second.second << "\"/>";
        }
        elements << "</g>\n";
    }
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-200 -200 400 400\" style=\"background:#0f172a\">"
           "<g stroke=\"rgba(255,255,255,.12)\"><path d=\"M-200 0H200M0-200V200\"/></g>" +
           elements.str() + "</svg>";
}

std::string render_scene(const std::string& source) {
    // Plot expressions are intentionally kept as a small, portable expression
    // subset for the native HTML renderer. The language parser remains the
    // source of truth for program execution; this converts the same notation
    // into JavaScript for interactive rendering.
    auto js_expression = [](std::string expression, const std::string& x, const std::string& y = "") {
        auto replace_all = [](std::string& value, const std::string& from, const std::string& to) {
            size_t start = 0;
            while ((start = value.find(from, start)) != std::string::npos) {
                value.replace(start, from.size(), to); start += to.size();
            }
        };
        auto replace_identifier = [](std::string& value, const std::string& from, const std::string& to) {
            value = std::regex_replace(value, std::regex("\\b" + from + "\\b"), to);
        };
        expression = std::regex_replace(expression, std::regex(R"(\bx\b)"), x);
        expression = std::regex_replace(expression, std::regex(R"(\bt\b)"), x);
        if (!y.empty()) expression = std::regex_replace(expression, std::regex(R"(\by\b)"), y);
        replace_all(expression, "^", "**");
        replace_identifier(expression, "asin", "__MARILA_ASIN__");
        replace_identifier(expression, "acos", "__MARILA_ACOS__");
        replace_identifier(expression, "atan2", "__MARILA_ATAN2__");
        replace_identifier(expression, "atan", "__MARILA_ATAN__");
        replace_identifier(expression, "log2", "__MARILA_LOG2__");
        replace_identifier(expression, "log10", "__MARILA_LOG10__");
        replace_identifier(expression, "asinh", "Math.asinh");
        replace_identifier(expression, "acosh", "Math.acosh");
        replace_identifier(expression, "atanh", "Math.atanh");
        replace_identifier(expression, "sinh", "Math.sinh");
        replace_identifier(expression, "cosh", "Math.cosh");
        replace_identifier(expression, "tanh", "Math.tanh");
        replace_identifier(expression, "sin", "Math.sin");
        replace_identifier(expression, "cos", "Math.cos");
        replace_identifier(expression, "tan", "Math.tan");
        replace_identifier(expression, "sqrt", "Math.sqrt");
        replace_identifier(expression, "floor", "Math.floor");
        replace_identifier(expression, "ceil", "Math.ceil");
        replace_identifier(expression, "exp", "Math.exp");
        replace_identifier(expression, "log", "Math.log");
        replace_identifier(expression, "hypot", "Math.hypot");
        replace_identifier(expression, "abs", "Math.abs");
        replace_identifier(expression, "pi", "Math.PI");
        replace_all(expression, "__MARILA_ASIN__", "Math.asin");
        replace_all(expression, "__MARILA_ACOS__", "Math.acos");
        replace_all(expression, "__MARILA_ATAN__", "Math.atan");
        replace_all(expression, "__MARILA_ATAN2__", "Math.atan2");
        replace_all(expression, "__MARILA_LOG2__", "Math.log2");
        replace_all(expression, "__MARILA_LOG10__", "Math.log10");
        replace_identifier(expression, "min", "Math.min");
        replace_identifier(expression, "max", "Math.max");
        replace_identifier(expression, "pow", "Math.pow");
        replace_identifier(expression, "sign", "Math.sign");
        return expression;
    };
    const auto nodes = parse_visual_source(source);
    std::map<std::string, std::pair<double, double>> scene_points;
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Point) scene_points[node.id] = {node.x, node.y};
    std::vector<std::string> animations;
    for (const auto& node : nodes) {
        if (node.kind == SceneNode::Kind::Point) {
            std::ostringstream item; item << "{type:'point',id:" << std::quoted(node.id) << ",x:" << node.x << ",y:" << node.y << ",label:" << std::quoted(node.label.empty() ? node.id : node.label) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Move) {
            std::ostringstream item; item << "{type:'move',id:" << std::quoted(node.id) << ",x:" << node.from_x << ",y:" << node.from_y << ",tx:" << node.x << ",ty:" << node.y << ",duration:" << node.duration << ",start:" << node.start_time << ",repeat:" << node.repeat << ",yoyo:" << (node.yoyo ? "true" : "false") << ",easing:" << std::quoted(node.easing) << ",label:" << std::quoted(node.id) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Fade) {
            std::ostringstream item; item << "{type:'fade',target:" << std::quoted(node.first) << ",fadeIn:" << (node.fade_in ? "true" : "false") << ",duration:" << node.duration << ",start:" << node.start_time << ",repeat:" << node.repeat << ",yoyo:" << (node.yoyo ? "true" : "false") << ",easing:" << std::quoted(node.easing) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Wait) {
            std::ostringstream item; item << "{type:'wait',duration:" << node.duration << ",start:" << node.start_time << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Line) {
            const auto first = scene_points.count(node.first) ? scene_points[node.first] : std::pair<double, double>{0, 0};
            const auto second = scene_points.count(node.second) ? scene_points[node.second] : std::pair<double, double>{0, 0};
            std::ostringstream item; item << "{type:'line',x1:" << first.first << ",y1:" << first.second << ",x2:" << second.first << ",y2:" << second.second << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Triangle) {
            const auto first = scene_points.count(node.first) ? scene_points[node.first] : std::pair<double, double>{0, 0};
            const auto second = scene_points.count(node.second) ? scene_points[node.second] : std::pair<double, double>{0, 0};
            const auto third = scene_points.count(node.third) ? scene_points[node.third] : std::pair<double, double>{0, 0};
            std::ostringstream item; item << "{type:'triangle',x1:" << first.first << ",y1:" << first.second << ",x2:" << second.first << ",y2:" << second.second << ",x3:" << third.first << ",y3:" << third.second << ",color:" << std::quoted(svg_color(node.color, "#f59e0b")) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Circle) {
            const auto center = scene_points.count(node.first) ? scene_points[node.first] : std::pair<double, double>{0, 0};
            std::ostringstream item; item << "{type:'circle',x:" << center.first << ",y:" << center.second << ",r:" << node.radius << ",dashed:" << (node.dashed ? "true" : "false") << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Morph) {
            std::ostringstream item; item << "{type:'morph',from:'" << node.first << "',to:'" << node.second
                << "',fromSize:" << node.x << ",toSize:" << node.y << ",duration:" << node.duration << ",start:" << node.start_time << ",repeat:" << node.repeat << ",yoyo:" << (node.yoyo ? "true" : "false") << ",easing:" << std::quoted(node.easing) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Sphere) {
            std::ostringstream item; item << "{type:'sphere',x:" << node.x << ",y:" << node.y << ",z:" << node.z << ",r:" << node.radius << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Cube) {
            std::ostringstream item; item << "{type:'cube',x:" << node.x << ",y:" << node.y << ",z:" << node.z << ",s:" << node.size << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Parametric4D) {
            std::ostringstream item;
            item << "{type:'param4d',expr:["
                 << std::quoted(js_expression(node.first, "u")) << ","
                 << std::quoted(js_expression(node.second, "u")) << ","
                 << std::quoted(js_expression(node.third, "u")) << ","
                 << std::quoted(js_expression(node.fourth, "u"))
                 << "],title:" << std::quoted(node.title)
                 << ",color:" << std::quoted(node.color)
                 << ",xmin:" << node.xmin << ",xmax:" << node.xmax
                 << ",trace:" << (node.animate_trace ? "true" : "false") << ",duration:" << node.duration << ",start:" << node.start_time
                 << ",repeat:" << node.repeat << ",yoyo:" << (node.yoyo ? "true" : "false") << ",easing:" << std::quoted(node.easing) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Plot4D) {
            animations.push_back("{type:'4d'}");
        } else if (node.kind == SceneNode::Kind::Data2D) {
            std::ostringstream item; item << "{type:'data2d',points:[";
            for (size_t index = 0; index < node.samples.size(); ++index) { if (index) item << ","; item << "[" << node.samples[index].first << "," << node.samples[index].second << "]"; }
            item << "],title:" << std::quoted(node.title) << ",color:" << std::quoted(node.color)
                 << ",trace:" << (node.animate_trace ? "true" : "false") << ",duration:" << node.duration << ",start:" << node.start_time
                 << ",repeat:" << node.repeat << ",yoyo:" << (node.yoyo ? "true" : "false") << ",easing:" << std::quoted(node.easing) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Data3D) {
            std::ostringstream item; item << "{type:'data3d',grid:[";
            for (size_t row = 0; row < node.grid.size(); ++row) { if (row) item << ","; item << "["; for (size_t column = 0; column < node.grid[row].size(); ++column) { if (column) item << ","; item << node.grid[row][column]; } item << "]"; }
            item << "],title:" << std::quoted(node.title) << ",color:" << std::quoted(node.color)
                 << ",trace:" << (node.animate_trace ? "true" : "false") << ",duration:" << node.duration << ",start:" << node.start_time
                 << ",repeat:" << node.repeat << ",yoyo:" << (node.yoyo ? "true" : "false") << ",easing:" << std::quoted(node.easing) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::VectorField2D) {
            std::ostringstream item; item << "{type:'vector2d',exprx:" << std::quoted(js_expression(node.first, "u", "v")) << ",expry:" << std::quoted(js_expression(node.second, "u", "v")) << ",title:" << std::quoted(node.title) << ",color:" << std::quoted(node.color) << ",xmin:" << node.xmin << ",xmax:" << node.xmax << ",ymin:" << node.ymin << ",ymax:" << node.ymax << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Parametric2D) {
            std::ostringstream item;
            item << "{type:'param2d',exprx:" << std::quoted(js_expression(node.first, "u"))
                 << ",expry:" << std::quoted(js_expression(node.second, "u"))
                 << ",title:" << std::quoted(node.title)
                 << ",color:" << std::quoted(node.color)
                 << ",xmin:" << node.xmin << ",xmax:" << node.xmax
                 << ",ymin:" << node.ymin << ",ymax:" << node.ymax
                 << ",trace:" << (node.animate_trace ? "true" : "false") << ",duration:" << node.duration << ",start:" << node.start_time
                 << ",repeat:" << node.repeat << ",yoyo:" << (node.yoyo ? "true" : "false") << ",easing:" << std::quoted(node.easing) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Implicit2D) {
            std::ostringstream item;
            item << "{type:'implicit2d',expr:" << std::quoted(js_expression(node.expression, "u", "v"))
                 << ",title:" << std::quoted(node.title)
                 << ",color:" << std::quoted(node.color)
                 << ",xmin:" << node.xmin << ",xmax:" << node.xmax << ",ymin:" << node.ymin << ",ymax:" << node.ymax << "}";
            animations.push_back(item.str());
        } else if (node.kind != SceneNode::Kind::Plot2D && node.kind != SceneNode::Kind::Plot3D) {
            continue;
        } else {
            const std::string expression = node.kind == SceneNode::Kind::Plot2D ? js_expression(node.expression, "u") : js_expression(node.expression, "u", "v");
            const std::string type = node.kind == SceneNode::Kind::Plot2D ? "2d" : "3d";
            std::ostringstream item;
            item << "{type:'" << type << "',expr:" << std::quoted(expression)
                 << ",title:" << std::quoted(node.title)
                 << ",color:" << std::quoted(node.color)
                 << ",xmin:" << node.xmin << ",xmax:" << node.xmax << ",ymin:" << node.ymin << ",ymax:" << node.ymax
                 << ",duration:" << node.duration << ",start:" << node.start_time << ",repeat:" << node.repeat << ",yoyo:" << (node.yoyo ? "true" : "false") << ",easing:" << std::quoted(node.easing)
                 << ",trace:" << (node.animate_trace ? "true" : "false") << "}";
            animations.push_back(item.str());
        }
    }
    std::string data = "[";
    for (size_t i = 0; i < animations.size(); ++i) { if (i) data += ","; data += animations[i]; }
    data += "]";
    std::stringstream titles;
    for (const auto& node : nodes) if (!node.title.empty()) titles << "<span class=\"plot-title\">" << html_escape(node.title) << "</span>";
    titles << "<script>const clamp=(v,l,h)=>Math.min(h,Math.max(l,v));</script>";
    return "<!doctype html><meta charset=\"utf-8\"><title>Marila scene</title>"
           "<style>body{margin:0;background:#040711;color:white;font:16px sans-serif;text-align:center}"
           "canvas{max-width:95vw;border:1px solid #334155}.plot-title{display:inline-block;margin:8px;padding:6px 10px;border:1px solid #475569;border-radius:6px}.controls{display:flex;gap:8px;justify-content:center;align-items:center;margin:12px}.controls button{background:#1e293b;color:white;border:1px solid #475569;border-radius:5px;padding:6px 12px}.controls input{width:min(520px,60vw)}</style><h2>Marila native scene</h2>" + titles.str() +
           "<div class=\"controls\"><button id=\"play\">Pause</button><button id=\"reset\">Reset</button><input id=\"timeline\" type=\"range\" min=\"0\" max=\"1\" step=\"0.001\" value=\"0\" aria-label=\"Animation timeline\"></div><canvas id=\"scene\" width=\"900\" height=\"500\"></canvas><script>"
           "const c=document.getElementById('scene'),x=c.getContext('2d'),a=" + data + ";let t=0,playing=true,yaw=.6,pitch=.4,drag=false,lx=0,ly=0;const timeline=document.getElementById('timeline'),play=document.getElementById('play'),reset=document.getElementById('reset'),maxDuration=Math.max(1,...a.map(q=>(q.start||0)+(q.duration||0)*(q.repeat||1)));const ease=(p,m)=>m==='in'?p*p:m==='out'?1-(1-p)*(1-p):m==='in-out'?p<.5?2*p*p:1-Math.pow(-2*p+2,2)/2:p;const progressFor=q=>{let local=(t-(q.start||0))/Math.max(1e-9,q.duration),cycles=q.repeat||1;if(local<=0)return 0;if(local>=cycles)return q.yoyo&&cycles%2===0?0:1;let cycle=Math.floor(local),p=local-cycle;return ease(q.yoyo&&cycle%2?1-p:p,q.easing)};const positionFor=id=>{let p=[0,0];for(const q of a){if(q.type==='point'&&q.id===id)p=[q.x,q.y];if(q.type==='move'&&q.id===id&&t>=(q.start||0)){let z=progressFor(q);p=[q.x+(q.tx-q.x)*z,q.y+(q.ty-q.y)*z]}}return p};const opacityFor=id=>{let value=1;for(const q of a)if(q.type==='fade'&&q.target===id&&t>=(q.start||0)){let p=progressFor(q);value=q.fadeIn?p:1-p}return value};play.onclick=()=>{playing=!playing;play.textContent=playing?'Pause':'Play'};reset.onclick=()=>{t=0;timeline.value=0};timeline.oninput=()=>{t=Number(timeline.value)*maxDuration};c.onpointerdown=e=>{drag=true;lx=e.clientX;ly=e.clientY};c.onpointerup=()=>drag=false;c.onpointerleave=()=>drag=false;c.onpointermove=e=>{if(drag){yaw+=(e.clientX-lx)*.01;pitch+=(e.clientY-ly)*.01;pitch=Math.max(-1.4,Math.min(1.4,pitch));lx=e.clientX;ly=e.clientY}};"
           "function frame(){x.fillStyle='#040711';x.fillRect(0,0,c.width,c.height);"
           "x.strokeStyle='rgba(255,255,255,.08)';for(let i=0;i<900;i+=40){x.beginPath();x.moveTo(i,0);x.lineTo(i,500);x.stroke()}"
           "for(let i=0;i<500;i+=40){x.beginPath();x.moveTo(0,i);x.lineTo(900,i);x.stroke()}"
           "const cx=450,cy=250;function proj(xv,yv,zv){let cyaw=Math.cos(yaw),syaw=Math.sin(yaw),cp=Math.cos(pitch),sp=Math.sin(pitch),rx=xv*cyaw-zv*syaw,rz=xv*syaw+zv*cyaw,ry=yv*cp-rz*sp,rz2=yv*sp+rz*cp,s=1/(1+rz2/10);return[cx+(rx-ry)*28*s,cy+(rx+ry)*14*s-rz2*20*s]}function point2(xv,yv){return[cx+xv*2,cy-yv*2]}for(const q of a){if(q.type==='point'){x.globalAlpha=opacityFor(q.id);let p=positionFor(q.id),pp=point2(p[0],p[1]);x.fillStyle='#22d3ee';x.beginPath();x.arc(pp[0],pp[1],5,0,Math.PI*2);x.fill();x.fillStyle='white';x.fillText(q.label,pp[0]+8,pp[1]-8);x.globalAlpha=1}"
           "else if(q.type==='move'){if(a.some(z=>z.type==='move'&&z.id===q.id&&(z.start||0)>(q.start||0)))continue;let p=positionFor(q.id),pp=point2(p[0],p[1]);x.globalAlpha=opacityFor(q.id);x.fillStyle='#f59e0b';x.beginPath();x.arc(pp[0],pp[1],6,0,Math.PI*2);x.fill();x.fillStyle='white';x.fillText(q.label,pp[0]+8,pp[1]-8);x.globalAlpha=1}"
           "else if(q.type==='line'){let p=point2(q.x1,q.y1),r=point2(q.x2,q.y2);x.strokeStyle='#6366f1';x.lineWidth=3;x.beginPath();x.moveTo(...p);x.lineTo(...r);x.stroke()}"
           "else if(q.type==='triangle'){let p=point2(q.x1,q.y1),r=point2(q.x2,q.y2),s=point2(q.x3,q.y3);x.strokeStyle=q.color||'#f59e0b';x.fillStyle=(q.color||'#f59e0b')+'33';x.beginPath();x.moveTo(...p);x.lineTo(...r);x.lineTo(...s);x.closePath();x.fill();x.stroke()}"
           "else if(q.type==='circle'){let p=point2(q.x,q.y);x.strokeStyle='#22d3ee';x.setLineDash(q.dashed?[6,4]:[]);x.beginPath();x.arc(p[0],p[1],q.r*2,0,Math.PI*2);x.stroke();x.setLineDash([])}"
           "else if(q.type==='param4d'){let fs=q.expr.map(e=>new Function('u','return '+e)),count=800,limit=Math.floor(count*(q.trace?progressFor(q):1)),angle=yaw,ca=Math.cos(angle),sa=Math.sin(angle);x.strokeStyle=q.color||'#c084fc';x.lineWidth=2.2;x.beginPath();let started=false;for(let i=0;i<=limit;i++){let u0=q.xmin+(q.xmax-q.xmin)*i/count,v=fs.map(f=>Number(f(u0)));if(!v.every(Number.isFinite)){started=false;continue}let [vx,vy,vz,vw]=v,xx=vx*ca-vw*sa,ww=vx*sa+vw*ca,yy=vy*Math.cos(pitch)-vz*Math.sin(pitch),zz=vy*Math.sin(pitch)+vz*Math.cos(pitch),persp=4/Math.max(.4,4-ww),p=proj(xx*persp,yy*persp,zz*persp);if(!started){x.moveTo(...p);started=true}else x.lineTo(...p)}x.stroke();x.lineWidth=1}"
           "else if(q.type==='4d'){x.strokeStyle='#a855f7';for(let i=0;i<16;i++)for(let b of[1,2,4,8]){let j=i^b;if(i<j){let p=u(i),r=u(j);x.beginPath();x.moveTo(p[0],p[1]);x.lineTo(r[0],r[1]);x.stroke()}}}"
           "else if(q.type==='data3d'){let rows=q.grid.length,cols=rows?q.grid[0].length:0,fraction=q.trace?progressFor(q):1,rendRows=Math.ceil(rows*fraction),rendCols=Math.ceil(cols*fraction);x.strokeStyle=q.color||'#a855f7';for(let r=0;r<rendRows;r++){x.beginPath();for(let j=0;j<rendCols;j++){let p=proj(j-(cols-1)/2,(rows-1)/2-r,q.grid[r][j]);if(j===0)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}for(let j=0;j<rendCols;j++){x.beginPath();for(let r=0;r<rendRows;r++){let p=proj(j-(cols-1)/2,(rows-1)/2-r,q.grid[r][j]);if(r===0)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}}"
           "else if(q.type==='3d'){let f=new Function('u','v','return '+q.expr),sx=300/(q.xmax-q.xmin),sy=300/(q.ymax-q.ymin),mx=(q.xmin+q.xmax)/2,my=(q.ymin+q.ymax)/2,fraction=q.trace?progressFor(q):1,xend=q.xmin+(q.xmax-q.xmin)*fraction,yend=q.ymin+(q.ymax-q.ymin)*fraction;x.strokeStyle=q.color||'#10b981';for(let i=q.xmin;i<=xend;i+=(q.xmax-q.xmin)/25){x.beginPath();for(let j=q.ymin;j<=yend;j+=(q.ymax-q.ymin)/25){let z=Number(f(i,j));if(!Number.isFinite(z))continue;let p=proj((i-mx)*sx/28,(j-my)*sy/28,z);if(j===q.ymin)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}}"
           "else if(q.type==='data2d'){let limit=q.trace?Math.floor(q.points.length*progressFor(q)):q.points.length;x.strokeStyle=q.color||'#f59e0b';x.beginPath();for(let i=0;i<limit;i++){let p=point2(q.points[i][0],q.points[i][1]);if(i===0)x.moveTo(...p);else x.lineTo(...p)}x.stroke();for(let i=0;i<limit;i++){let p=q.points[i],v=point2(p[0],p[1]);x.fillStyle=q.color||'#f59e0b';x.beginPath();x.arc(v[0],v[1],4,0,Math.PI*2);x.fill()}}"
           "else if(q.type==='vector2d'){let fx=new Function('u','v','return '+q.exprx),fy=new Function('u','v','return '+q.expry),sx=780/(q.xmax-q.xmin),sy=220/(q.ymax-q.ymin),mx=(q.xmin+q.xmax)/2,my=(q.ymin+q.ymax)/2,px=v=>cx+(v-mx)*sx,py=v=>cy-(v-my)*sy;x.strokeStyle=q.color||'#22d3ee';for(let i=0;i<=10;i++)for(let j=0;j<=10;j++){let u=q.xmin+(q.xmax-q.xmin)*j/10,v=q.ymin+(q.ymax-q.ymin)*i/10,dx=Number(fx(u,v)),dy=Number(fy(u,v));if(!Number.isFinite(dx)||!Number.isFinite(dy))continue;let p=point2(u,v),r=point2(u+dx*.18,v+dy*.18);x.beginPath();x.moveTo(...p);x.lineTo(...r);x.stroke()}}"
           "else if(q.type==='2d'){let f=new Function('u','return '+q.expr),sx=780/(q.xmax-q.xmin),sy=220/(q.ymax-q.ymin),mx=(q.xmin+q.xmax)/2,my=(q.ymin+q.ymax)/2,px=v=>cx+(v-mx)*sx,py=v=>cy-(v-my)*sy;x.strokeStyle='rgba(255,255,255,.35)';if(q.xmin<=0&&q.xmax>=0){x.beginPath();x.moveTo(px(0),0);x.lineTo(px(0),500);x.stroke()}if(q.ymin<=0&&q.ymax>=0){x.beginPath();x.moveTo(0,py(0));x.lineTo(900,py(0));x.stroke()}x.strokeStyle=q.color||'#06b6d4';x.beginPath();let first=true,progress=q.trace?progressFor(q):1,end=q.xmin+(q.xmax-q.xmin)*progress;for(let i=q.xmin;i<=end;i+=(q.xmax-q.xmin)/400){let y=Number(f(i));if(!Number.isFinite(y)){first=true;continue}let p=[px(i),py(y)];if(first){x.moveTo(...p);first=false}else x.lineTo(...p)}x.stroke()}"
           "else if(q.type==='param2d'){let fx=new Function('u','return '+q.exprx),fy=new Function('u','return '+q.expry),sx=780/(q.xmax-q.xmin),sy=220/(q.ymax-q.ymin),px=v=>cx+(v+6)*55,py=v=>cy-v*sy,end=q.xmin+(q.xmax-q.xmin)*(q.trace?progressFor(q):1);x.strokeStyle=q.color||'#f59e0b';x.beginPath();for(let i=q.xmin;i<=end;i+=(q.xmax-q.xmin)/400){let pxv=Number(fx(i)),pyv=Number(fy(i));if(!Number.isFinite(pxv)||!Number.isFinite(pyv))continue;let p=[pxv*55+cx,py(pyv)];if(i===q.xmin)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}"
           "else if(q.type==='implicit2d'){let f=new Function('u','v','return '+q.expr),sx=780/(q.xmax-q.xmin),sy=220/(q.ymax-q.ymin),mx=(q.xmin+q.xmax)/2,my=(q.ymin+q.ymax)/2;x.fillStyle=q.color||'#fb7185';for(let i=q.xmin;i<=q.xmax;i+=(q.xmax-q.xmin)/180)for(let j=q.ymin;j<=q.ymax;j+=(q.ymax-q.ymin)/180){let value=Number(f(i,j));if(Number.isFinite(value)&&Math.abs(value)<.08){let px=cx+(i-mx)*sx,py=cy-(j-my)*sy;x.fillRect(px,py,3,3)}}}"
           "else if(q.type==='morph'){let p=progressFor(q),s=q.fromSize+(q.toSize-q.fromSize)*p;x.strokeStyle='#f59e0b';x.lineWidth=4;if(q.from==='square'&&q.to==='circle'){x.beginPath();x.roundRect(cx-s/2,cy-s/2,s,s,s*p);x.stroke()}else if(q.from==='circle'&&q.to==='square'){x.beginPath();x.roundRect(cx-s/2,cy-s/2,s,s,s*(1-p));x.stroke()}else if(q.to==='circle'){x.beginPath();x.arc(cx,cy,s/2,0,Math.PI*2);x.stroke()}else{x.strokeRect(cx-s/2,cy-s/2,s,s)}}"
           "else if(q.type==='sphere'){let p=proj(q.x,q.y,q.z),r=Math.max(2,q.r*18/(q.z+8));x.strokeStyle='#38bdf8';x.beginPath();x.arc(p[0],p[1],r,0,Math.PI*2);x.stroke()}"
           "else if(q.type==='cube'){let s=q.s/2,pts=[[-s,-s,-s],[s,-s,-s],[s,s,-s],[-s,s,-s],[-s,-s,s],[s,-s,s],[s,s,s],[-s,s,s]].map(v=>proj(q.x+v[0],q.y+v[1],q.z+v[2]));x.strokeStyle='#f97316';for(let e of[[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]]){x.beginPath();x.moveTo(...pts[e[0]]);x.lineTo(...pts[e[1]]);x.stroke()}}}if(playing)t+=.02;if(t>maxDuration)t=0;timeline.value=t/maxDuration;requestAnimationFrame(frame)}"
           "function u(i){let p=[i&1?1:-1,i&2?1:-1,i&4?1:-1,i&8?1:-1],s=1/(4.5-p[3]);return[cx+p[0]*80*s,cy-p[1]*80*s]}frame();</script>";
}

}  // namespace

std::string render_markdown(const std::string& source) {
    return render_markdown_impl(source);
}

std::string sheet_json(const std::string& source) {
    auto quote = [](const std::string& value) {
        std::string result = "\"";
        for (unsigned char character : value) {
            if (character == '\\') result += "\\\\";
            else if (character == '"') result += "\\\"";
            else if (character == '\n') result += "\\n";
            else if (character == '\r') result += "\\r";
            else if (character == '\t') result += "\\t";
            else result += static_cast<char>(character);
        }
        return result + "\"";
    };
    std::vector<std::pair<std::string, std::string>> cells;
    struct Chart { std::string type, range, title; };
    std::vector<Chart> charts;
    std::regex assignment(R"(^\s*([A-Z]+[0-9]+)\s*=\s*(.*?)\s*$)");
    std::regex chart(R"MARILA(^\s*chart\s+(bar|line|scatter)\s+from\s+([A-Z]+[0-9]+:[A-Z]+[0-9]+)(?:\s+title\s+"([^"]*)")?\s*$)MARILA");
    std::stringstream lines(source); std::string line; std::smatch match;
    while (std::getline(lines, line)) {
        if (std::regex_match(line, match, assignment)) cells.emplace_back(match[1].str(), match[2].str());
        else if (std::regex_match(line, match, chart)) charts.push_back({match[1].str(), match[2].str(), match[3].str()});
    }
    std::string result = "{\"kind\":\"sheet\",\"cells\":[";
    for (size_t index = 0; index < cells.size(); ++index) {
        if (index) result += ',';
        result += "{\"reference\":" + quote(cells[index].first) + ",\"formula\":" + quote(cells[index].second) + "}";
    }
    result += "],\"charts\":[";
    for (size_t index = 0; index < charts.size(); ++index) {
        if (index) result += ',';
        result += "{\"type\":" + quote(charts[index].type) + ",\"range\":" + quote(charts[index].range) + ",\"title\":" + quote(charts[index].title) + "}";
    }
    return result + "]}\n";
}

std::string sheet_delimited(const std::string& source, char separator) {
    std::map<std::string, std::string> cells;
    const std::regex assignment(R"(^\s*([A-Z]+)([0-9]+)\s*=\s*(.*?)\s*$)");
    std::stringstream lines(source); std::string line; std::smatch match;
    int max_column = 0, max_row = 0;
    while (std::getline(lines, line)) if (std::regex_match(line, match, assignment)) {
        const int column = spreadsheet_column(match[1].str());
        const int row = std::stoi(match[2].str());
        cells[match[1].str() + match[2].str()] = match[3].str();
        max_column = std::max(max_column, column); max_row = std::max(max_row, row);
    }
    auto column_name = [](int column) {
        std::string result; while (column) { result.insert(result.begin(), static_cast<char>('A' + (column - 1) % 26)); column = (column - 1) / 26; } return result;
    };
    auto csv_field = [separator](const std::string& value) {
        const bool quote = value.find(separator) != std::string::npos || value.find('"') != std::string::npos || value.find('\n') != std::string::npos || value.find('\r') != std::string::npos;
        if (!quote) return value;
        std::string escaped = "\""; for (char character : value) { if (character == '"') escaped += "\"\""; else escaped += character; } return escaped + "\"";
    };
    std::ostringstream output;
    for (int row = 1; row <= max_row; ++row) {
        for (int column = 1; column <= max_column; ++column) {
            if (column > 1) output << separator;
            const auto found = cells.find(column_name(column) + std::to_string(row));
            if (found != cells.end()) {
                std::string value = found->second;
                const size_t first = value.find_first_not_of(" \t");
                if (first != std::string::npos) value.erase(0, first);
                if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
                    const char quote = value.front(); value = value.substr(1, value.size() - 2); std::string unquoted;
                    for (size_t index = 0; index < value.size(); ++index) { if (value[index] == quote && index + 1 < value.size() && value[index + 1] == quote) ++index; unquoted += value[index]; }
                    value = std::move(unquoted);
                } else if (!value.empty() && !std::regex_match(value, std::regex(R"([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)"))) value = "=" + value;
                output << csv_field(value);
            }
        }
        output << '\n';
    }
    return output.str();
}

std::string text_source_json(const std::string& type, const std::string& source) {
    auto quote = [](const std::string& value) {
        std::string result = "\"";
        for (unsigned char character : value) {
            if (character == '\\') result += "\\\\";
            else if (character == '"') result += "\\\"";
            else if (character == '\n') result += "\\n";
            else if (character == '\r') result += "\\r";
            else if (character == '\t') result += "\\t";
            else result += static_cast<char>(character);
        }
        return result + "\"";
    };
    std::string result = "{\"kind\":\"text-program\",\"type\":" + quote(type) + ",\"lines\":[";
    std::stringstream lines(source); std::string line; size_t line_number = 0; bool first = true;
    while (std::getline(lines, line)) {
        ++line_number;
        if (!first) result += ',';
        first = false;
        result += "{\"line\":" + std::to_string(line_number) + ",\"text\":" + quote(line) + "}";
    }
    return result + "]}\n";
}

std::string detect_file_type(const std::string& source) {
    std::smatch match;
    if (std::regex_search(source, match, std::regex(R"(file\s+type\s+([A-Za-z]+))"))) return match[1];
    return "program";
}

std::vector<std::string> validate_visual_source(const std::string& source) {
    std::vector<std::string> diagnostics;
    const std::string type = detect_file_type(source);
    if (type != "image" && type != "scene" && type != "video") return diagnostics;

    auto trim = [](const std::string& value) {
        const size_t first = value.find_first_not_of(" \t\r");
        const size_t last = value.find_last_not_of(" \t\r");
        return first == std::string::npos ? std::string() : value.substr(first, last - first + 1);
    };
    auto report = [&diagnostics](size_t line, size_t column, const std::string& message) {
        diagnostics.push_back("line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + message);
    };
    auto starts_with = [](const std::string& value, const std::string& prefix) {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    };

    const std::string numeric = R"([-+]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)";
    const std::regex point(std::string(R"(^point\s+([A-Za-z_]\w*)\s+at\s*\(\s*)") + numeric + R"(\s*,\s*)" + numeric + R"(\s*\)(?:\s+label\s+"[^"]*")?(?:\s+moves\s+to\s*\(\s*)" + numeric + R"(\s*,\s*)" + numeric + R"(\s*\)(?:\s+over\s+)" + numeric + R"(s)?(?:\s+ease\s+(?:linear|in-out|in|out))?(?:\s+at\s+)" + numeric + R"(s)?(?:\s+repeat\s+[1-9][0-9]*(?:\s+yoyo)?)?)?$)");
    const std::regex line_ref(R"(^line\s+from\s+([A-Za-z_]\w*)\s+to\s+([A-Za-z_]\w*)(?:\s+.*)?$)");
    const std::regex triangle_ref(R"(^triangle\s+([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s+([A-Za-z_]\w*)(?:\s+.*)?$)");
    const std::regex compact_triangle(R"(^triangle\s+([A-Za-z_])([A-Za-z_])([A-Za-z_])(?:\s+.*)?$)");
    const std::regex circle_ref(std::string(R"(^circle(?:\s+at)?\s+([A-Za-z_]\w*)\s+radius\s+)") + numeric + R"((?:\s+.*)?$)");
    const std::regex fade_ref(std::string("^fade\\s+(?:in|out)\\s+([A-Za-z_]\\w*)(?:\\s+over\\s+") + numeric + "s)?(?:\\s+ease\\s+(?:linear|in-out|in|out))?(?:\\s+at\\s+" + numeric + "s)?(?:\\s+repeat\\s+[1-9][0-9]*(?:\\s+yoyo)?)?$");
    const std::regex wait(std::string(R"(^wait\s+)") + numeric + R"(s(?:\s+at\s+)" + numeric + R"(s)?$)");
    const std::regex morph(std::string("^(?:square|circle)\\s*\\(\\s*size\\s+") + numeric + "\\s*\\)\\s+morphs\\s+to\\s+(?:square|circle)\\s*\\(\\s*size\\s+" + numeric + "\\s*\\)(?:\\s+over\\s+" + numeric + "s)?(?:\\s+ease\\s+(?:linear|in-out|in|out))?(?:\\s+at\\s+" + numeric + "s)?(?:\\s+repeat\\s+[1-9][0-9]*(?:\\s+yoyo)?)?$");
    const std::regex plot(R"(^plot(?:2d|3d|4d)?\b.+$)");
    const std::set<std::string> plot_functions = {
        "abs", "acos", "acosh", "asin", "asinh", "atan", "atan2", "atanh", "ceil", "clamp", "cos", "cosh", "exp",
        "data", "floor", "hypot", "implicit", "log", "log10", "log2", "max", "min", "parametric", "polar",
        "pow", "sign", "sin", "sinh", "sqrt", "tan", "tanh", "vectorfield"
    };

    std::set<std::string> point_ids;
    std::vector<std::pair<size_t, std::string>> references;
    std::stringstream input(strip_visual_comments(source));
    std::string line_text;
    size_t line_number = 0;
    size_t visual_statement_count = 0;
    while (std::getline(input, line_text)) {
        ++line_number;
        const size_t first = line_text.find_first_not_of(" \t");
        const size_t column = first == std::string::npos ? 1 : first + 1;
        const std::string statement = trim(line_text);
        if (statement.empty() || statement == "[" || statement == "]" || starts_with(statement, "file type ") ||
            starts_with(statement, "render ") || starts_with(statement, "%") || starts_with(statement, "//")) continue;

        std::smatch match;
        if (std::regex_match(statement, match, point)) {
            const bool is_motion = statement.find(" moves to ") != std::string::npos;
            if (!point_ids.insert(match[1]).second && !is_motion) report(line_number, column, "duplicate point '" + match[1].str() + "'");
            ++visual_statement_count; continue;
        }
        if (std::regex_match(statement, match, line_ref)) {
            references.emplace_back(line_number, match[1]); references.emplace_back(line_number, match[2]); ++visual_statement_count; continue;
        }
        if (std::regex_match(statement, match, compact_triangle)) {
            references.emplace_back(line_number, match[1]); references.emplace_back(line_number, match[2]); references.emplace_back(line_number, match[3]); ++visual_statement_count; continue;
        }
        if (std::regex_match(statement, match, triangle_ref)) {
            references.emplace_back(line_number, match[1]); references.emplace_back(line_number, match[2]); references.emplace_back(line_number, match[3]); ++visual_statement_count; continue;
        }
        if (std::regex_match(statement, match, circle_ref) || std::regex_match(statement, match, fade_ref)) {
            references.emplace_back(line_number, match[1]); ++visual_statement_count; continue;
        }
        if (std::regex_match(statement, wait) || std::regex_match(statement, morph) || std::regex_match(statement, plot) ||
            starts_with(statement, "sphere ") || starts_with(statement, "cube ")) {
            if (starts_with(statement, "plot")) {
                std::string expression = statement;
                for (size_t index = 0; index < expression.size(); ++index) {
                    if (expression[index] != '\"') continue;
                    const size_t end_quote = expression.find('\"', index + 1);
                    if (end_quote == std::string::npos) break;
                    std::fill(expression.begin() + static_cast<std::ptrdiff_t>(index), expression.begin() + static_cast<std::ptrdiff_t>(end_quote + 1), ' ');
                    index = end_quote;
                }
                const std::regex function_call(R"(([A-Za-z_]\w*)\s*\()");
                for (std::sregex_iterator function_it(expression.begin(), expression.end(), function_call), function_end;
                     function_it != function_end; ++function_it) {
                    const std::string function = (*function_it)[1].str();
                    if (!plot_functions.count(function)) report(line_number, column, "unsupported plot function '" + function + "'");
                }
                const size_t expression_start = statement.find(' ');
                std::string plot_expression = expression_start == std::string::npos ? std::string() : statement.substr(expression_start + 1);
                const size_t over = plot_expression.find(" over ");
                if (over != std::string::npos) plot_expression.resize(over);
                const size_t modifier = plot_expression.find(" & ");
                if (modifier != std::string::npos) plot_expression.resize(modifier);
                for (const std::string& suffix : {std::string(" title "), std::string(" color "), std::string(" animate ")}) {
                    const size_t suffix_position = plot_expression.find(suffix);
                    if (suffix_position != std::string::npos) plot_expression.resize(suffix_position);
                }
                const std::set<std::string> plot_names = {
                    "data", "implicit", "in", "parametric", "polar", "tesseract", "trace", "vectorfield",
                    "x", "y", "t", "pi", "e"
                };
                const std::regex identifier(R"(\b([A-Za-z_]\w*)\b)");
                for (std::sregex_iterator name_it(plot_expression.begin(), plot_expression.end(), identifier), name_end;
                     name_it != name_end; ++name_it) {
                    const std::string name = (*name_it)[1].str();
                    if (!plot_functions.count(name) && !plot_names.count(name))
                        report(line_number, column, "unknown plot name '" + name + "'");
                }
            }
            ++visual_statement_count; continue;
        }
        report(line_number, column, "unknown visual statement");
    }
    for (const auto& reference : references) {
        if (!point_ids.count(reference.second)) report(reference.first, 1, "unknown point '" + reference.second + "'");
    }
    if (visual_statement_count == 0) report(1, 1, "renderer source contains no visual statements");
    try {
        const auto nodes = parse_visual_source(source);
        for (const auto& node : nodes) {
            if (node.duration <= 0) report(1, 1, "animation duration must be positive");
            if (node.start_time < 0) report(1, 1, "animation start time cannot be negative");
            if (node.repeat < 1) report(1, 1, "animation repeat count must be positive");
        }
    } catch (const std::exception& error) {
        report(1, 1, error.what());
    }
    return diagnostics;
}

void export_obj(const std::string& source, const std::string& output) {
    const auto diagnostics = validate_visual_source(source);
    if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front());
    const auto nodes = parse_visual_source(source);
    std::ofstream file(output);
    if (!file) throw std::runtime_error("unable to write OBJ output '" + output + "'");
    file << "# Marila Wavefront OBJ export\n";
    int next_vertex = 1;
    auto vertex = [&](double x, double y, double z) {
        file << "v " << x << ' ' << y << ' ' << z << '\n';
        return next_vertex++;
    };
    auto line = [&](const std::vector<int>& indices) {
        if (indices.size() >= 2) { file << "l"; for (const int index : indices) file << ' ' << index; file << '\n'; }
    };
    auto face = [&](const std::vector<int>& indices) {
        if (indices.size() >= 3) { file << "f"; for (const int index : indices) file << ' ' << index; file << '\n'; }
    };
    auto project_4d = [](double x, double y, double z, double w) {
        const double angle = .5; const double rotated_x = x * std::cos(angle) - w * std::sin(angle); const double rotated_z = x * std::sin(angle) + w * std::cos(angle);
        return std::array<double, 3>{rotated_x, y, z + rotated_z * .15};
    };
    struct Expression {
        std::string text; size_t position{0}; double first{0}; double second{0}; double third{0};
        void skip() { while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) ++position; }
        bool take(char character) { skip(); if (position < text.size() && text[position] == character) { ++position; return true; } return false; }
        double primary() {
            skip();
            if (take('(')) { double value = add(); take(')'); return value; }
            if (position < text.size() && (std::isdigit(static_cast<unsigned char>(text[position])) || text[position] == '.')) {
                const char* start = text.c_str() + position; char* end = nullptr; const double value = std::strtod(start, &end); position += static_cast<size_t>(end - start); return value;
            }
            if (position < text.size() && (std::isalpha(static_cast<unsigned char>(text[position])) || text[position] == '_')) {
                size_t end = position; while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_')) ++end;
                const std::string name = text.substr(position, end - position); position = end; skip();
                if (take('(')) { const double a = add(); const bool comma = take(','); const double b = comma ? add() : 0; const double c = comma && take(',') ? add() : 0; take(')');
                    if (name == "sin") return std::sin(a); if (name == "cos") return std::cos(a); if (name == "tan") return std::tan(a); if (name == "sinh") return std::sinh(a); if (name == "cosh") return std::cosh(a); if (name == "tanh") return std::tanh(a); if (name == "asinh") return std::asinh(a); if (name == "acosh") return std::acosh(a); if (name == "atanh") return std::atanh(a);
                    if (name == "asin") return std::asin(a); if (name == "acos") return std::acos(a); if (name == "atan") return std::atan(a);
                    if (name == "sqrt") return std::sqrt(a); if (name == "exp") return std::exp(a); if (name == "log") return std::log(a); if (name == "log2") return std::log2(a); if (name == "log10") return std::log10(a);
                    if (name == "abs") return std::abs(a); if (name == "floor") return std::floor(a); if (name == "ceil") return std::ceil(a); if (name == "min") return std::min(a, b); if (name == "max") return std::max(a, b); if (name == "clamp") return std::max(b, std::min(c, a)); if (name == "hypot") return std::hypot(a, b); if (name == "sign") return a > 0 ? 1.0 : (a < 0 ? -1.0 : 0.0); if (name == "pow") return std::pow(a, b); if (name == "atan2") return std::atan2(a, b);
                    return 0;
                }
                if (name == "x" || name == "t") return first; if (name == "y") return second;
                if (name == "pi") return 3.14159265358979323846; if (name == "e") return 2.71828182845904523536;
            }
            return 0;
        }
        double unary() { skip(); if (take('+')) return unary(); if (take('-')) return -unary(); return primary(); }
        double power() { const double value = unary(); return take('^') ? std::pow(value, power()) : value; }
        double multiply() { double value = power(); while (true) { if (take('*')) value *= power(); else if (take('/')) value /= power(); else return value; } }
        double add() { double value = multiply(); while (true) { if (take('+')) value += multiply(); else if (take('-')) value -= multiply(); else return value; } }
        double evaluate(double x) { first = x; second = 0; position = 0; return add(); }
        double evaluate2(double x, double y) { first = x; second = y; position = 0; return add(); }
    };
    auto add_planar_plot = [&](const SceneNode& node) {
        Expression x_expression{node.kind == SceneNode::Kind::Plot2D ? "x" : node.first};
        Expression y_expression{node.kind == SceneNode::Kind::Plot2D ? node.expression : node.second};
        std::vector<int> indices;
        for (int sample = 0; sample <= 256; ++sample) {
            const double t = node.xmin + (node.xmax - node.xmin) * sample / 256.0;
            const double x = node.kind == SceneNode::Kind::Plot2D ? t : x_expression.evaluate(t);
            const double y = y_expression.evaluate(t);
            if (std::isfinite(x) && std::isfinite(y)) indices.push_back(vertex(x, y, 0));
        }
        line(indices);
    };
    std::map<std::string, std::pair<double, double>> points;
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Point) points[node.id] = {node.x, node.y};
    for (const auto& node : nodes) {
        if (node.kind == SceneNode::Kind::Point) file << "p " << vertex(node.x, node.y, 0) << '\n';
        else if (node.kind == SceneNode::Kind::Line) {
            // Named geometry is resolved from the point declarations below.
            continue;
        } else if (node.kind == SceneNode::Kind::Triangle) {
            continue;
        } else if (node.kind == SceneNode::Kind::Circle) {
            const auto center_it = points.find(node.first); if (center_it == points.end()) continue;
            std::vector<int> indices; const auto center = center_it->second;
            for (int sample = 0; sample <= 32; ++sample) { const double angle = 2 * 3.14159265358979323846 * sample / 32.0; indices.push_back(vertex(center.first + node.radius * std::cos(angle), center.second + node.radius * std::sin(angle), 0)); }
            line(indices);
        } else if (node.kind == SceneNode::Kind::Data2D) {
            std::vector<int> indices; for (const auto& point : node.samples) indices.push_back(vertex(point.first, point.second, 0)); line(indices);
        } else if (node.kind == SceneNode::Kind::Plot2D || node.kind == SceneNode::Kind::Parametric2D) add_planar_plot(node);
        else if (node.kind == SceneNode::Kind::Implicit2D) {
            Expression expression{node.expression}; std::vector<std::vector<int>> contour(121, std::vector<int>(121, 0));
            for (int row = 0; row <= 120; ++row) for (int column = 0; column <= 120; ++column) {
                const double x = node.xmin + (node.xmax - node.xmin) * column / 120.0, y = node.ymin + (node.ymax - node.ymin) * row / 120.0, value = expression.evaluate2(x, y);
                if (std::isfinite(value) && std::abs(value) < .08) contour[row][column] = vertex(x, y, 0);
            }
            for (int row = 0; row <= 120; ++row) for (int column = 0; column <= 120; ++column) {
                if (!contour[row][column]) continue;
                if (column < 120 && contour[row][column + 1]) line({contour[row][column], contour[row][column + 1]});
                if (row < 120 && contour[row + 1][column]) line({contour[row][column], contour[row + 1][column]});
            }
        } else if (node.kind == SceneNode::Kind::VectorField2D) {
            Expression horizontal{node.first}, vertical{node.second};
            for (int row = 0; row <= 12; ++row) for (int column = 0; column <= 12; ++column) {
                const double x = node.xmin + (node.xmax - node.xmin) * column / 12.0, y = node.ymin + (node.ymax - node.ymin) * row / 12.0;
                const double dx = horizontal.evaluate2(x, y), dy = vertical.evaluate2(x, y); if (!std::isfinite(dx) || !std::isfinite(dy)) continue;
                line({vertex(x, y, 0), vertex(x + dx * .18, y + dy * .18, 0)});
            }
        } else if (node.kind == SceneNode::Kind::Parametric4D) {
            Expression expressions[4] = {Expression{node.first}, Expression{node.second}, Expression{node.third}, Expression{node.fourth}}; std::vector<int> path;
            for (int sample = 0; sample <= 320; ++sample) {
                const double parameter = node.xmin + (node.xmax - node.xmin) * sample / 320.0; double values[4]; bool valid = true;
                for (auto& expression : expressions) { const double value = expression.evaluate(parameter); if (!std::isfinite(value)) valid = false; values[&expression - expressions] = value; }
                if (!valid) { line(path); path.clear(); continue; }
                const auto point = project_4d(values[0], values[1], values[2], values[3]); path.push_back(vertex(point[0], point[1], point[2]));
            }
            line(path);
        } else if (node.kind == SceneNode::Kind::Plot4D) {
            const std::array<std::pair<int, int>, 32> edges{{{0,1},{0,2},{0,4},{0,8},{1,3},{1,5},{1,9},{2,3},{2,6},{2,10},{3,7},{3,11},{4,5},{4,6},{4,12},{5,7},{5,13},{6,7},{6,14},{7,15},{8,9},{8,10},{8,12},{9,11},{9,13},{10,11},{11,15},{12,13},{12,14},{13,15},{14,15}}};
            std::array<int, 16> corners{}; for (int index = 0; index < 16; ++index) corners[index] = vertex((index & 1) ? 1.5 : -1.5, (index & 2) ? 1.5 : -1.5, (index & 4) ? 1.5 : -1.5 + (index & 8 ? .35 : -.35));
            for (const auto& edge : edges) line({corners[edge.first], corners[edge.second]});
        } else if (node.kind == SceneNode::Kind::Sphere) {
            const int latitude = 12, longitude = 24; std::vector<std::vector<int>> rings;
            for (int i = 0; i <= latitude; ++i) { const double phi = 3.14159265358979323846 * i / latitude; std::vector<int> ring; for (int j = 0; j < longitude; ++j) { const double theta = 2 * 3.14159265358979323846 * j / longitude; ring.push_back(vertex(node.x + node.radius * std::sin(phi) * std::cos(theta), node.y + node.radius * std::cos(phi), node.z + node.radius * std::sin(phi) * std::sin(theta))); } rings.push_back(ring); }
            for (int i = 0; i < latitude; ++i) for (int j = 0; j < longitude; ++j) face({rings[i][j], rings[i][(j + 1) % longitude], rings[i + 1][(j + 1) % longitude], rings[i + 1][j]});
        } else if (node.kind == SceneNode::Kind::Cube) {
            const double h = node.size / 2; std::vector<int> v;
            for (const auto& p : std::vector<std::array<double, 3>>{{{-h,-h,-h}},{{h,-h,-h}},{{h,h,-h}},{{-h,h,-h}},{{-h,-h,h}},{{h,-h,h}},{{h,h,h}},{{-h,h,h}}}) v.push_back(vertex(node.x + p[0], node.y + p[1], node.z + p[2]));
            for (const auto& f : std::vector<std::vector<int>>{{v[0],v[1],v[2],v[3]},{v[4],v[7],v[6],v[5]},{v[0],v[4],v[5],v[1]},{v[1],v[5],v[6],v[2]},{v[2],v[6],v[7],v[3]},{v[4],v[0],v[3],v[7]}}) face(f);
        } else if (node.kind == SceneNode::Kind::Data3D || node.kind == SceneNode::Kind::Plot3D) {
            const int rows = node.kind == SceneNode::Kind::Data3D ? static_cast<int>(node.grid.size()) : 25;
            const int columns = node.kind == SceneNode::Kind::Data3D ? (rows ? static_cast<int>(node.grid.front().size()) : 0) : 25;
            if (rows < 2 || columns < 2) continue;
            Expression expression{node.expression}; std::vector<std::vector<int>> grid(rows, std::vector<int>(columns, 0));
            for (int row = 0; row < rows; ++row) for (int column = 0; column < columns; ++column) {
                const double x = node.kind == SceneNode::Kind::Data3D ? column : node.xmin + (node.xmax - node.xmin) * column / (columns - 1.0);
                const double y = node.kind == SceneNode::Kind::Data3D ? row : node.ymin + (node.ymax - node.ymin) * row / (rows - 1.0);
                const double z = node.kind == SceneNode::Kind::Data3D ? node.grid[row][column] : expression.evaluate2(x, y);
                if (std::isfinite(z)) grid[row][column] = vertex(x, y, z);
            }
            for (int row = 0; row + 1 < rows; ++row) for (int column = 0; column + 1 < columns; ++column)
                if (grid[row][column] && grid[row][column + 1] && grid[row + 1][column + 1] && grid[row + 1][column]) face({grid[row][column], grid[row][column + 1], grid[row + 1][column + 1], grid[row + 1][column]});
        }
    }
    // Add named lines and triangles in a second pass so forward references work.
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Line || node.kind == SceneNode::Kind::Triangle) {
        auto first = points.find(node.first), second = points.find(node.second), third = points.find(node.third);
        if (first == points.end() || second == points.end()) continue;
        const int a = vertex(first->second.first, first->second.second, 0), b = vertex(second->second.first, second->second.second, 0);
        if (node.kind == SceneNode::Kind::Line) line({a, b});
        else if (third != points.end()) face({a, b, vertex(third->second.first, third->second.second, 0)});
    }
}

void export_stl(const std::string& source, const std::string& output) {
    const auto diagnostics = validate_visual_source(source);
    if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front());
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporary_obj = std::filesystem::temp_directory_path() / ("marila-stl-" + std::to_string(unique) + ".obj");
    auto cleanup = [&]() { std::error_code error; std::filesystem::remove(temporary_obj, error); };
    try {
        export_obj(source, temporary_obj.string());
        std::ifstream input(temporary_obj); if (!input) throw std::runtime_error("unable to read temporary OBJ geometry");
        std::vector<std::array<double, 3>> vertices; std::vector<std::vector<int>> faces; std::string line;
        while (std::getline(input, line)) {
            std::stringstream values(line); std::string kind; values >> kind;
            if (kind == "v") { std::array<double, 3> point{}; values >> point[0] >> point[1] >> point[2]; vertices.push_back(point); }
            else if (kind == "f") { std::vector<int> face; std::string token; while (values >> token) { const size_t slash = token.find('/'); face.push_back(std::stoi(token.substr(0, slash)) - 1); } if (face.size() >= 3) faces.push_back(std::move(face)); }
        }
        if (faces.empty()) throw std::runtime_error("STL export requires at least one triangular or polygonal face");
        std::ofstream output_file(output); if (!output_file) throw std::runtime_error("unable to write STL output '" + output + "'");
        output_file << "solid marila\n";
        auto emit_triangle = [&](const std::array<double, 3>& a, const std::array<double, 3>& b, const std::array<double, 3>& c) {
            const std::array<double, 3> u{b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            std::array<double, 3> normal{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
            const double length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]); if (length > 1e-12) for (double& item : normal) item /= length;
            output_file << " facet normal " << normal[0] << ' ' << normal[1] << ' ' << normal[2] << "\n outer loop\n";
            output_file << "  vertex " << a[0] << ' ' << a[1] << ' ' << a[2] << "\n";
            output_file << "  vertex " << b[0] << ' ' << b[1] << ' ' << b[2] << "\n";
            output_file << "  vertex " << c[0] << ' ' << c[1] << ' ' << c[2] << "\n endloop\n endfacet\n";
        };
        for (const auto& face : faces) for (size_t index = 1; index + 1 < face.size(); ++index) {
            if (face[0] < 0 || face[1] < 0 || face[index + 1] < 0 || static_cast<size_t>(face[0]) >= vertices.size() || static_cast<size_t>(face[index]) >= vertices.size() || static_cast<size_t>(face[index + 1]) >= vertices.size()) throw std::runtime_error("OBJ face references an invalid vertex");
            emit_triangle(vertices[face[0]], vertices[face[index]], vertices[face[index + 1]]);
        }
        output_file << "endsolid marila\n";
    } catch (...) { cleanup(); throw; }
    cleanup();
}

void export_ply(const std::string& source, const std::string& output) {
    const auto diagnostics = validate_visual_source(source);
    if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front());
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporary_obj = std::filesystem::temp_directory_path() / ("marila-ply-" + std::to_string(unique) + ".obj");
    auto cleanup = [&]() { std::error_code error; std::filesystem::remove(temporary_obj, error); };
    try {
        export_obj(source, temporary_obj.string());
        std::ifstream input(temporary_obj); if (!input) throw std::runtime_error("unable to read temporary OBJ geometry");
        std::vector<std::array<double, 3>> vertices; std::vector<std::vector<int>> faces; std::vector<std::vector<int>> lines; std::string line;
        while (std::getline(input, line)) {
            std::stringstream values(line); std::string kind; values >> kind;
            if (kind == "v") { std::array<double, 3> point{}; values >> point[0] >> point[1] >> point[2]; vertices.push_back(point); }
            else if (kind == "f" || kind == "l") {
                std::vector<int> indices; std::string token;
                while (values >> token) { const size_t slash = token.find('/'); indices.push_back(std::stoi(token.substr(0, slash)) - 1); }
                if (indices.size() >= 2) (kind == "f" ? faces : lines).push_back(std::move(indices));
            }
        }
        if (vertices.empty()) throw std::runtime_error("PLY export requires at least one vertex");
        std::ofstream output_file(output); if (!output_file) throw std::runtime_error("unable to write PLY output '" + output + "'");
        output_file << "ply\nformat ascii 1.0\ncomment generated by Marila\nelement vertex " << vertices.size() << "\nproperty double x\nproperty double y\nproperty double z\n";
        output_file << "element face " << faces.size() << "\nproperty list uchar int vertex_indices\n";
        output_file << "element edge " << std::accumulate(lines.begin(), lines.end(), size_t{0}, [](size_t total, const auto& path) { return total + (path.size() > 1 ? path.size() - 1 : 0); }) << "\nproperty int vertex1\nproperty int vertex2\nend_header\n";
        for (const auto& point : vertices) output_file << point[0] << ' ' << point[1] << ' ' << point[2] << '\n';
        for (const auto& face : faces) { output_file << face.size(); for (const int index : face) { if (index < 0 || static_cast<size_t>(index) >= vertices.size()) throw std::runtime_error("PLY face references an invalid vertex"); output_file << ' ' << index; } output_file << '\n'; }
        for (const auto& path : lines) for (size_t index = 1; index < path.size(); ++index) {
            if (path[index - 1] < 0 || path[index] < 0 || static_cast<size_t>(path[index - 1]) >= vertices.size() || static_cast<size_t>(path[index]) >= vertices.size()) throw std::runtime_error("PLY edge references an invalid vertex");
            output_file << path[index - 1] << ' ' << path[index] << '\n';
        }
    } catch (...) { cleanup(); throw; }
    cleanup();
}

void export_video(const std::string& source, const std::string& format, const std::string& output) {
    if (format != "mp4" && format != "gif" && format != "webm")
        throw std::runtime_error("unsupported video export format '" + format + "' (supported: mp4, gif, webm)");
    const auto diagnostics = validate_visual_source(source);
    if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front());
    const auto nodes = parse_visual_source(source);
    const int width = 640, height = 360, fps = 30;
    double duration = 1.0;
    for (const auto& node : nodes) duration = std::max(duration, node.start_time + node.duration * node.repeat);
    const int frames = std::max(1, static_cast<int>(std::ceil(duration * fps)) + 1);
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path frame_directory = std::filesystem::temp_directory_path() / ("marila-frames-" + std::to_string(unique));
    std::filesystem::create_directories(frame_directory);
    auto cleanup = [&]() { std::error_code error; std::filesystem::remove_all(frame_directory, error); };
    auto shell_quote = [](const std::string& value) {
        std::string result = "'";
        for (const char character : value) { if (character == '\'') result += "'\\''"; else result += character; }
        return result + "'";
    };
    auto ease = [](double value, const std::string& mode) {
        value = std::max(0.0, std::min(1.0, value));
        if (mode == "in") return value * value;
        if (mode == "out") return 1.0 - (1.0 - value) * (1.0 - value);
        if (mode == "in-out") return value < .5 ? 2.0 * value * value : 1.0 - std::pow(-2.0 * value + 2.0, 2.0) / 2.0;
        return value;
    };
    auto progress_for = [&](const SceneNode& node, double time) {
        const double local = (time - node.start_time) / std::max(1e-9, node.duration);
        if (local <= 0) return 0.0;
        if (local >= node.repeat) return node.yoyo && node.repeat % 2 == 0 ? 0.0 : 1.0;
        const int cycle = static_cast<int>(std::floor(local));
        const double fraction = local - cycle;
        return ease(node.yoyo && cycle % 2 ? 1.0 - fraction : fraction, node.easing);
    };
    struct VideoExpression {
        std::string text; size_t position{0}; double variable{0}; double secondary{0}; double tertiary{0};
        void skip() { while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) ++position; }
        bool take(char value) { skip(); if (position < text.size() && text[position] == value) { ++position; return true; } return false; }
        double primary() {
            skip();
            if (take('(')) { const double value = add(); take(')'); return value; }
            if (position < text.size() && (std::isdigit(static_cast<unsigned char>(text[position])) || text[position] == '.')) { const char* start = text.c_str() + position; char* end = nullptr; const double value = std::strtod(start, &end); position += static_cast<size_t>(end - start); return value; }
            if (position < text.size() && (std::isalpha(static_cast<unsigned char>(text[position])) || text[position] == '_')) {
                size_t end = position;
                while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_')) ++end;
                const std::string name = text.substr(position, end - position);
                position = end;
                if (take('(')) {
                    const double first = add();
                    const bool two = take(',');
                    const double second = two ? add() : 0;
                    const bool three = two && take(',');
                    const double third = three ? add() : 0;
                    take(')');
                    if (name == "sin") return std::sin(first);
                    if (name == "cos") return std::cos(first);
                    if (name == "tan") return std::tan(first);
                    if (name == "sinh") return std::sinh(first);
                    if (name == "cosh") return std::cosh(first);
                    if (name == "tanh") return std::tanh(first);
                    if (name == "asinh") return std::asinh(first);
                    if (name == "acosh") return std::acosh(first);
                    if (name == "atanh") return std::atanh(first);
                    if (name == "asin") return std::asin(first);
                    if (name == "acos") return std::acos(first);
                    if (name == "atan") return std::atan(first);
                    if (name == "atan2") return std::atan2(first, second);
                    if (name == "sqrt") return std::sqrt(first);
                    if (name == "exp") return std::exp(first);
                    if (name == "log") return std::log(first);
                    if (name == "log2") return std::log2(first);
                    if (name == "log10") return std::log10(first);
                    if (name == "abs") return std::abs(first);
                    if (name == "clamp") return std::max(second, std::min(third, first));
                    if (name == "min") return std::min(first, second);
                    if (name == "max") return std::max(first, second);
                    if (name == "sign") return first > 0 ? 1.0 : (first < 0 ? -1.0 : 0.0);
                    if (name == "hypot") return std::hypot(first, second);
                    if (name == "pow") return std::pow(first, second);
                    return 0;
                }
                if (name == "x" || name == "t") return variable;
                if (name == "y") return secondary;
                if (name == "pi") return 3.141592653589793;
                if (name == "e") return 2.718281828459045;
            }
            return 0;
        }
        double unary() { skip(); if (take('+')) return unary(); if (take('-')) return -unary(); return primary(); }
        double power() { double value = unary(); if (take('^')) value = std::pow(value, power()); return value; }
        double multiply() { double value = power(); while (true) { if (take('*')) value *= power(); else if (take('/')) value /= power(); else break; } return value; }
        double add() { double value = multiply(); while (true) { if (take('+')) value += multiply(); else if (take('-')) value -= multiply(); else break; } return value; }
        double evaluate(double input) { variable = input; position = 0; return add(); }
        double evaluate2(double first, double second) { variable = first; secondary = second; position = 0; return add(); }
    };
    std::map<std::string, std::pair<double, double>> points;
    for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Point) points[node.id] = {node.x, node.y};
    auto position = [&](const std::string& id, double time) {
        auto found = points.find(id); std::pair<double, double> result = found == points.end() ? std::pair<double, double>{0, 0} : found->second;
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Move && node.id == id) {
            if (time < node.start_time) continue;
            const double progress = progress_for(node, time);
            result = {node.from_x + (node.x - node.from_x) * progress, node.from_y + (node.y - node.from_y) * progress};
        }
        return result;
    };
    auto opacity = [&](const std::string& id, double time) {
        double result = 1.0;
        for (const auto& node : nodes) if (node.kind == SceneNode::Kind::Fade && node.first == id && time >= node.start_time) {
            const double progress = progress_for(node, time);
            result = node.fade_in ? progress : 1.0 - progress;
        }
        return std::max(0.0, std::min(1.0, result));
    };
    auto draw_frame = [&](const std::filesystem::path& path, double time) {
        std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 3), 4);
        auto blend = [&](int px, int py, unsigned char r, unsigned char g, unsigned char b, double alpha = 1.0) {
            if (px < 0 || px >= width || py < 0 || py >= height) return;
            const size_t offset = static_cast<size_t>((py * width + px) * 3); const double a = std::max(0.0, std::min(1.0, alpha));
            pixels[offset] = static_cast<unsigned char>(pixels[offset] * (1 - a) + r * a);
            pixels[offset + 1] = static_cast<unsigned char>(pixels[offset + 1] * (1 - a) + g * a);
            pixels[offset + 2] = static_cast<unsigned char>(pixels[offset + 2] * (1 - a) + b * a);
        };
        auto project = [&](double x, double y) { return std::pair<int, int>{static_cast<int>(width / 2 + x * 40), static_cast<int>(height / 2 - y * 40)}; };
        auto project3d = [&](double x, double y, double z) { const double yaw = .65, pitch = .45; const double rx = x * std::cos(yaw) - z * std::sin(yaw), rz = x * std::sin(yaw) + z * std::cos(yaw), ry = y * std::cos(pitch) - rz * std::sin(pitch), rz2 = y * std::sin(pitch) + rz * std::cos(pitch); return std::pair<int, int>{static_cast<int>(width / 2 + (rx - ry) * 28), static_cast<int>(height / 2 - (rx + ry) * 14 + rz2 * 20)}; };
        auto draw_line = [&](std::pair<int, int> a, std::pair<int, int> b, unsigned char r, unsigned char g, unsigned char bl, double alpha = 1.0) {
            const int dx = std::abs(b.first - a.first), sx = a.first < b.first ? 1 : -1, dy = -std::abs(b.second - a.second), sy = a.second < b.second ? 1 : -1; int error = dx + dy;
            while (true) { blend(a.first, a.second, r, g, bl, alpha); if (a == b) break; const int twice = 2 * error; if (twice >= dy) { error += dy; a.first += sx; } if (twice <= dx) { error += dx; a.second += sy; } }
        };
        auto draw_circle = [&](std::pair<int, int> center, int radius, unsigned char r, unsigned char g, unsigned char b, double alpha = 1.0) {
            for (int y = -radius; y <= radius; ++y) for (int x = -radius; x <= radius; ++x) if (x * x + y * y <= radius * radius) blend(center.first + x, center.second + y, r, g, b, alpha);
        };
        for (int x = 0; x < width; x += 40) for (int y = 0; y < height; ++y) blend(x, y, 35, 45, 65, .45);
        for (int y = 0; y < height; y += 40) for (int x = 0; x < width; ++x) blend(x, y, 35, 45, 65, .45);
        for (const auto& node : nodes) {
            if (node.kind == SceneNode::Kind::Point || node.kind == SceneNode::Kind::Move) {
                if (node.kind == SceneNode::Kind::Move && std::any_of(nodes.begin(), nodes.end(), [&](const SceneNode& later) { return later.kind == SceneNode::Kind::Move && later.id == node.id && later.start_time > node.start_time; })) continue;
                const auto p = position(node.id, time); draw_circle(project(p.first, p.second), 6, 34, 211, 238, opacity(node.id, time));
            } else if (node.kind == SceneNode::Kind::Line) {
                draw_line(project(position(node.first, time).first, position(node.first, time).second), project(position(node.second, time).first, position(node.second, time).second), 99, 102, 241);
            } else if (node.kind == SceneNode::Kind::Triangle) {
                const auto a = project(position(node.first, time).first, position(node.first, time).second), b = project(position(node.second, time).first, position(node.second, time).second), c = project(position(node.third, time).first, position(node.third, time).second);
                draw_line(a, b, 245, 158, 11); draw_line(b, c, 245, 158, 11); draw_line(c, a, 245, 158, 11);
            } else if (node.kind == SceneNode::Kind::Circle) {
                const auto p = project(position(node.first, time).first, position(node.first, time).second); draw_circle(p, std::max(1, static_cast<int>(node.radius * 40)), 34, 211, 238, .25); draw_circle(p, std::max(1, static_cast<int>(node.radius * 40)), 34, 211, 238, .8);
            } else if (node.kind == SceneNode::Kind::Parametric2D) {
                const int count = 240; const double fraction = node.animate_trace ? progress_for(node, time) : 1.0;
                const int limit = static_cast<int>(count * fraction); VideoExpression x_expression{node.first}, y_expression{node.second};
                std::vector<std::pair<double, double>> samples; samples.reserve(static_cast<size_t>(limit + 1));
                for (int index = 0; index <= limit; ++index) { const double parameter = node.xmin + (node.xmax - node.xmin) * index / count; x_expression.variable = parameter; y_expression.variable = parameter; const double x = x_expression.evaluate(parameter), y = y_expression.evaluate(parameter); if (std::isfinite(x) && std::isfinite(y)) samples.emplace_back(x, y); }
                if (samples.size() >= 2) { auto map_plot = [&](const std::pair<double, double>& sample) { return std::pair<int, int>{static_cast<int>(width / 2 + sample.first * 40), static_cast<int>(height / 2 - sample.second * 40)}; }; for (size_t index = 1; index < samples.size(); ++index) draw_line(map_plot(samples[index - 1]), map_plot(samples[index]), 245, 158, 11); }
            } else if (node.kind == SceneNode::Kind::VectorField2D) {
                VideoExpression x_expression{node.first}, y_expression{node.second};
                for (int row = 0; row <= 12; ++row) for (int column = 0; column <= 12; ++column) {
                    const double x = node.xmin + (node.xmax - node.xmin) * column / 12.0, y = node.ymin + (node.ymax - node.ymin) * row / 12.0;
                    const double dx = x_expression.evaluate2(x, y), dy = y_expression.evaluate2(x, y); if (!std::isfinite(dx) || !std::isfinite(dy)) continue;
                    draw_line(project(x, y), project(x + dx * .15, y + dy * .15), 34, 211, 238, .8);
                }
            } else if (node.kind == SceneNode::Kind::Implicit2D) {
                VideoExpression expression{node.expression};
                for (int row = 0; row < 160; ++row) for (int column = 0; column < 160; ++column) {
                    const double x = node.xmin + (node.xmax - node.xmin) * column / 159.0, y = node.ymin + (node.ymax - node.ymin) * row / 159.0;
                    const double value = expression.evaluate2(x, y); if (std::isfinite(value) && std::abs(value) < .08) draw_circle(project(x, y), 1, 251, 113, 133, .9);
                }
            } else if (node.kind == SceneNode::Kind::Parametric4D) {
                const int count = 320; const double fraction = node.animate_trace ? progress_for(node, time) : 1.0; const int limit = static_cast<int>(count * fraction);
                VideoExpression expressions[4] = {VideoExpression{node.first}, VideoExpression{node.second}, VideoExpression{node.third}, VideoExpression{node.fourth}};
                std::pair<int, int> previous{}; bool have_previous = false;
                for (int index = 0; index <= limit; ++index) {
                    const double parameter = node.xmin + (node.xmax - node.xmin) * index / count; double values[4]; bool valid = true;
                    for (auto& expression : expressions) { const double value = expression.evaluate(parameter); if (!std::isfinite(value)) valid = false; values[&expression - expressions] = value; }
                    if (!valid) { have_previous = false; continue; }
                    const double angle = .5, xx = values[0] * std::cos(angle) - values[3] * std::sin(angle), zz = values[0] * std::sin(angle) + values[3] * std::cos(angle); const auto current = project3d(xx, values[1], values[2] + zz * .15);
                    if (have_previous) draw_line(previous, current, 192, 132, 252, .9); previous = current; have_previous = true;
                }
            } else if (node.kind == SceneNode::Kind::Plot4D) {
                const std::array<std::pair<int, int>, 32> edges{{{0,1},{0,2},{0,4},{0,8},{1,3},{1,5},{1,9},{2,3},{2,6},{2,10},{3,7},{3,11},{4,5},{4,6},{4,12},{5,7},{5,13},{6,7},{6,14},{7,15},{8,9},{8,10},{8,12},{9,11},{9,13},{10,11},{10,14},{11,15},{12,13},{12,14},{13,15},{14,15}}};
                auto corner = [&](int index) { return project3d((index & 1) ? 1.5 : -1.5, (index & 2) ? 1.5 : -1.5, (index & 4) ? 1.5 : -1.5 + (index & 8 ? .35 : -.35)); };
                for (const auto& edge : edges) draw_line(corner(edge.first), corner(edge.second), 168, 85, 247, .8);
            } else if (node.kind == SceneNode::Kind::Data3D || node.kind == SceneNode::Kind::Plot3D) {
                const int rows = node.kind == SceneNode::Kind::Data3D ? static_cast<int>(node.grid.size()) : 20;
                const int columns = node.kind == SceneNode::Kind::Data3D && rows ? static_cast<int>(node.grid.front().size()) : 20;
                if (rows >= 2 && columns >= 2) { VideoExpression expression{node.expression}; std::vector<std::vector<double>> grid(rows, std::vector<double>(columns)); for (int row = 0; row < rows; ++row) for (int column = 0; column < columns; ++column) grid[row][column] = node.kind == SceneNode::Kind::Data3D ? node.grid[row][column] : expression.evaluate2(node.xmin + (node.xmax - node.xmin) * column / (columns - 1.0), node.ymin + (node.ymax - node.ymin) * row / (rows - 1.0)); const double fraction = node.animate_trace ? progress_for(node, time) : 1.0; const int visible_rows = std::max(0, static_cast<int>(std::ceil(rows * fraction))), visible_columns = std::max(0, static_cast<int>(std::ceil(columns * fraction))); auto point = [&](int row, int column) { const double x = node.kind == SceneNode::Kind::Data3D ? column - (columns - 1) / 2.0 : node.xmin + (node.xmax - node.xmin) * column / (columns - 1.0); const double y = node.kind == SceneNode::Kind::Data3D ? (rows - 1) / 2.0 - row : node.ymin + (node.ymax - node.ymin) * row / (rows - 1.0); return project3d(x, y, grid[row][column]); }; for (int row = 0; row < visible_rows; ++row) for (int column = 1; column < visible_columns; ++column) draw_line(point(row, column - 1), point(row, column), 16, 185, 129); for (int column = 0; column < visible_columns; ++column) for (int row = 1; row < visible_rows; ++row) draw_line(point(row - 1, column), point(row, column), 16, 185, 129); }
            } else if (node.kind == SceneNode::Kind::Sphere) {
                const int latitude = 10, longitude = 20; std::vector<std::vector<std::pair<int, int>>> rings;
                for (int row = 0; row <= latitude; ++row) { std::vector<std::pair<int, int>> ring; const double phi = 3.141592653589793 * row / latitude; for (int column = 0; column < longitude; ++column) { const double theta = 2 * 3.141592653589793 * column / longitude; ring.push_back(project3d(node.x + node.radius * std::sin(phi) * std::cos(theta), node.y + node.radius * std::cos(phi), node.z + node.radius * std::sin(phi) * std::sin(theta))); } rings.push_back(ring); }
                for (int row = 0; row <= latitude; ++row) for (int column = 1; column <= longitude; ++column) draw_line(rings[row][column - 1], rings[row][column % longitude], 34, 211, 238, .8);
                for (int row = 1; row <= latitude; ++row) for (int column = 0; column < longitude; ++column) draw_line(rings[row - 1][column], rings[row][column], 34, 211, 238, .8);
            } else if (node.kind == SceneNode::Kind::Cube) {
                const double half = node.size / 2; const std::vector<std::array<double, 3>> corners{{{-half, -half, -half}}, {{half, -half, -half}}, {{half, half, -half}}, {{-half, half, -half}}, {{-half, -half, half}}, {{half, -half, half}}, {{half, half, half}}, {{-half, half, half}}}; std::vector<std::pair<int, int>> projected; for (const auto& corner : corners) projected.push_back(project3d(node.x + corner[0], node.y + corner[1], node.z + corner[2])); const std::array<std::pair<int, int>, 12> edges{{{0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7}}}; for (const auto& edge : edges) draw_line(projected[edge.first], projected[edge.second], 245, 158, 11, .9);
            } else if (node.kind == SceneNode::Kind::Data2D || node.kind == SceneNode::Kind::Plot2D) {
                std::vector<std::pair<double, double>> samples;
                if (node.kind == SceneNode::Kind::Data2D) {
                    const size_t limit = node.animate_trace ? static_cast<size_t>(std::floor(node.samples.size() * progress_for(node, time))) : node.samples.size();
                    samples.assign(node.samples.begin(), node.samples.begin() + std::min(limit, node.samples.size()));
                }
                else { const int count = 240; const double fraction = node.animate_trace ? progress_for(node, time) : 1.0; const int limit = static_cast<int>(count * fraction); VideoExpression expression{node.expression}; for (int index = 0; index <= limit; ++index) { const double x = node.xmin + (node.xmax - node.xmin) * index / count; const double y = expression.evaluate(x); if (std::isfinite(y)) samples.emplace_back(x, y); } }
                if (samples.size() >= 2) { double xmin = samples.front().first, xmax = xmin, ymin = samples.front().second, ymax = ymin; for (const auto& sample : samples) { xmin = std::min(xmin, sample.first); xmax = std::max(xmax, sample.first); ymin = std::min(ymin, sample.second); ymax = std::max(ymax, sample.second); } auto map_plot = [&](const std::pair<double, double>& sample) { return std::pair<int, int>{static_cast<int>(40 + (sample.first - xmin) * (width - 80) / std::max(1e-12, xmax - xmin)), static_cast<int>(height - 40 - (sample.second - ymin) * (height - 80) / std::max(1e-12, ymax - ymin))}; }; for (size_t index = 1; index < samples.size(); ++index) draw_line(map_plot(samples[index - 1]), map_plot(samples[index]), 56, 189, 248); }
            } else if (node.kind == SceneNode::Kind::Morph) {
                const double progress = progress_for(node, time), size = node.x + (node.y - node.x) * progress; const auto p = project(0, 0); const int half = static_cast<int>(size / 2);
                if (node.second == "circle" || (node.first == "square" && node.second == "circle" && progress > .5)) draw_circle(p, std::max(1, half), 245, 158, 11);
                else { draw_line({p.first - half, p.second - half}, {p.first + half, p.second - half}, 245, 158, 11); draw_line({p.first + half, p.second - half}, {p.first + half, p.second + half}, 245, 158, 11); draw_line({p.first + half, p.second + half}, {p.first - half, p.second + half}, 245, 158, 11); draw_line({p.first - half, p.second + half}, {p.first - half, p.second - half}, 245, 158, 11); }
            }
        }
        std::ofstream frame(path, std::ios::binary); if (!frame) throw std::runtime_error("unable to write animation frame");
        frame << "P6\n" << width << ' ' << height << "\n255\n"; frame.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    };
    try {
        for (int frame = 0; frame < frames; ++frame) {
            std::ostringstream filename; filename << "frame-" << std::setfill('0') << std::setw(6) << frame << ".ppm";
            draw_frame(frame_directory / filename.str(), static_cast<double>(frame) / fps);
        }
        const std::string input_pattern = (frame_directory / "frame-%06d.ppm").string();
        std::string command = "ffmpeg -y -loglevel error -framerate 30 -i " + shell_quote(input_pattern);
        if (format == "mp4") command += " -c:v libx264 -pix_fmt yuv420p ";
        else if (format == "webm") command += " -c:v libvpx-vp9 -pix_fmt yuv420p ";
        else command += " -vf fps=15 ";
        command += shell_quote(output);
        if (std::system(command.c_str()) != 0) throw std::runtime_error("FFmpeg failed while encoding " + format + " output");
    } catch (...) { cleanup(); throw; }
    cleanup();
}

void export_gltf(const std::string& source, const std::string& output) {
    const auto diagnostics = validate_visual_source(source);
    if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front());
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporary_obj = std::filesystem::temp_directory_path() / ("marila-gltf-" + std::to_string(unique) + ".obj");
    auto cleanup = [&]() { std::error_code error; std::filesystem::remove(temporary_obj, error); };
    try {
        export_obj(source, temporary_obj.string());
        std::ifstream input(temporary_obj);
        if (!input) throw std::runtime_error("unable to read temporary OBJ geometry");
        std::vector<float> positions;
        std::vector<std::uint32_t> triangles, lines;
        std::string line;
        while (std::getline(input, line)) {
            std::stringstream values(line);
            std::string kind; values >> kind;
            if (kind == "v") { double x, y, z; values >> x >> y >> z; positions.push_back(static_cast<float>(x)); positions.push_back(static_cast<float>(y)); positions.push_back(static_cast<float>(z)); }
            else if (kind == "f" || kind == "l") {
                std::vector<std::uint32_t> indices; std::string token;
                while (values >> token) { const size_t slash = token.find('/'); indices.push_back(static_cast<std::uint32_t>(std::stoul(token.substr(0, slash)) - 1)); }
                if (kind == "f" && indices.size() >= 3) for (size_t index = 1; index + 1 < indices.size(); ++index) { triangles.push_back(indices[0]); triangles.push_back(indices[index]); triangles.push_back(indices[index + 1]); }
                if (kind == "l" && indices.size() >= 2) for (size_t index = 1; index < indices.size(); ++index) { lines.push_back(indices[index - 1]); lines.push_back(indices[index]); }
            }
        }
        if (positions.empty()) throw std::runtime_error("scene produced no vertices for glTF export");
        std::vector<unsigned char> binary;
        auto align = [&]() { while (binary.size() % 4) binary.push_back(0); };
        const size_t position_offset = binary.size();
        for (const float value : positions) { const auto* bytes = reinterpret_cast<const unsigned char*>(&value); binary.insert(binary.end(), bytes, bytes + sizeof(float)); }
        align();
        const size_t triangle_offset = binary.size();
        for (const std::uint32_t value : triangles) { const auto* bytes = reinterpret_cast<const unsigned char*>(&value); binary.insert(binary.end(), bytes, bytes + sizeof(std::uint32_t)); }
        align();
        const size_t line_offset = binary.size();
        for (const std::uint32_t value : lines) { const auto* bytes = reinterpret_cast<const unsigned char*>(&value); binary.insert(binary.end(), bytes, bytes + sizeof(std::uint32_t)); }
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string encoded;
        for (size_t index = 0; index < binary.size(); index += 3) {
            const std::uint32_t value = (static_cast<std::uint32_t>(binary[index]) << 16) | (index + 1 < binary.size() ? static_cast<std::uint32_t>(binary[index + 1]) << 8 : 0) | (index + 2 < binary.size() ? binary[index + 2] : 0);
            encoded += alphabet[(value >> 18) & 63]; encoded += alphabet[(value >> 12) & 63]; encoded += index + 1 < binary.size() ? alphabet[(value >> 6) & 63] : '='; encoded += index + 2 < binary.size() ? alphabet[value & 63] : '=';
        }
        const size_t vertex_count = positions.size() / 3;
        float minimum[3] = {positions[0], positions[1], positions[2]}, maximum[3] = {positions[0], positions[1], positions[2]};
        for (size_t index = 0; index < positions.size(); index += 3) for (int axis = 0; axis < 3; ++axis) { minimum[axis] = std::min(minimum[axis], positions[index + axis]); maximum[axis] = std::max(maximum[axis], positions[index + axis]); }
        std::ofstream output_file(output);
        if (!output_file) throw std::runtime_error("unable to write glTF output '" + output + "'");
        output_file << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Marila\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[";
        bool primitive_written = false;
        if (!triangles.empty()) { output_file << "{\"attributes\":{\"POSITION\":0},\"indices\":1,\"mode\":4}"; primitive_written = true; }
        if (!lines.empty()) { if (primitive_written) output_file << ','; output_file << "{\"attributes\":{\"POSITION\":0},\"indices\":2,\"mode\":1}"; }
        output_file << "]}],\"buffers\":[{\"byteLength\":" << binary.size() << ",\"uri\":\"data:application/octet-stream;base64," << encoded << "\"}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" << position_offset + vertex_count * 12 << ",\"target\":34962}";
        if (!triangles.empty()) output_file << ",{\"buffer\":0,\"byteOffset\":" << triangle_offset << ",\"byteLength\":" << triangles.size() * 4 << ",\"target\":34963}";
        if (!lines.empty()) output_file << ",{\"buffer\":0,\"byteOffset\":" << line_offset << ",\"byteLength\":" << lines.size() * 4 << ",\"target\":34963}";
        output_file << "],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":" << vertex_count << ",\"type\":\"VEC3\",\"min\":[" << minimum[0] << ',' << minimum[1] << ',' << minimum[2] << "],\"max\":[" << maximum[0] << ',' << maximum[1] << ',' << maximum[2] << "]}";
        int view = 1;
        if (!triangles.empty()) { output_file << ",{\"bufferView\":" << view++ << ",\"componentType\":5125,\"count\":" << triangles.size() << ",\"type\":\"SCALAR\"}"; }
        if (!lines.empty()) { output_file << ",{\"bufferView\":" << view << ",\"componentType\":5125,\"count\":" << lines.size() << ",\"type\":\"SCALAR\"}"; }
        output_file << "]}\n";
    } catch (...) { cleanup(); throw; }
    cleanup();
}

void export_svg(const std::string& source, const std::string& output) {
    const std::string type = detect_file_type(source);
    if (type != "image" && type != "scene" && type != "video") throw std::runtime_error("SVG export expects an image, scene, or video source");
    const auto diagnostics = validate_visual_source(source);
    if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front());
    write_file(output, render_image(source));
}

void export_png(const std::string& source, const std::string& output) {
    const std::string type = detect_file_type(source);
    if (type != "image" && type != "scene" && type != "video") throw std::runtime_error("PNG export expects an image, scene, or video source");
    const auto diagnostics = validate_visual_source(source);
    if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front());
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporary_svg = std::filesystem::temp_directory_path() / ("marila-png-" + std::to_string(unique) + ".svg");
    auto cleanup = [&]() { std::error_code error; std::filesystem::remove(temporary_svg, error); };
    auto shell_quote = [](const std::string& value) {
        std::string result = "'";
        for (const char character : value) { if (character == '\'') result += "'\\''"; else result += character; }
        return result + "'";
    };
    try {
        write_file(temporary_svg.string(), render_image(source));
        const std::string command = "rsvg-convert -f png -o " + shell_quote(output) + " " + shell_quote(temporary_svg.string());
        if (std::system(command.c_str()) != 0) throw std::runtime_error("rsvg-convert failed while creating PNG output");
    } catch (...) { cleanup(); throw; }
    cleanup();
}

void export_pdf(const std::string& source, const std::string& output) {
    const std::string type = detect_file_type(source);
    const auto diagnostics = validate_visual_source(source);
    if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front());
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporary_directory = std::filesystem::temp_directory_path() / ("marila-pdf-" + std::to_string(unique));
    std::filesystem::create_directories(temporary_directory);
    auto cleanup = [&]() { std::error_code error; std::filesystem::remove_all(temporary_directory, error); };
    auto shell_quote = [](const std::string& value) {
        std::string result = "'";
        for (const char character : value) { if (character == '\'') result += "'\\''"; else result += character; }
        return result + "'";
    };
    try {
        if (type == "latex" || type == "tikz") {
            const std::filesystem::path tex_path = temporary_directory / "document.tex";
            write_file(tex_path.string(), type == "latex" ? render_latex(source) : render_tikz(source));
            const std::string command = "pdflatex -interaction=nonstopmode -halt-on-error -output-directory " + shell_quote(temporary_directory.string()) + " " + shell_quote(tex_path.string()) + " >/dev/null 2>&1";
            if (std::system(command.c_str()) != 0) throw std::runtime_error("pdflatex failed while creating PDF output");
            const std::filesystem::path generated = temporary_directory / "document.pdf";
            if (!std::filesystem::exists(generated)) throw std::runtime_error("pdflatex did not produce a PDF output");
            std::filesystem::copy_file(generated, output, std::filesystem::copy_options::overwrite_existing);
        } else if (type == "image" || type == "scene" || type == "video") {
            const std::filesystem::path svg_path = temporary_directory / "image.svg";
            write_file(svg_path.string(), render_image(source));
            const std::string command = "rsvg-convert -f pdf -o " + shell_quote(output) + " " + shell_quote(svg_path.string());
            if (std::system(command.c_str()) != 0) throw std::runtime_error("rsvg-convert failed while creating PDF output");
        } else {
            throw std::runtime_error("PDF export expects a latex, tikz, image, scene, or video source");
        }
    } catch (...) { cleanup(); throw; }
    cleanup();
}

void render_source(const std::string& source, const std::string& output) {
    const std::string type = detect_file_type(source);
    const auto diagnostics = validate_visual_source(source);
    if (!diagnostics.empty()) throw std::runtime_error(diagnostics.front());
    if (type == "document") write_file(output, render_document(source));
    else if (type == "latex") write_file(output, render_latex(source));
    else if (type == "tikz") write_file(output, render_tikz(source));
    else if (type == "sheet") write_file(output, render_sheet(source));
    else if (type == "image") write_file(output, render_image(source));
    else if (type == "video" || type == "scene") write_file(output, render_scene(source));
    else throw std::runtime_error("program files do not produce a render output");
}

std::string format_source(const std::string& source) {
    std::stringstream input(source), output;
    std::string line;
    int indent = 0;
    char quote = '\0';
    bool escaped = false, block_comment = false;
    while (std::getline(input, line)) {
        const size_t first = line.find_first_not_of(" \t");
        const std::string trimmed = first == std::string::npos ? "" : line.substr(first);
        if (trimmed.empty()) { output << '\n'; continue; }
        bool closes = false;
        char last_structural = '\0';
        bool saw_code = false;
        for (size_t index = 0; index < line.size(); ++index) {
            const char character = line[index];
            if (block_comment) {
                if (character == '*' && index + 1 < line.size() && line[index + 1] == '/') { block_comment = false; ++index; }
                continue;
            }
            if (quote) {
                if (escaped) escaped = false;
                else if (character == '\\') escaped = true;
                else if (character == quote) quote = '\0';
                continue;
            }
            if (character == '"' || character == '\'') { quote = character; saw_code = true; continue; }
            if (character == '/' && index + 1 < line.size() && line[index + 1] == '*') { block_comment = true; ++index; continue; }
            if (character == '#' || (character == '/' && index + 1 < line.size() && line[index + 1] == '/') ||
                (character == '%' && line.find_first_not_of(" \t") == index)) break;
            if (!std::isspace(static_cast<unsigned char>(character))) {
                if (!saw_code && (character == '}' || character == ']')) closes = true;
                saw_code = true;
                if (character == '{' || character == '[' || character == '}' || character == ']') last_structural = character;
            }
        }
        if (closes) indent = std::max(0, indent - 1);
        output << std::string(static_cast<size_t>(indent) * 2, ' ') << trimmed << '\n';
        if (last_structural == '{' || last_structural == '[') ++indent;
    }
    return output.str();
}

int lint_source(const std::string& source) {
    const std::string type = detect_file_type(source);
    if (source.find("file type") == std::string::npos) {
        std::cerr << "warning: source has no explicit file type\n";
    } else if (type != "program" && type != "document" && type != "latex" && type != "tikz" &&
               type != "sheet" && type != "image" && type != "video" && type != "scene") {
        std::cerr << "error: unsupported file type '" << type << "'\n";
        return 1;
    }
    int square = 0, curly = 0, parentheses = 0;
    char quote = '\0';
    bool escaped = false, block_comment = false, line_comment = false;
    for (size_t index = 0; index < source.size(); ++index) {
        const char character = source[index];
        if (line_comment) { if (character == '\n') line_comment = false; continue; }
        if (block_comment) {
            if (character == '*' && index + 1 < source.size() && source[index + 1] == '/') { block_comment = false; ++index; }
            continue;
        }
        if (quote) {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == quote) quote = '\0';
            continue;
        }
        if (character == '"' || character == '\'') { quote = character; continue; }
        if (character == '#' || (character == '/' && index + 1 < source.size() && source[index + 1] == '/')) { line_comment = true; if (character == '/') ++index; continue; }
        if (character == '/' && index + 1 < source.size() && source[index + 1] == '*') { block_comment = true; ++index; continue; }
        if (character == '[') ++square; else if (character == ']') --square;
        else if (character == '{') ++curly; else if (character == '}') --curly;
        else if (character == '(') ++parentheses; else if (character == ')') --parentheses;
        if (square < 0 || curly < 0 || parentheses < 0) { std::cerr << "error: unmatched closing delimiter\n"; return 1; }
    }
    if (quote || block_comment || square || curly || parentheses) { std::cerr << "error: unbalanced source delimiters\n"; return 1; }
    if (type == "image" || type == "scene" || type == "video") {
        const auto diagnostics = validate_visual_source(source);
        for (const auto& diagnostic : diagnostics) std::cerr << "error: " << diagnostic << '\n';
        if (!diagnostics.empty()) return 1;
    }
    if (type != "program" && source.find("render") == std::string::npos)
        std::cerr << "warning: renderer source has no render directive\n";
    if (type == "scene" && parse_visual_source(source).empty())
        std::cerr << "warning: scene contains no plot or animation primitive\n";
    return 0;
}

}  // namespace marila
