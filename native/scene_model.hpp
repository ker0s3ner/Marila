#pragma once

#include <string>
#include <utility>
#include <vector>

namespace marila {

// Remove visual-language comments while preserving line positions for diagnostics.
std::string strip_visual_comments(const std::string& source);

struct SceneNode {
    enum class Kind { Point, Move, Fade, Wait, Line, Triangle, Circle, Plot2D, Data2D, Parametric2D, Parametric4D, VectorField2D, Implicit2D, Plot3D, Data3D, Plot4D, Sphere, Cube, Morph };
    Kind kind;
    std::string id;
    std::string first;
    std::string second;
    std::string third;
    std::string fourth;
    std::string label;
    std::string easing{"linear"};
    std::string title;
    std::string color;
    std::string expression;
    double x{0};
    double y{0};
    double from_x{0};
    double from_y{0};
    double z{0};
    double radius{0};
    double size{0};
    double xmin{-6};
    double xmax{6};
    double ymin{-6};
    double ymax{6};
    double duration{2};
    double start_time{0};
    int repeat{1};
    bool yoyo{false};
    bool dashed{false};
    bool fade_in{true};
    bool show_angles{false};
    bool animate_trace{false};
    std::vector<std::pair<double, double>> samples;
    std::vector<std::vector<double>> grid;

    explicit SceneNode(Kind node_kind) : kind(node_kind) {}
};

std::vector<SceneNode> parse_visual_source(const std::string& source);

}  // namespace marila
