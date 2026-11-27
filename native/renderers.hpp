#pragma once

#include <string>
#include <vector>

namespace marila {

std::string detect_file_type(const std::string& source);
void render_source(const std::string& source, const std::string& output);
std::string render_markdown(const std::string& source);
void export_obj(const std::string& source, const std::string& output);
void export_stl(const std::string& source, const std::string& output);
void export_ply(const std::string& source, const std::string& output);
void export_video(const std::string& source, const std::string& format, const std::string& output);
void export_gltf(const std::string& source, const std::string& output);
void export_pdf(const std::string& source, const std::string& output);
void export_svg(const std::string& source, const std::string& output);
void export_png(const std::string& source, const std::string& output);
std::string sheet_json(const std::string& source);
std::string sheet_delimited(const std::string& source, char separator);
std::string text_source_json(const std::string& type, const std::string& source);
std::string format_source(const std::string& source);
int lint_source(const std::string& source);
std::vector<std::string> validate_visual_source(const std::string& source);

}  // namespace marila
