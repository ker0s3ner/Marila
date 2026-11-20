#include "renderers.hpp"
#include "scene_model.hpp"

#include <fstream>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
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
    std::regex item(R"MARILA((\w+)\s*-\s*"([^"]*)"(?:\s*::\s*(?:"([^"]*)"|([^\r\n\]]+)))?)MARILA");
    for (std::sregex_iterator it(source.begin(), source.end(), item), end; it != end; ++it) {
        const std::string id = (*it)[1], text = html_escape((*it)[2]);
        const std::string subtitle = html_escape((*it)[3].matched ? (*it)[3].str() : (*it)[4].str());
        if (id.rfind("table", 0) == 0) {
            body << "<table class=\"document-table\"><tbody>";
            const auto rows = document_table_rows((*it)[2]);
            for (size_t row = 0; row < rows.size(); ++row) { body << "<tr>"; for (const auto& cell : rows[row]) body << (row == 0 ? "<th>" : "<td>") << html_escape(cell) << (row == 0 ? "</th>" : "</td>"); body << "</tr>\n"; }
            body << "</tbody></table>\n";
        } else if (id.rfind("bullet", 0) == 0 || id.rfind("item", 0) == 0) {
            body << "<ul><li>" << text << "</li></ul>\n";
        } else if (id.rfind("code", 0) == 0) {
            body << "<pre><code>" << text << "</code></pre>\n";
        } else if (id.rfind("theorem", 0) == 0 || id.rfind("definition", 0) == 0 ||
            id.rfind("proof", 0) == 0 || id.rfind("prop", 0) == 0) {
            body << "<section class=\"callout\"><h3>" << html_escape(id) << "</h3><p>" << text << "</p>";
            if (!subtitle.empty()) body << "<p>" << subtitle << "</p>";
            body << "</section>\n";
        } else if (id.rfind("equation", 0) == 0 || id.rfind("math", 0) == 0) {
            body << "<div class=\"equation\" id=\"" << html_escape(id) << "\"><code>" << text << "</code></div>\n";
        } else if (!subtitle.empty()) {
            body << "<header><h1>" << text << "</h1><p>" << subtitle << "</p></header>\n";
        } else {
            body << "<p>" << text << "</p>\n";
        }
    }
    const auto visual_nodes = parse_visual_source(source);
    if (!visual_nodes.empty()) body << "<figure><figcaption>Marila visual</figcaption>" << render_image(source) << "</figure>\n";
    return "<!doctype html><meta charset=\"utf-8\"><title>Marila document</title>"
           "<style>body{max-width:800px;margin:40px auto;font:18px Georgia;line-height:1.6}"
           ".callout{border-left:4px solid #6366f1;background:#f1f5f9;padding:12px;margin:20px 0}.equation{font:22px Georgia;text-align:center;padding:18px;margin:18px 0;background:#e2e8f0;color:#0f172a}.document-table{border-collapse:collapse;margin:20px 0}.document-table th,.document-table td{border:1px solid #94a3b8;padding:6px 10px}.document-table th{background:#e2e8f0}"
           "header{border-bottom:2px solid #334155}</style><main>" + body.str() + "</main>";
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
    std::regex item(R"MARILA((\w+)\s*-\s*"([^"]*)"(?:\s*::\s*(?:"([^"]*)"|([^\r\n\]]+)))?)MARILA");
    for (std::sregex_iterator it(source.begin(), source.end(), item), end; it != end; ++it) {
        const std::string id = (*it)[1], text = (*it)[2], subtitle = (*it)[3].matched ? (*it)[3].str() : (*it)[4].str();
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
        } else if (id.rfind("code", 0) == 0) {
            body << "\\begin{verbatim}\n" << text << "\n\\end{verbatim}\n";
        } else if (id.rfind("theorem", 0) == 0 || id.rfind("definition", 0) == 0 ||
            id.rfind("proof", 0) == 0 || id.rfind("prop", 0) == 0) {
            body << "\\begin{quote}\\textbf{" << latex_escape(id) << "}. "
                 << latex_inline(text); if (!subtitle.empty()) body << "\\par " << latex_inline(subtitle); body << "\\end{quote}\n";
        } else if (id.rfind("equation", 0) == 0 || id.rfind("math", 0) == 0) {
            body << "\\[" << text << "\\]\n";
        } else if (!subtitle.empty()) {
            body << "\\section{" << latex_escape(text) << "}\n"
                 << latex_inline(subtitle) << "\n";
        } else {
            body << latex_inline(text) << "\n\n";
        }
    }
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
    return std::string("\\documentclass{article}\n\\usepackage{amsmath,amssymb}") + (visual.empty() ? "" : "\n\\usepackage{tikz}") +
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

    std::function<double(const std::string&, std::vector<std::string>&)> evaluate;
    evaluate = [&](const std::string& input, std::vector<std::string>& stack) -> double {
        std::string expression = input;
        expression.erase(0, expression.find_first_not_of(" \t"));
        if (!expression.empty()) expression.erase(expression.find_last_not_of(" \t\r\n") + 1);
        if (!expression.empty() && expression.front() == '=') expression.erase(0, 1);
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
        if (std::regex_match(expression, match, std::regex(R"((SUM|AVERAGE|COUNT|MIN|MAX|MEDIAN|STDEV)\(([A-Z]+[0-9]+):([A-Z]+[0-9]+)\))"))) {
            const std::string first = match[2], last = match[3];
            std::smatch first_parts, last_parts;
            std::regex_match(first, first_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
            std::regex_match(last, last_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
            int left = spreadsheet_column(first_parts[1]), right = spreadsheet_column(last_parts[1]);
            int top = std::stoi(first_parts[2]), bottom = std::stoi(last_parts[2]);
            double total = 0; int count = 0; double minimum = 0; double maximum = 0; std::vector<double> samples;
            for (int row = std::min(top, bottom); row <= std::max(top, bottom); ++row)
                for (int column = std::min(left, right); column <= std::max(left, right); ++column) {
                    int column_value = column; std::string name;
                    while (column_value) { name.insert(name.begin(), static_cast<char>('A' + (column_value - 1) % 26)); column_value = (column_value - 1) / 26; }
                    const double cell_value = evaluate(name + std::to_string(row), stack); samples.push_back(cell_value); total += cell_value; if (count == 0) minimum = maximum = cell_value; else { minimum = std::min(minimum, cell_value); maximum = std::max(maximum, cell_value); } ++count;
                }
            if (match[1] == "AVERAGE" && count) return total / count;
            if (match[1] == "COUNT") return static_cast<double>(count);
            if (match[1] == "MIN") return minimum;
            if (match[1] == "MAX") return maximum;
            if (match[1] == "MEDIAN" && count) { std::sort(samples.begin(), samples.end()); const size_t middle = samples.size() / 2; return samples.size() % 2 ? samples[middle] : (samples[middle - 1] + samples[middle]) / 2.0; }
            if (match[1] == "STDEV" && count) { const double average = total / count; double squared = 0; for (double value : samples) squared += (value - average) * (value - average); return std::sqrt(squared / count); }
            return total;
        }
        if (std::regex_match(expression, match, std::regex(R"(IF\(([^,]+),([^,]+),([^\)]+)\))"))) {
            std::smatch condition;
            const std::string condition_text = match[1].str();
            if (!std::regex_match(condition_text, condition, std::regex(R"(\s*([^<>=]+?)\s*(==|>=|<=|>|<)\s*([^<>=]+?)\s*)"))) throw std::runtime_error("invalid IF condition");
            const double left = evaluate(condition[1], stack), right = evaluate(condition[3], stack); const std::string operation = condition[2];
            bool result = operation == "==" ? left == right : operation == ">=" ? left >= right : operation == "<=" ? left <= right : operation == ">" ? left > right : left < right;
            return evaluate(result ? match[2].str() : match[3].str(), stack);
        }
        if (std::regex_match(expression, match, std::regex(R"((SUMIF|COUNTIF)\(([A-Z]+[0-9]+):([A-Z]+[0-9]+),\s*([^,]+),\s*([A-Z]+[0-9]+):([A-Z]+[0-9]+)\))"))) {
            const auto criteria_cells = range_references(match[2].str(), match[3].str());
            const auto result_cells = range_references(match[5].str(), match[6].str());
            if (criteria_cells.size() != result_cells.size()) throw std::runtime_error("conditional aggregate ranges must have equal sizes");
            std::string criteria = match[4].str(); const size_t first = criteria.find_first_not_of(" \t"); const size_t last = criteria.find_last_not_of(" \t");
            if (first != std::string::npos) criteria = criteria.substr(first, last - first + 1);
            if (criteria.size() >= 2 && criteria.front() == '"' && criteria.back() == '"') criteria = criteria.substr(1, criteria.size() - 2);
            std::string operation = "==", expected_text = criteria;
            for (const std::string& candidate : {std::string(">="), std::string("<="), std::string("=="), std::string(">"), std::string("<")}) if (criteria.rfind(candidate, 0) == 0) { operation = candidate; expected_text = criteria.substr(candidate.size()); break; }
            double expected = 0; try { expected = std::stod(expected_text); } catch (...) { expected = 0; }
            auto matches = [&](double value) { if (operation == ">=") return value >= expected; if (operation == "<=") return value <= expected; if (operation == ">") return value > expected; if (operation == "<") return value < expected; return value == expected; };
            double total = 0; int count = 0;
            for (size_t index = 0; index < criteria_cells.size(); ++index) { std::vector<std::string> nested; const double criterion_value = evaluate(criteria_cells[index], nested); if (matches(criterion_value)) { ++count; if (match[1] == "SUMIF") { nested.clear(); total += evaluate(result_cells[index], nested); } } }
            return match[1] == "COUNTIF" ? static_cast<double>(count) : total;
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
        if (std::regex_match(expression, match, std::regex(R"((SUM|AVERAGE|COUNT|MIN|MAX|MEDIAN|STDEV)\(([^()]*)\))"))) {
            std::stringstream arguments(match[2].str());
            std::string argument;
            std::vector<double> values;
            while (std::getline(arguments, argument, ',')) {
                std::smatch range_parts;
                if (std::regex_match(argument, range_parts, std::regex(R"(\s*([A-Z]+[0-9]+)\s*:\s*([A-Z]+[0-9]+)\s*)"))) {
                    std::smatch first_parts, last_parts;
                    const std::string first_reference = range_parts[1].str(), last_reference = range_parts[2].str();
                    std::regex_match(first_reference, first_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
                    std::regex_match(last_reference, last_parts, std::regex(R"(([A-Z]+)([0-9]+))"));
                    const int left = spreadsheet_column(first_parts[1]), right = spreadsheet_column(last_parts[1]);
                    const int top = std::stoi(first_parts[2]), bottom = std::stoi(last_parts[2]);
                    for (int row = std::min(top, bottom); row <= std::max(top, bottom); ++row)
                        for (int column = std::min(left, right); column <= std::max(left, right); ++column) {
                            int number = column; std::string name;
                            while (number) { name.insert(name.begin(), static_cast<char>('A' + (number - 1) % 26)); number = (number - 1) / 26; }
                            values.push_back(evaluate(name + std::to_string(row), stack));
                        }
                } else values.push_back(evaluate(argument, stack));
            }
            if (values.empty()) return 0;
            double total = 0;
            for (double value : values) total += value;
            if (match[1] == "SUM") return total;
            if (match[1] == "AVERAGE") return total / values.size();
            if (match[1] == "COUNT") return static_cast<double>(values.size());
            if (match[1] == "MIN") return *std::min_element(values.begin(), values.end());
            if (match[1] == "MAX") return *std::max_element(values.begin(), values.end());
            if (match[1] == "MEDIAN") { std::sort(values.begin(), values.end()); const size_t middle = values.size() / 2; return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0; }
            const double average = total / values.size(); double squared = 0; for (double value : values) squared += (value - average) * (value - average);
            return std::sqrt(squared / values.size());
        }
        if (std::regex_match(expression, match, std::regex(R"((ABS|SQRT|SIN|COS|TAN|EXP|LN|LOG2|LOG10|ROUND)\(([^()]*)\))"))) {
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
                return operation == '*' ? evaluate(expression.substr(0, index), stack) * right
                                        : evaluate(expression.substr(0, index), stack) / right;
            }
        }
        try { return std::stod(expression); } catch (...) { return 0; }
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
        std::string text; size_t position{0}; double variable{0}; double secondary{0};
        void skip() { while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) ++position; }
        bool match(char value) { skip(); if (position < text.size() && text[position] == value) { ++position; return true; } return false; }
        double primary() {
            skip();
            if (match('(')) { double value = add(); match(')'); return value; }
            if (position < text.size() && (std::isdigit(static_cast<unsigned char>(text[position])) || text[position] == '.')) {
                size_t end = position; while (end < text.size() && (std::isdigit(static_cast<unsigned char>(text[end])) || text[end] == '.')) ++end;
                double value = std::stod(text.substr(position, end - position)); position = end; return value;
            }
            if (position < text.size() && (std::isalpha(static_cast<unsigned char>(text[position])) || text[position] == '_')) {
                size_t end = position; while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_')) ++end;
                const std::string name = text.substr(position, end - position); position = end; skip();
                if (match('(')) { double first = add(); double second = 0; bool two = match(','); if (two) second = add(); match(')');
                    if (name == "sin") return std::sin(first); if (name == "cos") return std::cos(first); if (name == "tan") return std::tan(first);
                    if (name == "asin") return std::asin(first); if (name == "acos") return std::acos(first); if (name == "sqrt") return std::sqrt(first);
                    if (name == "atan") return std::atan(first); if (name == "exp") return std::exp(first); if (name == "log") return std::log(first); if (name == "log2") return std::log2(first); if (name == "log10") return std::log10(first);
                    if (name == "abs") return std::abs(first); if (name == "atan2") return std::atan2(first, second); if (name == "hypot") return std::hypot(first, second);
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
        expression = std::regex_replace(expression, std::regex(R"(\bx\b)"), x);
        if (!y.empty()) expression = std::regex_replace(expression, std::regex(R"(\by\b)"), y);
        replace_all(expression, "^", "**");
        replace_all(expression, "asin", "__MARILA_ASIN__");
        replace_all(expression, "acos", "__MARILA_ACOS__");
        replace_all(expression, "atan2", "__MARILA_ATAN2__");
        replace_all(expression, "atan", "__MARILA_ATAN__");
        replace_all(expression, "log2", "__MARILA_LOG2__");
        replace_all(expression, "log10", "__MARILA_LOG10__");
        replace_all(expression, "sin", "Math.sin");
        replace_all(expression, "cos", "Math.cos");
        replace_all(expression, "tan", "Math.tan");
        replace_all(expression, "sqrt", "Math.sqrt");
        replace_all(expression, "floor", "Math.floor");
        replace_all(expression, "ceil", "Math.ceil");
        replace_all(expression, "exp", "Math.exp");
        replace_all(expression, "log", "Math.log");
        replace_all(expression, "hypot", "Math.hypot");
        replace_all(expression, "abs", "Math.abs");
        replace_all(expression, "pi", "Math.PI");
        replace_all(expression, "__MARILA_ASIN__", "Math.asin");
        replace_all(expression, "__MARILA_ACOS__", "Math.acos");
        replace_all(expression, "__MARILA_ATAN__", "Math.atan");
        replace_all(expression, "__MARILA_ATAN2__", "Math.atan2");
        replace_all(expression, "__MARILA_LOG2__", "Math.log2");
        replace_all(expression, "__MARILA_LOG10__", "Math.log10");
        replace_all(expression, "pow", "Math.pow");
        replace_all(expression, "sign", "Math.sign");
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
            std::ostringstream item; item << "{type:'move',x:" << node.from_x << ",y:" << node.from_y << ",tx:" << node.x << ",ty:" << node.y << ",duration:" << node.duration << ",easing:" << std::quoted(node.easing) << ",label:" << std::quoted(node.id) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Fade) {
            std::ostringstream item; item << "{type:'fade',target:" << std::quoted(node.first) << ",fadeIn:" << (node.fade_in ? "true" : "false") << ",duration:" << node.duration << ",easing:" << std::quoted(node.easing) << "}";
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
                << "',fromSize:" << node.x << ",toSize:" << node.y << ",duration:" << node.duration << ",easing:" << std::quoted(node.easing) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Sphere) {
            std::ostringstream item; item << "{type:'sphere',x:" << node.x << ",y:" << node.y << ",z:" << node.z << ",r:" << node.radius << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Cube) {
            std::ostringstream item; item << "{type:'cube',x:" << node.x << ",y:" << node.y << ",z:" << node.z << ",s:" << node.size << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Plot4D) {
            animations.push_back("{type:'4d'}");
        } else if (node.kind == SceneNode::Kind::Data2D) {
            std::ostringstream item; item << "{type:'data2d',points:[";
            for (size_t index = 0; index < node.samples.size(); ++index) { if (index) item << ","; item << "[" << node.samples[index].first << "," << node.samples[index].second << "]"; }
            item << "],title:" << std::quoted(node.title) << ",color:" << std::quoted(node.color) << "}";
            animations.push_back(item.str());
        } else if (node.kind == SceneNode::Kind::Data3D) {
            std::ostringstream item; item << "{type:'data3d',grid:[";
            for (size_t row = 0; row < node.grid.size(); ++row) { if (row) item << ","; item << "["; for (size_t column = 0; column < node.grid[row].size(); ++column) { if (column) item << ","; item << node.grid[row][column]; } item << "]"; }
            item << "],title:" << std::quoted(node.title) << ",color:" << std::quoted(node.color) << "}";
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
                 << ",xmin:" << node.xmin << ",xmax:" << node.xmax << ",ymin:-6,ymax:6}";
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
                 << ",trace:" << (node.animate_trace ? "true" : "false") << "}";
            animations.push_back(item.str());
        }
    }
    std::string data = "[";
    for (size_t i = 0; i < animations.size(); ++i) { if (i) data += ","; data += animations[i]; }
    data += "]";
    std::stringstream titles;
    for (const auto& node : nodes) if (!node.title.empty()) titles << "<span class=\"plot-title\">" << html_escape(node.title) << "</span>";
    return "<!doctype html><meta charset=\"utf-8\"><title>Marila scene</title>"
           "<style>body{margin:0;background:#040711;color:white;font:16px sans-serif;text-align:center}"
           "canvas{max-width:95vw;border:1px solid #334155}.plot-title{display:inline-block;margin:8px;padding:6px 10px;border:1px solid #475569;border-radius:6px}.controls{display:flex;gap:8px;justify-content:center;align-items:center;margin:12px}.controls button{background:#1e293b;color:white;border:1px solid #475569;border-radius:5px;padding:6px 12px}.controls input{width:min(520px,60vw)}</style><h2>Marila native scene</h2>" + titles.str() +
           "<div class=\"controls\"><button id=\"play\">Pause</button><button id=\"reset\">Reset</button><input id=\"timeline\" type=\"range\" min=\"0\" max=\"1\" step=\"0.001\" value=\"0\" aria-label=\"Animation timeline\"></div><canvas id=\"scene\" width=\"900\" height=\"500\"></canvas><script>"
           "const c=document.getElementById('scene'),x=c.getContext('2d'),a=" + data + ";let t=0,playing=true,yaw=.6,pitch=.4,drag=false,lx=0,ly=0;const timeline=document.getElementById('timeline'),play=document.getElementById('play'),reset=document.getElementById('reset'),maxDuration=Math.max(1,...a.map(q=>q.duration||0));const ease=(p,m)=>m==='in'?p*p:m==='out'?1-(1-p)*(1-p):m==='in-out'?p<.5?2*p*p:1-Math.pow(-2*p+2,2)/2:p;const opacityFor=id=>{let value=1;for(const q of a)if(q.type==='fade'&&q.target===id){let p=ease(Math.min(1,Math.max(0,t)/q.duration),q.easing);value=q.fadeIn?p:1-p}return value};play.onclick=()=>{playing=!playing;play.textContent=playing?'Pause':'Play'};reset.onclick=()=>{t=0;timeline.value=0};timeline.oninput=()=>{t=Number(timeline.value)*maxDuration};c.onpointerdown=e=>{drag=true;lx=e.clientX;ly=e.clientY};c.onpointerup=()=>drag=false;c.onpointerleave=()=>drag=false;c.onpointermove=e=>{if(drag){yaw+=(e.clientX-lx)*.01;pitch+=(e.clientY-ly)*.01;pitch=Math.max(-1.4,Math.min(1.4,pitch));lx=e.clientX;ly=e.clientY}};"
           "function frame(){x.fillStyle='#040711';x.fillRect(0,0,c.width,c.height);"
           "x.strokeStyle='rgba(255,255,255,.08)';for(let i=0;i<900;i+=40){x.beginPath();x.moveTo(i,0);x.lineTo(i,500);x.stroke()}"
           "for(let i=0;i<500;i+=40){x.beginPath();x.moveTo(0,i);x.lineTo(900,i);x.stroke()}"
           "const cx=450,cy=250;function proj(xv,yv,zv){let cyaw=Math.cos(yaw),syaw=Math.sin(yaw),cp=Math.cos(pitch),sp=Math.sin(pitch),rx=xv*cyaw-zv*syaw,rz=xv*syaw+zv*cyaw,ry=yv*cp-rz*sp,rz2=yv*sp+rz*cp,s=1/(1+rz2/10);return[cx+(rx-ry)*28*s,cy+(rx+ry)*14*s-rz2*20*s]}function point2(xv,yv){return[cx+xv*2,cy-yv*2]}for(const q of a){if(q.type==='point'){x.globalAlpha=opacityFor(q.id);let p=point2(q.x,q.y);x.fillStyle='#22d3ee';x.beginPath();x.arc(p[0],p[1],5,0,Math.PI*2);x.fill();x.fillStyle='white';x.fillText(q.label,p[0]+8,p[1]-8);x.globalAlpha=1}"
           "else if(q.type==='move'){x.globalAlpha=opacityFor(q.label);let p=ease(Math.min(1,(t%q.duration)/q.duration),q.easing);let mx=q.x+(q.tx-q.x)*p,my=q.y+(q.ty-q.y)*p,pp=point2(mx,my);x.fillStyle='#f59e0b';x.beginPath();x.arc(pp[0],pp[1],6,0,Math.PI*2);x.fill();x.fillStyle='white';x.fillText(q.label,pp[0]+8,pp[1]-8);x.globalAlpha=1}"
           "else if(q.type==='line'){let p=point2(q.x1,q.y1),r=point2(q.x2,q.y2);x.strokeStyle='#6366f1';x.lineWidth=3;x.beginPath();x.moveTo(...p);x.lineTo(...r);x.stroke()}"
           "else if(q.type==='triangle'){let p=point2(q.x1,q.y1),r=point2(q.x2,q.y2),s=point2(q.x3,q.y3);x.strokeStyle=q.color||'#f59e0b';x.fillStyle=(q.color||'#f59e0b')+'33';x.beginPath();x.moveTo(...p);x.lineTo(...r);x.lineTo(...s);x.closePath();x.fill();x.stroke()}"
           "else if(q.type==='circle'){let p=point2(q.x,q.y);x.strokeStyle='#22d3ee';x.setLineDash(q.dashed?[6,4]:[]);x.beginPath();x.arc(p[0],p[1],q.r*2,0,Math.PI*2);x.stroke();x.setLineDash([])}"
           "else if(q.type==='4d'){x.strokeStyle='#a855f7';for(let i=0;i<16;i++)for(let b of[1,2,4,8]){let j=i^b;if(i<j){let p=u(i),r=u(j);x.beginPath();x.moveTo(p[0],p[1]);x.lineTo(r[0],r[1]);x.stroke()}}}"
           "else if(q.type==='data3d'){let rows=q.grid.length,cols=rows?q.grid[0].length:0;x.strokeStyle=q.color||'#a855f7';for(let r=0;r<rows;r++){x.beginPath();for(let j=0;j<cols;j++){let p=proj(j-(cols-1)/2,(rows-1)/2-r,q.grid[r][j]);if(j===0)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}for(let j=0;j<cols;j++){x.beginPath();for(let r=0;r<rows;r++){let p=proj(j-(cols-1)/2,(rows-1)/2-r,q.grid[r][j]);if(r===0)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}}"
           "else if(q.type==='3d'){let f=new Function('u','v','return '+q.expr),sx=300/(q.xmax-q.xmin),sy=300/(q.ymax-q.ymin),mx=(q.xmin+q.xmax)/2,my=(q.ymin+q.ymax)/2;x.strokeStyle=q.color||'#10b981';for(let i=q.xmin;i<=q.xmax;i+=(q.xmax-q.xmin)/25){x.beginPath();for(let j=q.ymin;j<=q.ymax;j+=(q.ymax-q.ymin)/25){let z=Number(f(i,j));if(!Number.isFinite(z))continue;let p=proj((i-mx)*sx/28,(j-my)*sy/28,z);if(j===q.ymin)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}}"
           "else if(q.type==='data2d'){x.strokeStyle=q.color||'#f59e0b';x.beginPath();for(let i=0;i<q.points.length;i++){let p=point2(q.points[i][0],q.points[i][1]);if(i===0)x.moveTo(...p);else x.lineTo(...p)}x.stroke();for(let p of q.points){let v=point2(p[0],p[1]);x.fillStyle=q.color||'#f59e0b';x.beginPath();x.arc(v[0],v[1],4,0,Math.PI*2);x.fill()}}"
           "else if(q.type==='vector2d'){let fx=new Function('u','v','return '+q.exprx),fy=new Function('u','v','return '+q.expry),sx=780/(q.xmax-q.xmin),sy=220/(q.ymax-q.ymin),mx=(q.xmin+q.xmax)/2,my=(q.ymin+q.ymax)/2,px=v=>cx+(v-mx)*sx,py=v=>cy-(v-my)*sy;x.strokeStyle=q.color||'#22d3ee';for(let i=0;i<=10;i++)for(let j=0;j<=10;j++){let u=q.xmin+(q.xmax-q.xmin)*j/10,v=q.ymin+(q.ymax-q.ymin)*i/10,dx=Number(fx(u,v)),dy=Number(fy(u,v));if(!Number.isFinite(dx)||!Number.isFinite(dy))continue;let p=point2(u,v),r=point2(u+dx*.18,v+dy*.18);x.beginPath();x.moveTo(...p);x.lineTo(...r);x.stroke()}}"
           "else if(q.type==='2d'){let f=new Function('u','return '+q.expr),sx=780/(q.xmax-q.xmin),sy=220/(q.ymax-q.ymin),mx=(q.xmin+q.xmax)/2,my=(q.ymin+q.ymax)/2,px=v=>cx+(v-mx)*sx,py=v=>cy-(v-my)*sy;x.strokeStyle='rgba(255,255,255,.35)';if(q.xmin<=0&&q.xmax>=0){x.beginPath();x.moveTo(px(0),0);x.lineTo(px(0),500);x.stroke()}if(q.ymin<=0&&q.ymax>=0){x.beginPath();x.moveTo(0,py(0));x.lineTo(900,py(0));x.stroke()}x.strokeStyle=q.color||'#06b6d4';x.beginPath();let first=true,progress=q.trace?Math.min(1,(t%4)/2):1;if(q.trace&&progress>1)progress=2-progress;let end=q.xmin+(q.xmax-q.xmin)*progress;for(let i=q.xmin;i<=end;i+=(q.xmax-q.xmin)/400){let y=Number(f(i));if(!Number.isFinite(y)){first=true;continue}let p=[px(i),py(y)];if(first){x.moveTo(...p);first=false}else x.lineTo(...p)}x.stroke()}"
           "else if(q.type==='param2d'){let fx=new Function('u','return '+q.exprx),fy=new Function('u','return '+q.expry),sx=780/(q.xmax-q.xmin),sy=220/(q.ymax-q.ymin),px=v=>cx+(v+6)*55,py=v=>cy-v*sy;x.strokeStyle=q.color||'#f59e0b';x.beginPath();for(let i=q.xmin;i<=q.xmax;i+=(q.xmax-q.xmin)/400){let pxv=Number(fx(i)),pyv=Number(fy(i));if(!Number.isFinite(pxv)||!Number.isFinite(pyv))continue;let p=[pxv*55+cx,py(pyv)];if(i===q.xmin)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}"
           "else if(q.type==='implicit2d'){let f=new Function('u','v','return '+q.expr),sx=780/(q.xmax-q.xmin),sy=220/(q.ymax-q.ymin),mx=(q.xmin+q.xmax)/2,my=(q.ymin+q.ymax)/2;x.fillStyle=q.color||'#fb7185';for(let i=q.xmin;i<=q.xmax;i+=(q.xmax-q.xmin)/180)for(let j=q.ymin;j<=q.ymax;j+=(q.ymax-q.ymin)/180){let value=Number(f(i,j));if(Number.isFinite(value)&&Math.abs(value)<.08){let px=cx+(i-mx)*sx,py=cy-(j-my)*sy;x.fillRect(px,py,3,3)}}}"
           "else if(q.type==='morph'){let p=ease(Math.min(1,(t%q.duration)/q.duration),q.easing),s=q.fromSize+(q.toSize-q.fromSize)*p;x.strokeStyle='#f59e0b';x.lineWidth=4;if(q.from==='square'&&q.to==='circle'){x.beginPath();x.roundRect(cx-s/2,cy-s/2,s,s,s*p);x.stroke()}else if(q.from==='circle'&&q.to==='square'){x.beginPath();x.roundRect(cx-s/2,cy-s/2,s,s,s*(1-p));x.stroke()}else if(q.to==='circle'){x.beginPath();x.arc(cx,cy,s/2,0,Math.PI*2);x.stroke()}else{x.strokeRect(cx-s/2,cy-s/2,s,s)}}"
           "else if(q.type==='sphere'){let p=proj(q.x,q.y,q.z),r=Math.max(2,q.r*18/(q.z+8));x.strokeStyle='#38bdf8';x.beginPath();x.arc(p[0],p[1],r,0,Math.PI*2);x.stroke()}"
           "else if(q.type==='cube'){let s=q.s/2,pts=[[-s,-s,-s],[s,-s,-s],[s,s,-s],[-s,s,-s],[-s,-s,s],[s,-s,s],[s,s,s],[-s,s,s]].map(v=>proj(q.x+v[0],q.y+v[1],q.z+v[2]));x.strokeStyle='#f97316';for(let e of[[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]]){x.beginPath();x.moveTo(...pts[e[0]]);x.lineTo(...pts[e[1]]);x.stroke()}}}if(playing)t+=.02;if(t>maxDuration)t=0;timeline.value=t/maxDuration;requestAnimationFrame(frame)}"
           "function u(i){let p=[i&1?1:-1,i&2?1:-1,i&4?1:-1,i&8?1:-1],s=1/(4.5-p[3]);return[cx+p[0]*80*s,cy-p[1]*80*s]}frame();</script>";
}

}  // namespace

std::string detect_file_type(const std::string& source) {
    std::smatch match;
    if (std::regex_search(source, match, std::regex(R"(file\s+type\s+([A-Za-z]+))"))) return match[1];
    return "program";
}

void render_source(const std::string& source, const std::string& output) {
    const std::string type = detect_file_type(source);
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
    while (std::getline(input, line)) {
        const size_t first = line.find_first_not_of(" \t");
        const std::string trimmed = first == std::string::npos ? "" : line.substr(first);
        if (trimmed.empty()) { output << '\n'; continue; }
        if (!trimmed.empty() && (trimmed[0] == '}' || trimmed[0] == ']')) indent = std::max(0, indent - 1);
        output << std::string(static_cast<size_t>(indent) * 2, ' ') << trimmed << '\n';
        if (trimmed.back() == '{' || trimmed.back() == '[') ++indent;
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
    bool string = false, escaped = false;
    for (char character : source) {
        if (string) {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == '"') string = false;
            continue;
        }
        if (character == '"') { string = true; continue; }
        if (character == '[') ++square; else if (character == ']') --square;
        else if (character == '{') ++curly; else if (character == '}') --curly;
        else if (character == '(') ++parentheses; else if (character == ')') --parentheses;
        if (square < 0 || curly < 0 || parentheses < 0) { std::cerr << "error: unmatched closing delimiter\n"; return 1; }
    }
    if (string || square || curly || parentheses) { std::cerr << "error: unbalanced source delimiters\n"; return 1; }
    if (type != "program" && source.find("render") == std::string::npos)
        std::cerr << "warning: renderer source has no render directive\n";
    if (type == "scene" && source.find("plot") == std::string::npos && source.find("morph") == std::string::npos)
        std::cerr << "warning: scene contains no plot or animation primitive\n";
    return 0;
}

}  // namespace marila
