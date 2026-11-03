#include "renderers.hpp"

#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
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

std::string render_document(const std::string& source) {
    std::stringstream body;
    std::regex item(R"MARILA((\w+)\s*-\s*"([^"]*)"(?:\s*::\s*"([^"]*)")?)MARILA");
    for (std::sregex_iterator it(source.begin(), source.end(), item), end; it != end; ++it) {
        const std::string id = (*it)[1], text = html_escape((*it)[2]), subtitle = html_escape((*it)[3]);
        if (id.rfind("theorem", 0) == 0 || id.rfind("definition", 0) == 0 ||
            id.rfind("proof", 0) == 0 || id.rfind("prop", 0) == 0) {
            body << "<section class=\"callout\"><h3>" << html_escape(id) << "</h3><p>" << text << "</p></section>\n";
        } else if (!subtitle.empty()) {
            body << "<header><h1>" << text << "</h1><p>" << subtitle << "</p></header>\n";
        } else {
            body << "<p>" << text << "</p>\n";
        }
    }
    return "<!doctype html><meta charset=\"utf-8\"><title>Marila document</title>"
           "<style>body{max-width:800px;margin:40px auto;font:18px Georgia;line-height:1.6}"
           ".callout{border-left:4px solid #6366f1;background:#f1f5f9;padding:12px;margin:20px 0}"
           "header{border-bottom:2px solid #334155}</style><main>" + body.str() + "</main>";
}

std::string render_image(const std::string& source) {
    struct Point { std::string id; double x; double y; };
    std::vector<Point> points;
    std::stringstream elements;
    std::regex point(R"(point\s+([A-Za-z_]\w*)\s+at\s*\(\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*\))");
    for (std::sregex_iterator it(source.begin(), source.end(), point), end; it != end; ++it) {
        points.push_back({(*it)[1], std::stod((*it)[2]), std::stod((*it)[3])});
        elements << "<circle cx=\"" << (*it)[2] << "\" cy=\"" << -std::stod((*it)[3])
                  << "\" r=\"4\" fill=\"#06b6d4\"/><text x=\"" << std::stod((*it)[2]) + 8
                  << "\" y=\"" << -std::stod((*it)[3]) - 8 << "\" fill=\"white\">" << (*it)[1] << "</text>\n";
    }
    std::regex line(R"(line\s+from\s+([A-Za-z_]\w*)\s+to\s+([A-Za-z_]\w*))");
    for (std::sregex_iterator it(source.begin(), source.end(), line), end; it != end; ++it) {
        auto find = [&](const std::string& id) { for (const auto& point : points) if (point.id == id) return point; return Point{id, 0, 0}; };
        Point first = find((*it)[1]), second = find((*it)[2]);
        elements << "<line x1=\"" << first.x << "\" y1=\"" << -first.y << "\" x2=\"" << second.x
                  << "\" y2=\"" << -second.y << "\" stroke=\"#6366f1\" stroke-width=\"3\"/>\n";
    }
    std::regex triangle(R"(triangle\s+([A-Za-z_])([A-Za-z_])([A-Za-z_]))");
    for (std::sregex_iterator it(source.begin(), source.end(), triangle), end; it != end; ++it) {
        auto find = [&](const std::string& id) {
            for (const auto& point : points) if (point.id == id) return point;
            return Point{id, 0, 0};
        };
        Point first = find((*it)[1]), second = find((*it)[2]), third = find((*it)[3]);
        elements << "<polygon points=\"" << first.x << "," << -first.y << " "
                  << second.x << "," << -second.y << " "
                  << third.x << "," << -third.y
                  << "\" fill=\"rgba(245,158,11,.2)\" stroke=\"#f59e0b\" stroke-width=\"2\"/>\n";
    }
    std::regex circle(R"(circle(?:\s+at)?\s+([A-Za-z_]\w*)\s+radius\s+(-?[0-9.]+))");
    for (std::sregex_iterator it(source.begin(), source.end(), circle), end; it != end; ++it) {
        auto find = [&](const std::string& id) {
            for (const auto& point : points) if (point.id == id) return point;
            return Point{id, 0, 0};
        };
        Point center = find((*it)[1]);
        elements << "<circle cx=\"" << center.x << "\" cy=\"" << -center.y
                  << "\" r=\"" << (*it)[2] << "\" fill=\"none\" stroke=\"#22d3ee\" stroke-width=\"2\"/>\n";
    }
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-200 -200 400 400\" style=\"background:#0f172a\">"
           "<g stroke=\"rgba(255,255,255,.12)\"><path d=\"M-200 0H200M0-200V200\"/></g>" +
           elements.str() + "</svg>";
}

std::string render_scene(const std::string& source) {
    std::vector<std::string> animations;
    if (source.find("plot4d") != std::string::npos) animations.push_back("{type:'4d'}");
    if (source.find("plot3d") != std::string::npos) animations.push_back("{type:'3d'}");
    if (source.find("plot2d") != std::string::npos || source.find("plot ") != std::string::npos) animations.push_back("{type:'2d'}");
    if (source.find("morphs") != std::string::npos) animations.push_back("{type:'morph'}");
    std::string data = "[";
    for (size_t i = 0; i < animations.size(); ++i) { if (i) data += ","; data += animations[i]; }
    data += "]";
    return "<!doctype html><meta charset=\"utf-8\"><title>Marila scene</title>"
           "<style>body{margin:0;background:#040711;color:white;font:16px sans-serif;text-align:center}"
           "canvas{max-width:95vw;border:1px solid #334155}</style><h2>Marila native scene</h2>"
           "<canvas id=\"scene\" width=\"900\" height=\"500\"></canvas><script>"
           "const c=document.getElementById('scene'),x=c.getContext('2d'),a=" + data + ";let t=0;"
           "function frame(){x.fillStyle='#040711';x.fillRect(0,0,c.width,c.height);"
           "x.strokeStyle='rgba(255,255,255,.08)';for(let i=0;i<900;i+=40){x.beginPath();x.moveTo(i,0);x.lineTo(i,500);x.stroke()}"
           "for(let i=0;i<500;i+=40){x.beginPath();x.moveTo(0,i);x.lineTo(900,i);x.stroke()}"
           "const cx=450,cy=250;for(const q of a){if(q.type==='4d'){x.strokeStyle='#a855f7';for(let i=0;i<16;i++)for(let b of[1,2,4,8]){let j=i^b;if(i<j){let p=u(i),r=u(j);x.beginPath();x.moveTo(p[0],p[1]);x.lineTo(r[0],r[1]);x.stroke()}}}"
           "else if(q.type==='3d'){x.strokeStyle='#10b981';for(let i=-5;i<5;i+=.4){x.beginPath();for(let j=-5;j<5;j+=.4){let z=Math.sin(i)*Math.cos(j);let p=[cx+i*35,cy-j*25-z*25];if(j===-5)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}}"
           "else if(q.type==='2d'){x.strokeStyle='#06b6d4';x.beginPath();for(let i=-6;i<6;i+=.05){let p=[cx+i*55,cy-Math.sin(i)*55];if(i===-6)x.moveTo(...p);else x.lineTo(...p)}x.stroke()}"
           "else{x.strokeStyle='#6366f1';x.strokeRect(cx-70,cy-70,140,140)}}t+=.02;requestAnimationFrame(frame)}"
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
    if (source.find("file type") == std::string::npos) {
        std::cerr << "warning: source has no explicit file type\n";
        return 0;
    }
    return 0;
}

}  // namespace marila
