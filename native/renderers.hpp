#pragma once

#include <string>

namespace marila {

std::string detect_file_type(const std::string& source);
void render_source(const std::string& source, const std::string& output);
std::string format_source(const std::string& source);
int lint_source(const std::string& source);

}  // namespace marila
