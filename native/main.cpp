#include <cmath>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "renderers.hpp"
#include "scene_model.hpp"

namespace marila {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class Kind { End, Newline, Number, String, Identifier, Operator, Symbol };
struct Token { Kind kind; std::string text; double number{}; int line{}; int column{}; };

class Lexer {
public:
    explicit Lexer(std::string source) : source_(std::move(source)) {}

    std::vector<Token> tokenize() {
        std::vector<Token> result;
        while (position_ < source_.size()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r') { advance(); continue; }
            if (c == '\n') { result.push_back({Kind::Newline, "\\n", 0, line_, column_}); advance(); continue; }
            if (c == '#' || (c == '/' && peek(1) == '/') || (c == '%' && at_line_start_ignoring_space())) {
                while (position_ < source_.size() && peek() != '\n') advance();
                continue;
            }
            if (c == '"' || c == '\'') { result.push_back(read_string(c)); continue; }
            if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))) {
                result.push_back(read_number()); continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                result.push_back(read_identifier()); continue;
            }
            int line = line_, column = column_;
            std::string pair{c, peek(1)};
            if (pair == "==" || pair == "!=" || pair == "<=" || pair == ">=" ||
                pair == "=>" || pair == ".*" || pair == "./" || pair == ".^" ||
                pair == "&&" || pair == "||" || pair == "::") {
                advance(); advance(); result.push_back({Kind::Operator, pair, 0, line, column}); continue;
            }
            if (std::string("+-*/%^=<>!@&").find(c) != std::string::npos) {
                advance(); result.push_back({Kind::Operator, std::string(1, c), 0, line, column}); continue;
            }
            if (std::string("()[]{};,.:").find(c) != std::string::npos) {
                advance(); result.push_back({Kind::Symbol, std::string(1, c), 0, line, column}); continue;
            }
            throw Error(location() + "unexpected character '" + std::string(1, c) + "'");
        }
        result.push_back({Kind::End, "", 0, line_, column_});
        return result;
    }

private:
    char peek(size_t offset = 0) const {
        return position_ + offset < source_.size() ? source_[position_ + offset] : '\0';
    }
    void advance() {
        if (peek() == '\n') { ++line_; column_ = 1; } else { ++column_; }
        ++position_;
    }
    bool at_line_start_ignoring_space() const {
        size_t line_start = source_.rfind('\n', position_ == 0 ? 0 : position_ - 1);
        line_start = line_start == std::string::npos ? 0 : line_start + 1;
        for (size_t index = line_start; index < position_; ++index)
            if (source_[index] != ' ' && source_[index] != '\t' && source_[index] != '\r') return false;
        return true;
    }
    std::string location() const { return "line " + std::to_string(line_) + ", column " + std::to_string(column_) + ": "; }
    Token read_string(char quote) {
        int line = line_, column = column_; advance();
        std::string value;
        while (peek() && peek() != quote) {
            if (peek() == '\\') {
                advance(); char escaped = peek(); advance();
                value += escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
            } else { value += peek(); advance(); }
        }
        if (!peek()) throw Error("line " + std::to_string(line) + ": unterminated string");
        advance(); return {Kind::String, value, 0, line, column};
    }
    Token read_number() {
        int line = line_, column = column_; std::string value; bool dot = false;
        while (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.') {
            if (peek() == '.' && dot) throw Error(location() + "invalid number");
            dot |= peek() == '.'; value += peek(); advance();
        }
        return {Kind::Number, value, std::stod(value), line, column};
    }
    Token read_identifier() {
        int line = line_, column = column_; std::string value;
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') { value += peek(); advance(); }
        return {Kind::Identifier, value, 0, line, column};
    }
    std::string source_; size_t position_{0}; int line_{1}; int column_{1};
};

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;
struct Expr {
    struct Literal { std::variant<double, std::string, bool, std::nullptr_t> value; };
    struct Name { std::string value; };
    struct Array { std::vector<ExprPtr> values; };
    struct Object { std::vector<std::pair<std::string, ExprPtr>> values; };
    struct Unary { std::string op; ExprPtr value; };
    struct Binary { ExprPtr left; std::string op; ExprPtr right; };
    struct Call { ExprPtr callee; std::vector<ExprPtr> args; };
    struct Index { ExprPtr collection; ExprPtr index; };
    struct Range { ExprPtr start; ExprPtr step; ExprPtr stop; };
    struct Lambda { std::vector<std::string> parameters; ExprPtr body; };
    std::variant<Literal, Name, Array, Object, Unary, Binary, Call, Index, Range, Lambda> value;
    template <typename T>
    explicit Expr(T node) : value(std::move(node)) {}
};

struct Statement {
    enum class Kind { Let, Assign, AssignIndex, Print, Expression, If, While, For, Function, Return, Import, Break, Continue, Try, Throw };
    Kind kind;
    std::string name;
    std::string type;
    ExprPtr expression;
    std::vector<std::string> parameters;
    std::vector<std::string> parameter_types;
    std::vector<Statement> body;
    std::vector<Statement> alternate;
    std::string module;
    ExprPtr target;

    Statement(Kind statement_kind) : kind(statement_kind) {}
    Statement(Kind statement_kind, std::string statement_name, ExprPtr statement_expression)
        : kind(statement_kind), name(std::move(statement_name)), expression(std::move(statement_expression)) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    std::vector<Statement> parse() {
        skip_lines();
        if (match("file")) {
            expect("type");
            if (!check_kind(Kind::Identifier)) {
                throw Error("line " + std::to_string(current().line) + ": expected file type");
            }
            advance();
            expect(":");
            skip_lines();
        }
        std::vector<Statement> statements = match("[") ? block("]") : block("");
        if (match("]")) {}
        if (match("render")) advance();
        return statements;
    }

private:
    const Token& current() const { return tokens_[position_]; }
    bool check(const std::string& value) const { return current().text == value; }
    bool check_kind(Kind kind) const { return current().kind == kind; }
    bool match(const std::string& value) { if (check(value)) { ++position_; return true; } return false; }
    void expect(const std::string& value) { if (!match(value)) throw Error("line " + std::to_string(current().line) + ": expected '" + value + "'"); }
    void advance() { if (!check_kind(Kind::End)) ++position_; }
    void skip_lines() { while (check_kind(Kind::Newline) || check(";")) advance(); }

    Statement statement() {
        if (match("let")) {
            Statement result{Statement::Kind::Let}; result.name = identifier();
            if (match(":")) result.type = identifier();
            expect("="); result.expression = expression(); return result;
        }
        if (match("print")) return {Statement::Kind::Print, "", expression()};
        if (match("return")) {
            if (check_kind(Kind::Newline) || check("}") || check("]") || check_kind(Kind::End))
                return {Statement::Kind::Return};
            return {Statement::Kind::Return, "", expression()};
        }
        if (match("break")) return {Statement::Kind::Break};
        if (match("continue")) return {Statement::Kind::Continue};
        if (match("throw")) return {Statement::Kind::Throw, "", expression()};
        if (match("import")) {
            Token module = current(); advance();
            if (module.kind != Kind::Identifier && module.kind != Kind::String)
                throw Error("line " + std::to_string(module.line) + ": expected module name");
            Statement result{Statement::Kind::Import}; result.module = module.text; return result;
        }
        if (match("if")) {
            Statement result{Statement::Kind::If}; result.expression = expression(); result.body = required_block();
            skip_lines();
            if (match("else")) {
                skip_lines();
                if (check("if")) result.alternate.push_back(statement());
                else result.alternate = required_block();
            }
            return result;
        }
        if (match("try")) {
            Statement result{Statement::Kind::Try}; result.body = required_block(); skip_lines(); expect("catch");
            if (match("(")) { result.name = identifier(); expect(")"); }
            else if (check_kind(Kind::Identifier)) result.name = identifier();
            result.alternate = required_block(); return result;
        }
        if (match("while")) {
            Statement result{Statement::Kind::While}; result.expression = expression(); result.body = required_block(); return result;
        }
        if (match("for")) {
            Statement result{Statement::Kind::For}; result.name = identifier(); expect("in");
            result.expression = expression(); result.body = required_block(); return result;
        }
        if (match("def") || match("function")) {
            Statement result{Statement::Kind::Function}; result.name = identifier(); expect("(");
            while (!check(")")) {
                result.parameters.push_back(identifier()); result.parameter_types.push_back(match(":") ? identifier() : "");
                if (!match(",")) break;
            }
            expect(")"); if (match(":")) result.type = identifier(); result.body = required_block(); return result;
        }
        ExprPtr left = expression();
        if (auto name = std::get_if<Expr::Name>(&left->value); name && match("="))
            return {Statement::Kind::Assign, name->value, expression()};
        if (std::holds_alternative<Expr::Index>(left->value) && match("=")) {
            Statement result{Statement::Kind::AssignIndex};
            result.target = left; result.expression = expression(); return result;
        }
        return {Statement::Kind::Expression, "", left};
    }
    std::string identifier() {
        if (!check_kind(Kind::Identifier)) throw Error("line " + std::to_string(current().line) + ": expected identifier");
        std::string value = current().text; advance(); return value;
    }
    ExprPtr expression(int minimum = 0) {
        ExprPtr left = prefix();
        while (true) {
            if (check(":") && minimum == 0) {
                advance();
                ExprPtr middle = expression(1);
                if (match(":")) {
                    left = std::make_shared<Expr>(Expr::Range{left, middle, expression(1)});
                } else {
                    left = std::make_shared<Expr>(Expr::Range{left, std::make_shared<Expr>(Expr::Literal{1.0}), middle});
                }
                continue;
            }
            static const std::unordered_map<std::string, int> precedence{
                {"||",1},{"or",1},{"&&",2},{"and",2},{"==",3},{"!=",3},{"<",4},{"<=",4},
                {">",4},{">=",4},{"+",5},{"-",5},{"*",6},{"/",6},{"%",6},{".*",6},{"./",6},
                {".^",7},{"^",7},{"@",6}};
            auto it = precedence.find(current().text);
            if (it == precedence.end() || it->second < minimum) break;
            std::string op = current().text; advance();
            left = std::make_shared<Expr>(Expr::Binary{left, op, expression(it->second + (op == "^" ? 0 : 1))});
        }
        return left;
    }
    ExprPtr prefix() {
        if (check("-") || check("!") || check("not")) {
            std::string op = current().text; advance();
            return std::make_shared<Expr>(Expr::Unary{op, expression(7)});
        }
        if (check("(")) {
            advance();
            std::vector<std::string> parameters;
            bool parameter_list = check_kind(Kind::Identifier) && (tokens_[position_ + 1].text == "," || tokens_[position_ + 1].text == ")");
            if (parameter_list) {
                while (!check(")")) { parameters.push_back(identifier()); if (!match(",")) break; }
                expect(")");
                if (match("=>")) return std::make_shared<Expr>(Expr::Lambda{parameters, expression()});
            } else {
                ExprPtr value = expression(); expect(")"); return suffix(value);
            }
        }
        if (check("[")) {
            advance(); std::vector<std::vector<ExprPtr>> rows; std::vector<ExprPtr> row;
            auto starts_expression = [&]() {
                return check_kind(Kind::Number) || check_kind(Kind::String) || check_kind(Kind::Identifier) ||
                       check("(") || check("[") || check("{") || check("-") || check("!");
            };
            while (!check("]")) {
                row.push_back(expression());
                if (match(",")) continue;
                if (match(";")) { rows.push_back(std::move(row)); row.clear(); skip_lines(); continue; }
                if (check("]")) break;
                if (!starts_expression()) throw Error("line " + std::to_string(current().line) + ": expected array separator");
            }
            rows.push_back(std::move(row)); expect("]");
            if (rows.size() == 1) return std::make_shared<Expr>(Expr::Array{std::move(rows.front())});
            std::vector<ExprPtr> matrix;
            for (auto& values : rows) matrix.push_back(std::make_shared<Expr>(Expr::Array{std::move(values)}));
            return std::make_shared<Expr>(Expr::Array{std::move(matrix)});
        }
        if (check("{")) {
            advance(); std::vector<std::pair<std::string, ExprPtr>> values; skip_lines();
            while (!check("}")) {
                std::string key;
                if (check_kind(Kind::String) || check_kind(Kind::Identifier)) { key = current().text; advance(); }
                else throw Error("line " + std::to_string(current().line) + ": expected object key");
                expect(":"); values.emplace_back(key, expression()); if (!match(",")) break; skip_lines();
            }
            skip_lines(); expect("}"); return std::make_shared<Expr>(Expr::Object{values});
        }
        if (check_kind(Kind::Number)) { double value = current().number; advance(); return suffix(std::make_shared<Expr>(Expr::Literal{value})); }
        if (check_kind(Kind::String)) { std::string value = current().text; advance(); return suffix(std::make_shared<Expr>(Expr::Literal{value})); }
        if (check("true") || check("false")) { bool value = check("true"); advance(); return suffix(std::make_shared<Expr>(Expr::Literal{value})); }
        if (check("none")) { advance(); return suffix(std::make_shared<Expr>(Expr::Literal{nullptr})); }
        if (check_kind(Kind::Identifier)) {
            std::string value = current().text; advance();
            ExprPtr result = suffix(std::make_shared<Expr>(Expr::Name{value}));
            if (match("=>")) result = std::make_shared<Expr>(Expr::Lambda{{value}, expression()});
            return result;
        }
        throw Error("line " + std::to_string(current().line) + ": expected expression");
    }
    ExprPtr suffix(ExprPtr value) {
        while (true) {
            if (match("(")) {
                std::vector<ExprPtr> args;
                while (!check(")")) { args.push_back(expression()); if (!match(",")) break; }
                expect(")"); value = std::make_shared<Expr>(Expr::Call{value, args});
            } else if (match("[")) {
                ExprPtr index = expression();
                if (match(",")) {
                    std::vector<ExprPtr> indices{index, expression()};
                    while (match(",")) indices.push_back(expression());
                    index = std::make_shared<Expr>(Expr::Array{indices});
                }
                expect("]"); value = std::make_shared<Expr>(Expr::Index{value, index});
            } else break;
        }
        return value;
    }
    std::vector<Statement> required_block() {
        skip_lines();
        if (!match("{")) throw Error("line " + std::to_string(current().line) + ": expected '{'");
        return block("}");
    }
    std::vector<Statement> block(const std::string& terminator) {
        std::vector<Statement> result;
        skip_lines();
        while (!check_kind(Kind::End) && (terminator.empty() || !check(terminator))) {
            if (check("]") && terminator.empty()) break;
            result.push_back(statement());
            skip_lines();
        }
        if (!terminator.empty()) expect(terminator);
        return result;
    }
    std::vector<Token> tokens_; size_t position_{0};
};

struct Value;
using Array = std::vector<std::shared_ptr<Value>>;
using Object = std::map<std::string, std::shared_ptr<Value>>;
struct Environment;
struct Function;
using FunctionPtr = std::shared_ptr<Function>;
struct Value : std::variant<double, std::string, bool, Array, Object, FunctionPtr, std::nullptr_t> {
    using std::variant<double, std::string, bool, Array, Object, FunctionPtr, std::nullptr_t>::variant;
};
struct Environment {
    std::unordered_map<std::string, Value> values;
    std::shared_ptr<Environment> parent;
};
struct Function {
    std::function<Value(const std::vector<Value>&)> native;
    std::vector<std::string> parameters;
    std::vector<std::string> parameter_types;
    std::string return_type;
    std::vector<Statement> body;
    std::shared_ptr<Environment> closure;
    ExprPtr lambda_body;
};

class Runtime {
public:
    struct ThrowSignal { Value value; };
    explicit Runtime(std::shared_ptr<Environment> environment = nullptr, std::string base_dir = ".", std::shared_ptr<std::set<std::string>> import_stack = nullptr)
        : environment_(environment ? std::move(environment) : std::make_shared<Environment>()), base_dir_(std::move(base_dir)), import_stack_(import_stack ? std::move(import_stack) : std::make_shared<std::set<std::string>>()) {
        if (!environment_->parent && environment_->values.empty()) install_builtins();
    }
    void run(const std::vector<Statement>& statements) {
        try { execute(statements, environment_); }
        catch (const ThrowSignal& signal) { throw Error("uncaught throw: " + display(signal.value)); }
    }
private:
    struct ReturnSignal { Value value; };
    struct BreakSignal {};
    struct ContinueSignal {};
    void install_builtins() {
        environment_->values["pi"] = 3.14159265358979323846;
        environment_->values["e"] = 2.71828182845904523536;
        environment_->values["sin"] = native([](const std::vector<Value>& args) { return std::sin(number(args, 0)); });
        environment_->values["cos"] = native([](const std::vector<Value>& args) { return std::cos(number(args, 0)); });
        environment_->values["tan"] = native([](const std::vector<Value>& args) { return std::tan(number(args, 0)); });
        environment_->values["asin"] = native([](const std::vector<Value>& args) { return std::asin(number(args, 0)); });
        environment_->values["acos"] = native([](const std::vector<Value>& args) { return std::acos(number(args, 0)); });
        environment_->values["atan"] = native([](const std::vector<Value>& args) { return std::atan(number(args, 0)); });
        environment_->values["atan2"] = native([](const std::vector<Value>& args) { if (args.size() != 2) throw Error("atan2 expects two arguments"); return std::atan2(number(args[0]), number(args[1])); });
        environment_->values["sqrt"] = native([](const std::vector<Value>& args) { return std::sqrt(number(args, 0)); });
        environment_->values["floor"] = native([](const std::vector<Value>& args) { return std::floor(number(args, 0)); });
        environment_->values["ceil"] = native([](const std::vector<Value>& args) { return std::ceil(number(args, 0)); });
        environment_->values["abs"] = native([](const std::vector<Value>& args) { return std::abs(number(args, 0)); });
        environment_->values["exp"] = native([](const std::vector<Value>& args) { return std::exp(number(args, 0)); });
        environment_->values["log"] = native([](const std::vector<Value>& args) { return std::log(number(args, 0)); });
        environment_->values["log2"] = native([](const std::vector<Value>& args) { return std::log2(number(args, 0)); });
        environment_->values["log10"] = native([](const std::vector<Value>& args) { return std::log10(number(args, 0)); });
        environment_->values["pow"] = native([](const std::vector<Value>& args) { if (args.size() != 2) throw Error("pow expects two arguments"); return std::pow(number(args[0]), number(args[1])); });
        environment_->values["sign"] = native([](const std::vector<Value>& args) { const double value = number(args, 0); return value > 0 ? 1.0 : (value < 0 ? -1.0 : 0.0); });
        environment_->values["hypot"] = native([](const std::vector<Value>& args) { if (args.size() != 2) throw Error("hypot expects two arguments"); return std::hypot(number(args[0]), number(args[1])); });
        environment_->values["differentiate"] = native([this](const std::vector<Value>& args) {
            if (args.size() < 2 || args.size() > 3) throw Error("differentiate expects a function, point, and optional step");
            const double point = number(args, 1); const double step = args.size() == 3 ? number(args, 2) : 1e-5;
            if (step == 0) throw Error("differentiate step cannot be zero");
            const double right = number(invoke(args[0], {Value(point + step)}));
            const double left = number(invoke(args[0], {Value(point - step)}));
            return Value((right - left) / (2 * step));
        });
        environment_->values["integrate"] = native([this](const std::vector<Value>& args) {
            if (args.size() < 3 || args.size() > 4) throw Error("integrate expects a function, lower bound, upper bound, and optional steps");
            const double lower = number(args, 1), upper = number(args, 2); int steps = args.size() == 4 ? static_cast<int>(number(args, 3)) : 1000;
            if (steps < 1) throw Error("integrate steps must be positive");
            const double width = (upper - lower) / steps; double result = 0;
            for (int index = 0; index <= steps; ++index) {
                const double x = lower + width * index; const double value = number(invoke(args[0], {Value(x)}));
                if (steps % 2 == 0) result += (index == 0 || index == steps) ? value : (index % 2 == 0 ? 2 * value : 4 * value);
                else result += (index == 0 || index == steps) ? value / 2 : value;
            }
            return Value(steps % 2 == 0 ? result * width / 3 : result * width / 2);
        });
        environment_->values["root"] = native([this](const std::vector<Value>& args) {
            if (args.size() < 2 || args.size() > 4) throw Error("root expects a function, initial guess, and optional tolerance and iterations");
            double x = number(args[1]); const double tolerance = args.size() > 2 ? number(args[2]) : 1e-8; const int iterations = args.size() > 3 ? static_cast<int>(number(args[3])) : 100;
            if (tolerance <= 0 || iterations < 1) throw Error("root tolerance and iterations must be positive");
            for (int iteration = 0; iteration < iterations; ++iteration) {
                const double value = number(invoke(args[0], {Value(x)})); if (std::abs(value) <= tolerance) return Value(x);
                const double step = 1e-6 * std::max(1.0, std::abs(x));
                const double derivative = (number(invoke(args[0], {Value(x + step)})) - number(invoke(args[0], {Value(x - step)}))) / (2 * step);
                if (std::abs(derivative) < 1e-14) throw Error("root derivative is too small");
                x -= value / derivative;
            }
            throw Error("root did not converge");
        });
        environment_->values["ode_euler"] = native([this](const std::vector<Value>& args) {
            if (args.size() < 4 || args.size() > 5) throw Error("ode_euler expects a derivative, initial value, start, end, and optional steps");
            double y = number(args[1]); const double start = number(args[2]), end = number(args[3]); const int steps = args.size() == 5 ? static_cast<int>(number(args[4])) : 100;
            if (steps < 1) throw Error("ode_euler steps must be positive");
            const double width = (end - start) / steps; Array result;
            for (int index = 0; index <= steps; ++index) {
                const double time = start + width * index; Array sample; sample.push_back(std::make_shared<Value>(time)); sample.push_back(std::make_shared<Value>(y)); result.push_back(std::make_shared<Value>(sample));
                if (index < steps) y += width * number(invoke(args[0], {Value(time), Value(y)}));
            }
            return Value(result);
        });
        environment_->values["ode_rk4"] = native([this](const std::vector<Value>& args) {
            if (args.size() < 4 || args.size() > 5) throw Error("ode_rk4 expects a derivative, initial value, start, end, and optional steps");
            double y = number(args[1]); const double start = number(args[2]), end = number(args[3]); const int steps = args.size() == 5 ? static_cast<int>(number(args[4])) : 100;
            if (steps < 1) throw Error("ode_rk4 steps must be positive");
            const double width = (end - start) / steps; auto derivative = [&](double time, double value) { return number(invoke(args[0], {Value(time), Value(value)})); };
            Array result;
            for (int index = 0; index <= steps; ++index) {
                const double time = start + width * index; Array sample; sample.push_back(std::make_shared<Value>(time)); sample.push_back(std::make_shared<Value>(y)); result.push_back(std::make_shared<Value>(sample));
                if (index < steps) {
                    const double k1 = derivative(time, y), k2 = derivative(time + width / 2, y + width * k1 / 2), k3 = derivative(time + width / 2, y + width * k2 / 2), k4 = derivative(time + width, y + width * k3);
                    y += width * (k1 + 2 * k2 + 2 * k3 + k4) / 6;
                }
            }
            return Value(result);
        });
        environment_->values["fft"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("fft expects one numeric vector");
            const auto& input = array(args[0]); const size_t size = input.size(); if (size == 0) return Value(Object{});
            Array real, imaginary; const double tau = 2 * 3.14159265358979323846;
            for (size_t frequency = 0; frequency < size; ++frequency) {
                double re = 0, im = 0;
                for (size_t sample = 0; sample < size; ++sample) { const double angle = tau * frequency * sample / size; const double value = number(*input[sample]); re += value * std::cos(angle); im -= value * std::sin(angle); }
                real.push_back(std::make_shared<Value>(re)); imaginary.push_back(std::make_shared<Value>(im));
            }
            Object result; result["real"] = std::make_shared<Value>(real); result["imag"] = std::make_shared<Value>(imaginary); return Value(result);
        });
        environment_->values["ifft"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1 || !std::holds_alternative<Object>(args[0])) throw Error("ifft expects an fft object");
            const auto& object = std::get<Object>(args[0]); const auto real_it = object.find("real"), imag_it = object.find("imag");
            if (real_it == object.end() || imag_it == object.end()) throw Error("ifft expects real and imag fields");
            const auto& real = array(*real_it->second); const auto& imaginary = array(*imag_it->second); if (real.size() != imaginary.size()) throw Error("ifft fields must have equal lengths");
            const size_t size = real.size(); Array result; const double tau = 2 * 3.14159265358979323846;
            for (size_t sample = 0; sample < size; ++sample) { double value = 0; for (size_t frequency = 0; frequency < size; ++frequency) { const double angle = tau * frequency * sample / size; value += number(*real[frequency]) * std::cos(angle) - number(*imaginary[frequency]) * std::sin(angle); } value /= size; result.push_back(std::make_shared<Value>(std::abs(value) < 1e-12 ? 0.0 : value)); }
            return Value(result);
        });
        environment_->values["polyfit"] = native([](const std::vector<Value>& args) {
            if (args.size() != 3) throw Error("polyfit expects x values, y values, and a degree");
            const auto& x = array(args[0]); const auto& y = array(args[1]); const int degree = static_cast<int>(number(args[2]));
            if (x.empty() || x.size() != y.size() || degree < 0 || x.size() < static_cast<size_t>(degree + 1)) throw Error("polyfit inputs have incompatible sizes");
            const size_t width = static_cast<size_t>(degree + 1); std::vector<std::vector<double>> normal(width, std::vector<double>(width, 0)); std::vector<double> target(width, 0);
            for (size_t sample = 0; sample < x.size(); ++sample) {
                const double xv = number(*x[sample]), yv = number(*y[sample]); std::vector<double> powers(width, 1);
                for (size_t index = 1; index < width; ++index) powers[index] = powers[index - 1] * xv;
                for (size_t row = 0; row < width; ++row) { target[row] += powers[row] * yv; for (size_t column = 0; column < width; ++column) normal[row][column] += powers[row] * powers[column]; }
            }
            std::vector<double> ascending = solve_linear(matrix_value(normal), vector_value(target)); std::reverse(ascending.begin(), ascending.end()); return vector_value(ascending);
        });
        environment_->values["polyval"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("polyval expects coefficients and a value");
            const auto& coefficients = array(args[0]); const double x = number(args[1]); double result = 0;
            for (const auto& coefficient : coefficients) result = result * x + number(*coefficient);
            return Value(result);
        });
        environment_->values["len"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("len expects one argument");
            if (auto values = std::get_if<Array>(&args[0])) return Value(static_cast<double>(values->size()));
            if (auto text = std::get_if<std::string>(&args[0])) return Value(static_cast<double>(text->size()));
            if (auto object = std::get_if<Object>(&args[0])) return Value(static_cast<double>(object->size()));
            throw Error("len expects an array, string, or object");
        });
        environment_->values["keys"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("keys expects one object");
            const auto& object = std::get<Object>(args[0]); Array result;
            for (const auto& entry : object) result.push_back(std::make_shared<Value>(entry.first));
            return Value(result);
        });
        environment_->values["values"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("values expects one object");
            const auto& object = std::get<Object>(args[0]); Array result;
            for (const auto& entry : object) result.push_back(std::make_shared<Value>(*entry.second));
            return Value(result);
        });
        environment_->values["has_key"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("has_key expects an object and a key");
            const auto& object = std::get<Object>(args[0]); const auto& key = std::get<std::string>(args[1]);
            return Value(object.find(key) != object.end());
        });
        environment_->values["str"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("str expects one argument");
            return Value(display(args[0]));
        });
        environment_->values["number"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("number expects one argument");
            if (std::holds_alternative<double>(args[0])) return args[0];
            if (auto text = std::get_if<std::string>(&args[0])) {
                try { size_t parsed = 0; const double result = std::stod(*text, &parsed); if (parsed == text->size()) return Value(result); } catch (...) {}
            }
            throw Error("number expects a numeric value");
        });
        environment_->values["lower"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("lower expects one string");
            std::string result = std::get<std::string>(args[0]);
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            return Value(result);
        });
        environment_->values["upper"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("upper expects one string");
            std::string result = std::get<std::string>(args[0]);
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
            return Value(result);
        });
        environment_->values["contains"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("contains expects a string and a substring");
            return Value(std::get<std::string>(args[0]).find(std::get<std::string>(args[1])) != std::string::npos);
        });
        environment_->values["split"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("split expects a string and a delimiter");
            const std::string text = std::get<std::string>(args[0]), delimiter = std::get<std::string>(args[1]);
            if (delimiter.empty()) throw Error("split delimiter cannot be empty");
            Array result; size_t start = 0;
            while (true) {
                const size_t end = text.find(delimiter, start);
                result.push_back(std::make_shared<Value>(text.substr(start, end == std::string::npos ? std::string::npos : end - start)));
                if (end == std::string::npos) break;
                start = end + delimiter.size();
            }
            return Value(result);
        });
        environment_->values["join"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("join expects an array and a delimiter");
            const auto& values = array(args[0]); const std::string delimiter = std::get<std::string>(args[1]); std::string result;
            for (size_t index = 0; index < values.size(); ++index) { if (index) result += delimiter; result += display(*values[index]); }
            return Value(result);
        });
        environment_->values["range"] = native([](const std::vector<Value>& args) {
            if (args.empty() || args.size() > 3) throw Error("range expects one to three arguments");
            const double start = args.size() == 1 ? 0.0 : number(args, 0);
            const double stop = number(args, args.size() == 1 ? 0 : 1);
            const double step = args.size() == 3 ? number(args, 2) : 1.0;
            Array result; if (step == 0) throw Error("range step cannot be zero");
            for (double value = start; step > 0 ? value < stop : value > stop; value += step) {
                if (result.size() > 1000000) throw Error("range exceeded safety limit");
                result.push_back(std::make_shared<Value>(value));
            }
            return Value(result);
        });
        environment_->values["linspace"] = native([](const std::vector<Value>& args) {
            if (args.size() < 2 || args.size() > 3) throw Error("linspace expects two or three arguments");
            int count = args.size() == 3 ? static_cast<int>(number(args, 2)) : 100;
            Array result; if (count < 2) { result.push_back(std::make_shared<Value>(number(args, 0))); return Value(result); }
            double step = (number(args, 1) - number(args, 0)) / (count - 1);
            for (int i = 0; i < count; ++i) result.push_back(std::make_shared<Value>(number(args, 0) + step * i));
            return Value(result);
        });
        environment_->values["meshgrid"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("meshgrid expects two vectors");
            const auto& x = array(args[0]); const auto& y = array(args[1]); Array grid_x, grid_y;
            for (size_t row_index = 0; row_index < y.size(); ++row_index) {
                Array row; for (const auto& x_value : x) row.push_back(std::make_shared<Value>(*x_value));
                grid_x.push_back(std::make_shared<Value>(row));
            }
            for (const auto& y_value : y) {
                Array row; for (size_t column = 0; column < x.size(); ++column) row.push_back(std::make_shared<Value>(*y_value));
                grid_y.push_back(std::make_shared<Value>(row));
            }
            Array result; result.push_back(std::make_shared<Value>(grid_x)); result.push_back(std::make_shared<Value>(grid_y)); return Value(result);
        });
        environment_->values["gradient"] = native([](const std::vector<Value>& args) {
            if (args.size() < 1 || args.size() > 2) throw Error("gradient expects a vector and optional spacing");
            const auto& values = array(args[0]); if (values.empty()) return Value(Array{});
            const double spacing = args.size() == 2 ? number(args[1]) : 1.0; if (spacing == 0) throw Error("gradient spacing cannot be zero");
            if (std::holds_alternative<Array>(*values.front())) {
                const size_t rows = values.size(); const auto& first_row = array(*values.front()); const size_t columns = first_row.size();
                if (columns == 0) return Value(Array{});
                for (const auto& row_value : values) if (!std::holds_alternative<Array>(*row_value) || array(*row_value).size() != columns) throw Error("gradient matrix must be rectangular");
                auto sample = [&](size_t row, size_t column) { return number(*array(*values[row])[column]); };
                auto derivative = [&](size_t index, size_t limit, auto&& value_at) {
                    if (limit == 1) return 0.0;
                    if (index == 0) return (value_at(1) - value_at(0)) / spacing;
                    if (index + 1 == limit) return (value_at(index) - value_at(index - 1)) / spacing;
                    return (value_at(index + 1) - value_at(index - 1)) / (2 * spacing);
                };
                Array dx, dy;
                for (size_t row = 0; row < rows; ++row) {
                    Array x_row, y_row;
                    for (size_t column = 0; column < columns; ++column) {
                        x_row.push_back(std::make_shared<Value>(derivative(column, columns, [&](size_t index) { return sample(row, index); })));
                        y_row.push_back(std::make_shared<Value>(derivative(row, rows, [&](size_t index) { return sample(index, column); })));
                    }
                    dx.push_back(std::make_shared<Value>(x_row)); dy.push_back(std::make_shared<Value>(y_row));
                }
                Array result; result.push_back(std::make_shared<Value>(dx)); result.push_back(std::make_shared<Value>(dy)); return Value(result);
            }
            Array result; result.reserve(values.size());
            for (size_t index = 0; index < values.size(); ++index) {
                double value = 0;
                if (values.size() == 1) value = 0;
                else if (index == 0) value = (number(*values[1]) - number(*values[0])) / spacing;
                else if (index + 1 == values.size()) value = (number(*values[index]) - number(*values[index - 1])) / spacing;
                else value = (number(*values[index + 1]) - number(*values[index - 1])) / (2 * spacing);
                result.push_back(std::make_shared<Value>(value));
            }
            return Value(result);
        });
        environment_->values["trapz"] = native([](const std::vector<Value>& args) {
            if (args.size() < 1 || args.size() > 2) throw Error("trapz expects a vector and optional spacing");
            const auto& values = array(args[0]); if (values.size() < 2) return Value(0.0);
            const double spacing = args.size() == 2 ? number(args[1]) : 1.0; double result = 0;
            for (size_t index = 1; index < values.size(); ++index) result += (number(*values[index - 1]) + number(*values[index])) * spacing / 2;
            return Value(result);
        });
        environment_->values["interp1"] = native([](const std::vector<Value>& args) {
            if (args.size() != 3) throw Error("interp1 expects a query and two sample vectors");
            const double query = number(args[0]); const auto& x = array(args[1]); const auto& y = array(args[2]);
            if (x.empty() || x.size() != y.size()) throw Error("interp1 sample vectors must have equal nonzero lengths");
            if (x.size() == 1) return Value(number(*y.front()));
            if (query <= number(*x.front())) return Value(number(*y.front()));
            if (query >= number(*x.back())) return Value(number(*y.back()));
            for (size_t index = 1; index < x.size(); ++index) {
                const double left_x = number(*x[index - 1]), right_x = number(*x[index]);
                if (right_x <= left_x) throw Error("interp1 x samples must be strictly increasing");
                if (query <= right_x) {
                    const double fraction = (query - left_x) / (right_x - left_x);
                    return Value(number(*y[index - 1]) + fraction * (number(*y[index]) - number(*y[index - 1])));
                }
            }
            return Value(number(*y.back()));
        });
        environment_->values["sum"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("sum expects one argument");
            const auto& values = array(args[0]); double result = 0;
            for (const auto& value : values) result += number(*value);
            return Value(result);
        });
        environment_->values["mean"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("mean expects one argument");
            const auto& values = array(args[0]); if (values.empty()) throw Error("mean of an empty array");
            double result = 0; for (const auto& value : values) result += number(*value);
            return Value(result / values.size());
        });
        environment_->values["median"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("median expects one argument");
            const auto& values = array(args[0]); if (values.empty()) throw Error("median of an empty array");
            std::vector<double> sorted; for (const auto& value : values) sorted.push_back(number(*value));
            std::sort(sorted.begin(), sorted.end());
            const size_t middle = sorted.size() / 2;
            return Value(sorted.size() % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2.0);
        });
        environment_->values["variance"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("variance expects one argument");
            const auto& values = array(args[0]); if (values.empty()) throw Error("variance of an empty array");
            double mean = 0; for (const auto& value : values) mean += number(*value); mean /= values.size();
            double result = 0; for (const auto& value : values) { const double delta = number(*value) - mean; result += delta * delta; }
            return Value(result / values.size());
        });
        environment_->values["std"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("std expects one argument");
            const auto& values = array(args[0]); if (values.empty()) throw Error("std of an empty array");
            double mean = 0; for (const auto& value : values) mean += number(*value); mean /= values.size();
            double result = 0; for (const auto& value : values) { const double delta = number(*value) - mean; result += delta * delta; }
            return Value(std::sqrt(result / values.size()));
        });
        environment_->values["sort"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("sort expects one argument");
            std::vector<double> sorted; for (const auto& value : array(args[0])) sorted.push_back(number(*value));
            std::sort(sorted.begin(), sorted.end()); return vector_value(sorted);
        });
        environment_->values["cumsum"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("cumsum expects one argument");
            std::vector<double> result; double total = 0;
            for (const auto& value : array(args[0])) { total += number(*value); result.push_back(total); }
            return vector_value(result);
        });
        environment_->values["map"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("map expects a function and an array");
            Array result; for (const auto& value : array(args[1])) result.push_back(std::make_shared<Value>(invoke(args[0], {*value})));
            return Value(result);
        });
        environment_->values["filter"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("filter expects a function and an array");
            Array result; for (const auto& value : array(args[1])) if (truthy(invoke(args[0], {*value}))) result.push_back(std::make_shared<Value>(*value));
            return Value(result);
        });
        environment_->values["reduce"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 3) throw Error("reduce expects a function, an array, and an initial value");
            Value result = args[2]; for (const auto& value : array(args[1])) result = invoke(args[0], {result, *value});
            return result;
        });
        environment_->values["all"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("all expects one array");
            for (const auto& value : array(args[0])) if (!truthy(*value)) return Value(false);
            return Value(true);
        });
        environment_->values["any"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("any expects one array");
            for (const auto& value : array(args[0])) if (truthy(*value)) return Value(true);
            return Value(false);
        });
        environment_->values["zip"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("zip expects two arrays");
            const auto& left = array(args[0]); const auto& right = array(args[1]); const size_t count = std::min(left.size(), right.size());
            Array result; for (size_t index = 0; index < count; ++index) { Array pair; pair.push_back(std::make_shared<Value>(*left[index])); pair.push_back(std::make_shared<Value>(*right[index])); result.push_back(std::make_shared<Value>(pair)); }
            return Value(result);
        });
        environment_->values["assert"] = native([](const std::vector<Value>& args) {
            if (args.empty() || args.size() > 2) throw Error("assert expects a condition and optional message");
            if (!truthy(args[0])) {
                if (args.size() == 2) { if (auto message = std::get_if<std::string>(&args[1])) throw Error("assertion failed: " + *message); }
                throw Error("assertion failed");
            }
            return Value(nullptr);
        });
        environment_->values["min"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("min expects one argument");
            const auto& values = array(args[0]); if (values.empty()) throw Error("min of an empty array");
            double result = number(*values.front()); for (const auto& value : values) result = std::min(result, number(*value));
            return Value(result);
        });
        environment_->values["max"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("max expects one argument");
            const auto& values = array(args[0]); if (values.empty()) throw Error("max of an empty array");
            double result = number(*values.front()); for (const auto& value : values) result = std::max(result, number(*value));
            return Value(result);
        });
        environment_->values["find"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("find expects one array");
            const auto& values = array(args[0]); Array result;
            for (size_t index = 0; index < values.size(); ++index) if (truthy(*values[index])) result.push_back(std::make_shared<Value>(static_cast<double>(index)));
            return Value(result);
        });
        environment_->values["argmin"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("argmin expects one array");
            const auto& values = array(args[0]); if (values.empty()) throw Error("argmin of an empty array");
            size_t result = 0; for (size_t index = 1; index < values.size(); ++index) if (number(*values[index]) < number(*values[result])) result = index;
            return Value(static_cast<double>(result));
        });
        environment_->values["argmax"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("argmax expects one array");
            const auto& values = array(args[0]); if (values.empty()) throw Error("argmax of an empty array");
            size_t result = 0; for (size_t index = 1; index < values.size(); ++index) if (number(*values[index]) > number(*values[result])) result = index;
            return Value(static_cast<double>(result));
        });
        environment_->values["norm"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("norm expects one vector");
            const auto& values = array(args[0]); double result = 0; for (const auto& value : values) result += number(*value) * number(*value);
            return Value(std::sqrt(result));
        });
        environment_->values["normalize"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("normalize expects one vector");
            const auto& values = array(args[0]); double result = 0; for (const auto& value : values) result += number(*value) * number(*value);
            if (result == 0) throw Error("cannot normalize the zero vector");
            Array normalized; for (const auto& value : values) normalized.push_back(std::make_shared<Value>(number(*value) / std::sqrt(result))); return Value(normalized);
        });
        environment_->values["cross"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("cross expects two vectors");
            const auto& left = array(args[0]); const auto& right = array(args[1]); if (left.size() != 3 || right.size() != 3) throw Error("cross expects three-dimensional vectors");
            Array result; result.push_back(std::make_shared<Value>(number(*left[1]) * number(*right[2]) - number(*left[2]) * number(*right[1]))); result.push_back(std::make_shared<Value>(number(*left[2]) * number(*right[0]) - number(*left[0]) * number(*right[2]))); result.push_back(std::make_shared<Value>(number(*left[0]) * number(*right[1]) - number(*left[1]) * number(*right[0]))); return Value(result);
        });
        environment_->values["shape"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("shape expects one argument");
            const auto& values = array(args[0]); Array result;
            result.push_back(std::make_shared<Value>(static_cast<double>(values.size())));
            if (!values.empty() && std::holds_alternative<Array>(*values.front()))
                result.push_back(std::make_shared<Value>(static_cast<double>(array(*values.front()).size())));
            return Value(result);
        });
        environment_->values["reshape"] = native([](const std::vector<Value>& args) {
            if (args.size() != 3) throw Error("reshape expects an array, rows, and columns");
            const auto& values = array(args[0]); const int rows = static_cast<int>(number(args, 1)); const int columns = static_cast<int>(number(args, 2));
            if (rows < 0 || columns < 0 || static_cast<size_t>(rows * columns) != values.size()) throw Error("reshape dimensions do not match array length");
            Array result; size_t index = 0;
            for (int row = 0; row < rows; ++row) { Array line; for (int column = 0; column < columns; ++column) line.push_back(std::make_shared<Value>(*values[index++])); result.push_back(std::make_shared<Value>(line)); }
            return Value(result);
        });
        environment_->values["dot"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("dot expects two arguments");
            const auto& left = array(args[0]);
            const auto& right = array(args[1]);
            if (left.size() != right.size()) throw Error("dot operands must have equal lengths");
            double result = 0;
            for (size_t index = 0; index < left.size(); ++index) {
                result += number(*left[index]) * number(*right[index]);
            }
            return Value(result);
        });
        environment_->values["outer"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("outer expects two vectors");
            const auto& left = array(args[0]); const auto& right = array(args[1]); Array result;
            for (const auto& left_value : left) {
                Array row;
                for (const auto& right_value : right) row.push_back(std::make_shared<Value>(number(*left_value) * number(*right_value)));
                result.push_back(std::make_shared<Value>(row));
            }
            return Value(result);
        });
        environment_->values["matmul"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("matmul expects two arrays");
            return matrix_multiply(args[0], args[1]);
        });
        environment_->values["transpose"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("transpose expects one argument");
            const auto& matrix_value = array(args[0]);
            if (matrix_value.empty()) return Value(Array{});
            if (!std::holds_alternative<Array>(*matrix_value.front())) {
                Array result;
                for (const auto& value : matrix_value) { Array row; row.push_back(std::make_shared<Value>(*value)); result.push_back(std::make_shared<Value>(row)); }
                return Value(result);
            }
            const auto& first_row = array(*matrix_value.front());
            Array result;
            for (size_t column = 0; column < first_row.size(); ++column) {
                Array row;
                for (const auto& row_value : matrix_value) {
                    const auto& values = array(*row_value);
                    if (column >= values.size()) throw Error("transpose requires a rectangular matrix");
                    row.push_back(std::make_shared<Value>(*values[column]));
                }
                result.push_back(std::make_shared<Value>(row));
            }
            return Value(result);
        });
        environment_->values["det"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("det expects one matrix");
            return Value(determinant(args[0]));
        });
        environment_->values["inverse"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("inverse expects one matrix");
            return matrix_value(inverse_matrix(args[0]));
        });
        environment_->values["solve"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("solve expects a matrix and a vector");
            return vector_value(solve_linear(args[0], args[1]));
        });
        environment_->values["eye"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("eye expects a matrix size");
            const int size = static_cast<int>(number(args, 0)); if (size < 0) throw Error("eye size cannot be negative");
            Array result; for (int row = 0; row < size; ++row) { Array values; for (int column = 0; column < size; ++column) values.push_back(std::make_shared<Value>(row == column ? 1.0 : 0.0)); result.push_back(std::make_shared<Value>(values)); } return Value(result);
        });
        environment_->values["diag"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("diag expects one vector or matrix");
            if (auto values = std::get_if<Array>(&args[0])) {
                if (!values->empty() && std::holds_alternative<Array>(*values->front())) {
                    Array diagonal; for (size_t index = 0; index < values->size(); ++index) { const auto& row = array(*(*values)[index]); if (index < row.size()) diagonal.push_back(std::make_shared<Value>(*row[index])); } return Value(diagonal);
                }
                const size_t size = values->size(); Array result; for (size_t row = 0; row < size; ++row) { Array line; for (size_t column = 0; column < size; ++column) line.push_back(std::make_shared<Value>(row == column ? number(*(*values)[row]) : 0.0)); result.push_back(std::make_shared<Value>(line)); } return Value(result);
            }
            throw Error("diag expects an array");
        });
        environment_->values["trace"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("trace expects a matrix");
            const auto matrix = numeric_matrix(args[0]); double result = 0; for (size_t index = 0; index < matrix.size(); ++index) result += matrix[index][index]; return Value(result);
        });
        environment_->values["zeros"] = native([](const std::vector<Value>& args) { return matrix(args, 0); });
        environment_->values["ones"] = native([](const std::vector<Value>& args) { return matrix(args, 1); });
        auto read_delimited = [this](const std::vector<Value>& args, char separator, const std::string& format) -> Value {
            if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) throw Error("read_" + format + " expects a file path");
            auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]);
            std::ifstream input(path); if (!input) throw Error("unable to open " + format + " '" + path.string() + "'");
            Array rows; std::string line;
            while (std::getline(input, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;
                Array row; std::vector<std::string> fields; std::string field; bool quoted = false;
                for (size_t index = 0; index <= line.size(); ++index) {
                    const char character = index < line.size() ? line[index] : separator;
                    if (character == '"') {
                        if (quoted && index + 1 < line.size() && line[index + 1] == '"') { field += '"'; ++index; }
                        else quoted = !quoted;
                    } else if (character == separator && !quoted) { fields.push_back(field); field.clear(); }
                    else field += character;
                }
                if (quoted) throw Error(format + " contains an unterminated quoted field");
                for (const auto& value : fields) {
                    try {
                        size_t parsed = 0; const double number_value = std::stod(value, &parsed);
                        if (parsed == value.size()) row.push_back(std::make_shared<Value>(number_value));
                        else row.push_back(std::make_shared<Value>(value));
                    } catch (...) { row.push_back(std::make_shared<Value>(value)); }
                }
                rows.push_back(std::make_shared<Value>(row));
            }
            return Value(rows);
        };
        environment_->values["read_csv"] = native([read_delimited](const std::vector<Value>& args) { return read_delimited(args, ',', "CSV"); });
        environment_->values["read_tsv"] = native([read_delimited](const std::vector<Value>& args) { return read_delimited(args, '\t', "TSV"); });
        auto write_delimited = [this](const std::vector<Value>& args, char separator, const std::string& format) -> Value {
            if (args.size() != 2 || !std::holds_alternative<std::string>(args[0])) throw Error("write_" + format + " expects a file path and an array");
            auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]);
            std::ofstream output(path); if (!output) throw Error("unable to write " + format + " '" + path.string() + "'");
            auto csv_field = [](const Value& value) {
                const std::string text = display(value);
                if (text.find_first_of("\t,\"\n\r") == std::string::npos) return text;
                std::string escaped = "\"";
                for (char character : text) { if (character == '"') escaped += '"'; escaped += character; }
                return escaped + "\"";
            };
            const auto& rows = array(args[1]);
            for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
                if (auto row = std::get_if<Array>(&*rows[row_index])) {
                    for (size_t column = 0; column < row->size(); ++column) { if (column) output << separator; output << csv_field(*(*row)[column]); }
                } else output << csv_field(*rows[row_index]);
                if (row_index + 1 < rows.size()) output << '\n';
            }
            return Value(nullptr);
        };
        environment_->values["write_csv"] = native([write_delimited](const std::vector<Value>& args) { return write_delimited(args, ',', "CSV"); });
        environment_->values["write_tsv"] = native([write_delimited](const std::vector<Value>& args) { return write_delimited(args, '\t', "TSV"); });
        environment_->values["read_text"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) throw Error("read_text expects a file path");
            const auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]); std::ifstream input(path);
            if (!input) throw Error("unable to open text file '" + path.string() + "'"); std::stringstream contents; contents << input.rdbuf(); return Value(contents.str());
        });
        environment_->values["write_text"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) throw Error("write_text expects a file path and string");
            const auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]); std::ofstream output(path);
            if (!output) throw Error("unable to write text file '" + path.string() + "'"); output << std::get<std::string>(args[1]); return Value(nullptr);
        });
        environment_->values["write_json"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("write_json expects a file path and a value");
            auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]);
            std::ofstream output(path); if (!output) throw Error("unable to write JSON '" + path.string() + "'");
            output << json(args[1]); return Value(nullptr);
        });
        environment_->values["read_json"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("read_json expects a file path");
            auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]);
            std::ifstream input(path); if (!input) throw Error("unable to open JSON '" + path.string() + "'");
            std::stringstream contents; contents << input.rdbuf(); const std::string text = contents.str(); size_t position = 0;
            Value result = parse_json(text, position); json_space(text, position);
            if (position != text.size()) throw Error("JSON contains trailing content");
            return result;
        });
    }
    static FunctionPtr native(std::function<Value(const std::vector<Value>&)> function) {
        auto result = std::make_shared<Function>(); result->native = std::move(function); return result;
    }
    void import_module(const std::string& module, const std::shared_ptr<Environment>& environment) {
        if (module.empty()) throw Error("module name cannot be empty");
        std::filesystem::path path(module);
        if (path.extension() != ".marila") path += ".marila";
        if (path.is_relative()) path = std::filesystem::path(base_dir_) / path;
        std::ifstream input(path);
        if (!input) throw Error("module '" + module + "' was not found");
        std::stringstream source; source << input.rdbuf();
        path = std::filesystem::weakly_canonical(path);
        if (import_stack_->count(path.string())) throw Error("circular import involving '" + path.string() + "'");
        import_stack_->insert(path.string());
        Runtime module_runtime(nullptr, path.parent_path().string(), import_stack_);
        try {
            module_runtime.run(Parser(Lexer(source.str()).tokenize()).parse());
        } catch (...) {
            import_stack_->erase(path.string());
            throw;
        }
        import_stack_->erase(path.string());
        for (const auto& entry : module_runtime.environment_->values)
            if (!entry.first.empty() && entry.first[0] != '_') environment->values[entry.first] = entry.second;
    }
    void execute(const std::vector<Statement>& statements, const std::shared_ptr<Environment>& environment) {
        for (const auto& statement : statements) execute_statement(statement, environment);
    }
    void execute_statement(const Statement& statement, const std::shared_ptr<Environment>& environment) {
        switch (statement.kind) {
        case Statement::Kind::Let: {
            Value value = evaluate(statement.expression, environment);
            if (!statement.type.empty() && !matches_type(value, statement.type)) throw Error("value for '" + statement.name + "' is not of type " + statement.type);
            environment->values[statement.name] = value; break;
        }
        case Statement::Kind::Assign: assign(environment, statement.name, evaluate(statement.expression, environment)); break;
        case Statement::Kind::AssignIndex: assign_index(statement.target, evaluate(statement.expression, environment), environment); break;
        case Statement::Kind::Print: std::cout << display(evaluate(statement.expression, environment)) << '\n'; break;
        case Statement::Kind::Expression: evaluate(statement.expression, environment); break;
        case Statement::Kind::Import: import_module(statement.module, environment); break;
        case Statement::Kind::Function: {
            auto function = std::make_shared<Function>(); function->parameters = statement.parameters;
            function->parameter_types = statement.parameter_types;
            function->return_type = statement.type;
            function->body = statement.body; function->closure = environment; environment->values[statement.name] = function; break;
        }
        case Statement::Kind::Return: throw ReturnSignal{statement.expression ? evaluate(statement.expression, environment) : Value(nullptr)};
        case Statement::Kind::Break: throw BreakSignal{};
        case Statement::Kind::Continue: throw ContinueSignal{};
        case Statement::Kind::Throw: throw ThrowSignal{evaluate(statement.expression, environment)};
        case Statement::Kind::Try:
            try { execute(statement.body, child(environment)); }
            catch (const ThrowSignal& signal) {
                auto catch_environment = child(environment);
                if (!statement.name.empty()) catch_environment->values[statement.name] = signal.value;
                execute(statement.alternate, catch_environment);
            }
            catch (const Error& error) {
                auto catch_environment = child(environment);
                if (!statement.name.empty()) catch_environment->values[statement.name] = std::string(error.what());
                execute(statement.alternate, catch_environment);
            }
            break;
        case Statement::Kind::If:
            if (truthy(evaluate(statement.expression, environment))) execute(statement.body, child(environment));
            else if (!statement.alternate.empty()) execute(statement.alternate, child(environment));
            break;
        case Statement::Kind::While: {
            size_t guard = 0;
            while (truthy(evaluate(statement.expression, environment))) {
                try { execute(statement.body, environment); }
                catch (const ContinueSignal&) {}
                catch (const BreakSignal&) { break; }
                if (++guard > 1000000) throw Error("loop exceeded safety limit");
            }
            break;
        }
        case Statement::Kind::For: {
            Value iterable = evaluate(statement.expression, environment);
            for (const auto& item : array(iterable)) {
                environment->values[statement.name] = *item;
                try { execute(statement.body, environment); }
                catch (const ContinueSignal&) { continue; }
                catch (const BreakSignal&) { break; }
            }
            break;
        }
        }
    }
    Value evaluate(const ExprPtr& expression, const std::shared_ptr<Environment>& environment) {
        return std::visit([this, &environment](const auto& node) -> Value { return eval(node, environment); }, expression->value);
    }
    Value eval(const Expr::Literal& node, const std::shared_ptr<Environment>&) { return std::visit([](const auto& value) -> Value { return value; }, node.value); }
    Value eval(const Expr::Name& node, const std::shared_ptr<Environment>& environment) {
        for (auto current = environment; current; current = current->parent) {
            auto it = current->values.find(node.value); if (it != current->values.end()) return it->second;
        }
        throw Error("undefined name '" + node.value + "'");
    }
    Value eval(const Expr::Array& node, const std::shared_ptr<Environment>& environment) {
        Array values; for (const auto& item : node.values) values.push_back(std::make_shared<Value>(evaluate(item, environment))); return values;
    }
    Value eval(const Expr::Object& node, const std::shared_ptr<Environment>& environment) {
        Object values; for (const auto& item : node.values) values[item.first] = std::make_shared<Value>(evaluate(item.second, environment)); return values;
    }
    Value eval(const Expr::Unary& node, const std::shared_ptr<Environment>& environment) {
        Value value = evaluate(node.value, environment);
        if (node.op == "-") return -number(value);
        if (node.op == "!" || node.op == "not") return !truthy(value);
        throw Error("unsupported unary operator '" + node.op + "'");
    }
    Value eval(const Expr::Binary& node, const std::shared_ptr<Environment>& environment) {
        Value left = evaluate(node.left, environment);
        if (node.op == "and" || node.op == "&&") { if (!truthy(left)) return false; return truthy(evaluate(node.right, environment)); }
        if (node.op == "or" || node.op == "||") { if (truthy(left)) return true; return truthy(evaluate(node.right, environment)); }
        Value right = evaluate(node.right, environment);
        if (node.op == "==") return equal_values(left, right);
        if (node.op == "!=") return !equal_values(left, right);
        if (node.op == "+" || node.op == "-" || node.op == "*" || node.op == "@" || node.op == ".*" || node.op == "/" ||
            node.op == "%" || node.op == "^" || node.op == ".^" || node.op == "./") return arithmetic(node.op, left, right);
        double a = number(left), b = number(right);
        if (node.op == "<") return a < b; if (node.op == "<=") return a <= b;
        if (node.op == ">") return a > b; if (node.op == ">=") return a >= b;
        throw Error("unsupported binary operator '" + node.op + "'");
    }
    Value eval(const Expr::Call& node, const std::shared_ptr<Environment>& environment) {
        Value callee = evaluate(node.callee, environment);
        auto function = std::get_if<FunctionPtr>(&callee);
        if (!function || !*function) throw Error("only functions can be called");
        std::vector<Value> args; for (const auto& arg : node.args) args.push_back(evaluate(arg, environment));
        return invoke(callee, args);
    }
    Value invoke(const Value& callee, const std::vector<Value>& args) {
        auto function = std::get_if<FunctionPtr>(&callee);
        if (!function || !*function) throw Error("only functions can be called");
        if ((*function)->native) return (*function)->native(args);
        if (args.size() != (*function)->parameters.size()) throw Error("wrong number of function arguments");
        auto call_environment = child((*function)->closure);
        for (size_t i = 0; i < args.size(); ++i) {
            if (i < (*function)->parameter_types.size() && !(*function)->parameter_types[i].empty() && !matches_type(args[i], (*function)->parameter_types[i]))
                throw Error("argument '" + (*function)->parameters[i] + "' is not of type " + (*function)->parameter_types[i]);
            call_environment->values[(*function)->parameters[i]] = args[i];
        }
        try {
            if ((*function)->lambda_body) return evaluate((*function)->lambda_body, call_environment);
            execute((*function)->body, call_environment);
        } catch (const ReturnSignal& signal) {
            if (!(*function)->return_type.empty() && !matches_type(signal.value, (*function)->return_type)) throw Error("function return value is not of type " + (*function)->return_type);
            return signal.value;
        }
        Value result(nullptr);
        if (!(*function)->return_type.empty() && !matches_type(result, (*function)->return_type)) throw Error("function returned none, expected " + (*function)->return_type);
        return result;
    }
    Value eval(const Expr::Index& node, const std::shared_ptr<Environment>& environment) {
        Value result = evaluate(node.collection, environment);
        if (std::holds_alternative<Expr::Range>(node.index->value)) {
            Value evaluated_index = evaluate(node.index, environment);
            if (std::holds_alternative<Array>(result)) {
                const auto& indices_value = array(evaluated_index);
                Array selected;
                for (const auto& item : indices_value) selected.push_back(std::make_shared<Value>(index_value(result, static_cast<size_t>(number(*item)))));
                return selected;
            }
        }
        Value evaluated_index = evaluate(node.index, environment);
        if (auto selections = std::get_if<Array>(&evaluated_index); selections && selections->size() == 2 && (std::holds_alternative<Array>(*(selections->at(0))) || std::holds_alternative<Array>(*(selections->at(1))))) {
            auto selected_indices = [](const Value& selection) {
                std::vector<size_t> values;
                if (auto array_selection = std::get_if<Array>(&selection)) for (const auto& item : *array_selection) values.push_back(static_cast<size_t>(number(*item)));
                else values.push_back(static_cast<size_t>(number(selection)));
                return values;
            };
            const auto rows = selected_indices(*selections->at(0));
            const auto columns = selected_indices(*selections->at(1));
            Array sliced;
            for (size_t row : rows) {
                Value row_value = index_value(result, row); const auto& values = array(row_value); Array selected_row;
                for (size_t column : columns) selected_row.push_back(std::make_shared<Value>(index_value(Value(values), column)));
                sliced.push_back(std::make_shared<Value>(selected_row));
            }
            const bool row_slice = std::holds_alternative<Array>(*(selections->at(0)));
            const bool column_slice = std::holds_alternative<Array>(*(selections->at(1)));
            if (row_slice && column_slice) return sliced;
            if (!sliced.empty()) {
                if (!row_slice && column_slice) return *sliced.front();
                Array column; for (const auto& row : sliced) column.push_back(std::make_shared<Value>(index_value(*row, 0))); return column;
            }
            return Array{};
        }
        for (const auto& index : indices(evaluate(node.index, environment))) {
            if (auto key = std::get_if<std::string>(&index)) result = object_value(result, *key);
            else result = index_value(result, static_cast<size_t>(number(index)));
        }
        return result;
    }
    Value eval(const Expr::Range& node, const std::shared_ptr<Environment>& environment) {
        const double start = number(evaluate(node.start, environment));
        const double step = number(evaluate(node.step, environment));
        const double stop = number(evaluate(node.stop, environment));
        if (step == 0) throw Error("range step cannot be zero");
        Array result;
        for (double value = start; step > 0 ? value <= stop : value >= stop; value += step) {
            if (result.size() > 1000000) throw Error("range exceeded safety limit");
            result.push_back(std::make_shared<Value>(value));
        }
        return result;
    }
    Value eval(const Expr::Lambda& node, const std::shared_ptr<Environment>& environment) {
        auto function = std::make_shared<Function>(); function->parameters = node.parameters;
        function->closure = environment; function->lambda_body = node.body; return function;
    }
    static std::shared_ptr<Environment> child(const std::shared_ptr<Environment>& parent) {
        auto result = std::make_shared<Environment>(); result->parent = parent; return result;
    }
    static void assign(const std::shared_ptr<Environment>& environment, const std::string& name, const Value& value) {
        for (auto current = environment; current; current = current->parent) {
            if (current->values.count(name)) { current->values[name] = value; return; }
        }
        throw Error("cannot assign undefined name '" + name + "'");
    }
    static Value* reference(const std::shared_ptr<Environment>& environment, const std::string& name) {
        for (auto current = environment; current; current = current->parent) {
            auto it = current->values.find(name); if (it != current->values.end()) return &it->second;
        }
        return nullptr;
    }
    using PathIndex = std::variant<size_t, std::string>;
    static void set_path(Value& value, const std::vector<PathIndex>& path, size_t depth, const Value& replacement) {
        if (depth >= path.size()) throw Error("invalid indexed assignment");
        if (auto values = std::get_if<Array>(&value)) {
            auto index = std::get_if<size_t>(&path[depth]); if (!index || *index >= values->size()) throw Error("array index out of range");
            if (depth + 1 == path.size()) { *(*values)[*index] = replacement; return; }
            set_path(*(*values)[*index], path, depth + 1, replacement); return;
        }
        if (auto object = std::get_if<Object>(&value)) {
            auto key = std::get_if<std::string>(&path[depth]); if (!key) throw Error("object index must be a string");
            if (depth + 1 == path.size()) { (*object)[*key] = std::make_shared<Value>(replacement); return; }
            auto found = object->find(*key); if (found == object->end()) throw Error("object key not found: " + *key);
            set_path(*found->second, path, depth + 1, replacement); return;
        }
        throw Error("indexed assignment expects an array or object");
    }
    static void collect_indices(const ExprPtr& target, std::vector<ExprPtr>& indices_out, std::string& base_name) {
        const auto& index = std::get<Expr::Index>(target->value);
        if (auto name = std::get_if<Expr::Name>(&index.collection->value)) base_name = name->value;
        else { collect_indices(index.collection, indices_out, base_name); }
        if (auto multiple = std::get_if<Expr::Array>(&index.index->value))
            for (const auto& item : multiple->values) indices_out.push_back(item);
        else indices_out.push_back(index.index);
    }
    static bool is_range_index(const ExprPtr& expression) {
        if (std::holds_alternative<Expr::Range>(expression->value)) return true;
        if (auto multiple = std::get_if<Expr::Array>(&expression->value)) for (const auto& item : multiple->values) if (std::holds_alternative<Expr::Range>(item->value)) return true;
        return false;
    }
    void assign_index(const ExprPtr& target, const Value& replacement, const std::shared_ptr<Environment>& environment) {
        const auto& indexed = std::get<Expr::Index>(target->value);
        if (is_range_index(indexed.index)) {
            std::string base_name;
            if (auto name = std::get_if<Expr::Name>(&indexed.collection->value)) base_name = name->value;
            if (base_name.empty()) throw Error("slice assignment requires a named array");
            Value* base = reference(environment, base_name); if (!base) throw Error("cannot assign indexed value of undefined name '" + base_name + "'");
            Value evaluated_index = evaluate(indexed.index, environment);
            auto select = [](const Value& value, bool& sliced) {
                std::vector<size_t> result;
                if (auto values = std::get_if<Array>(&value)) { sliced = true; for (const auto& item : *values) result.push_back(static_cast<size_t>(number(*item))); }
                else { sliced = false; result.push_back(static_cast<size_t>(number(value))); }
                return result;
            };
            if (std::holds_alternative<Expr::Array>(indexed.index->value)) if (auto multiple = std::get_if<Array>(&evaluated_index); multiple && multiple->size() == 2) {
                bool row_slice = false, column_slice = false;
                const auto rows = select(*multiple->at(0), row_slice), columns = select(*multiple->at(1), column_slice);
                auto matrix = std::get_if<Array>(base); if (!matrix || matrix->empty() || !std::holds_alternative<Array>(*matrix->front())) throw Error("2D slice assignment expects a matrix");
                const auto replacement_array = std::get_if<Array>(&replacement);
                const bool replacement_matrix = replacement_array && !replacement_array->empty() && std::holds_alternative<Array>(*replacement_array->front());
                size_t linear = 0;
                for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
                    Value row_value = index_value(*base, rows[row_index]); auto row = std::get_if<Array>(&row_value);
                    if (!row) throw Error("2D slice assignment expects a matrix");
                    for (size_t column_index = 0; column_index < columns.size(); ++column_index) {
                        Value value = replacement;
                        if (replacement_matrix) {
                            Value replacement_row = index_value(*replacement_array, row_slice ? row_index : 0);
                            value = index_value(replacement_row, column_slice ? column_index : 0);
                        } else if (replacement_array) value = index_value(replacement, linear);
                        if (rows[row_index] >= matrix->size() || columns[column_index] >= row->size()) throw Error("slice assignment index out of range");
                        auto actual_row = std::get_if<Array>(&*matrix->at(rows[row_index])); if (!actual_row || columns[column_index] >= actual_row->size()) throw Error("slice assignment index out of range");
                        *actual_row->at(columns[column_index]) = value; ++linear;
                    }
                }
                return;
            }
            if (auto range = std::get_if<Array>(&evaluated_index); range) {
                auto values = std::get_if<Array>(base); if (!values) throw Error("slice assignment expects an array");
                const auto replacement_array = std::get_if<Array>(&replacement);
                for (size_t index = 0; index < range->size(); ++index) {
                    const size_t target_index = static_cast<size_t>(number(*range->at(index))); if (target_index >= values->size()) throw Error("slice assignment index out of range");
                    *values->at(target_index) = replacement_array ? *replacement_array->at(index) : replacement;
                }
                return;
            }
        }
        std::vector<ExprPtr> index_expressions; std::string base_name;
        collect_indices(target, index_expressions, base_name);
        Value* base = reference(environment, base_name);
        if (!base) throw Error("cannot assign indexed value of undefined name '" + base_name + "'");
        std::vector<PathIndex> path;
        for (const auto& item : index_expressions) {
            Value index = evaluate(item, environment);
            if (auto key = std::get_if<std::string>(&index)) path.emplace_back(*key);
            else path.emplace_back(static_cast<size_t>(number(index)));
        }
        set_path(*base, path, 0, replacement);
    }
    static double number(const std::vector<Value>& values, size_t index) {
        if (index >= values.size()) throw Error("missing numeric argument");
        return number(values[index]);
    }
    static double number(const Value& value) { if (auto number = std::get_if<double>(&value)) return *number; throw Error("expected a number"); }
    static bool matches_type(const Value& value, const std::string& type) {
        if (type == "any") return true;
        if (type == "number") return std::holds_alternative<double>(value);
        if (type == "string") return std::holds_alternative<std::string>(value);
        if (type == "bool" || type == "boolean") return std::holds_alternative<bool>(value);
        if (type == "array" || type == "list") return std::holds_alternative<Array>(value);
        if (type == "object" || type == "record") return std::holds_alternative<Object>(value);
        throw Error("unknown type annotation '" + type + "'");
    }
    static const Array& array(const Value& value) {
        auto result = std::get_if<Array>(&value); if (!result) throw Error("expected an array"); return *result;
    }
    static std::vector<Value> indices(const Value& value) {
        if (auto values = std::get_if<Array>(&value)) { std::vector<Value> result; for (const auto& item : *values) result.push_back(*item); return result; }
        return {value};
    }
    static Value index_value(const Value& value, size_t index) {
        const auto& values = array(value); if (index >= values.size()) throw Error("index out of range (" + std::to_string(index) + " >= " + std::to_string(values.size()) + ")"); return *values[index];
    }
    static Value object_value(const Value& value, const std::string& key) {
        const auto& object = std::get_if<Object>(&value);
        if (!object) throw Error("expected an object");
        auto found = object->find(key); if (found == object->end()) throw Error("object key not found: " + key);
        return *found->second;
    }
    static Value matrix(const std::vector<Value>& args, double fill) {
        if (args.empty() || args.size() > 2) throw Error("matrix constructor expects rows and optional columns");
        int rows = static_cast<int>(number(args, 0)), cols = args.size() == 2 ? static_cast<int>(number(args, 1)) : 1;
        Array result; for (int row = 0; row < rows; ++row) {
            Array line; for (int column = 0; column < cols; ++column) line.push_back(std::make_shared<Value>(fill));
            result.push_back(std::make_shared<Value>(line));
        }
        return result;
    }
    static std::vector<std::vector<double>> numeric_matrix(const Value& value) {
        const auto& rows = array(value); if (rows.empty()) throw Error("matrix cannot be empty");
        std::vector<std::vector<double>> result; size_t width = 0;
        for (const auto& row_value : rows) {
            if (!std::holds_alternative<Array>(*row_value)) throw Error("expected a matrix");
            const auto& row = array(*row_value); if (!width) width = row.size();
            if (row.size() != width) throw Error("matrix must be rectangular");
            std::vector<double> converted; for (const auto& item : row) converted.push_back(number(*item));
            result.push_back(std::move(converted));
        }
        if (result.size() != width) throw Error("matrix must be square");
        return result;
    }
    static Value matrix_value(const std::vector<std::vector<double>>& matrix) {
        Array result; for (const auto& row : matrix) { Array converted; for (double value : row) converted.push_back(std::make_shared<Value>(value)); result.push_back(std::make_shared<Value>(converted)); }
        return result;
    }
    static Value vector_value(const std::vector<double>& values) {
        Array result; for (double value : values) result.push_back(std::make_shared<Value>(value)); return result;
    }
    static double determinant(const Value& value) {
        auto matrix = numeric_matrix(value); const size_t n = matrix.size(); double result = 1;
        for (size_t column = 0; column < n; ++column) {
            size_t pivot = column; for (size_t row = column + 1; row < n; ++row) if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
            if (std::abs(matrix[pivot][column]) < 1e-12) return 0;
            if (pivot != column) { std::swap(matrix[pivot], matrix[column]); result = -result; }
            const double diagonal = matrix[column][column]; result *= diagonal;
            for (size_t row = column + 1; row < n; ++row) { const double factor = matrix[row][column] / diagonal; for (size_t k = column + 1; k < n; ++k) matrix[row][k] -= factor * matrix[column][k]; }
        }
        return result;
    }
    static std::vector<std::vector<double>> inverse_matrix(const Value& value) {
        auto matrix = numeric_matrix(value); const size_t n = matrix.size();
        std::vector<std::vector<double>> augmented(n, std::vector<double>(2 * n, 0));
        for (size_t row = 0; row < n; ++row) for (size_t column = 0; column < n; ++column) { augmented[row][column] = matrix[row][column]; augmented[row][n + column] = row == column ? 1 : 0; }
        for (size_t column = 0; column < n; ++column) {
            size_t pivot = column; for (size_t row = column + 1; row < n; ++row) if (std::abs(augmented[row][column]) > std::abs(augmented[pivot][column])) pivot = row;
            if (std::abs(augmented[pivot][column]) < 1e-12) throw Error("matrix is singular");
            std::swap(augmented[pivot], augmented[column]); const double diagonal = augmented[column][column];
            for (double& item : augmented[column]) item /= diagonal;
            for (size_t row = 0; row < n; ++row) if (row != column) { const double factor = augmented[row][column]; for (size_t k = 0; k < 2 * n; ++k) augmented[row][k] -= factor * augmented[column][k]; }
        }
        std::vector<std::vector<double>> result(n, std::vector<double>(n)); for (size_t row = 0; row < n; ++row) for (size_t column = 0; column < n; ++column) result[row][column] = augmented[row][n + column]; return result;
    }
    static std::vector<double> solve_linear(const Value& matrix_value_input, const Value& vector_value_input) {
        auto matrix = numeric_matrix(matrix_value_input); const auto& vector = array(vector_value_input); const size_t n = matrix.size();
        if (vector.size() != n) throw Error("solve vector length does not match matrix");
        std::vector<std::vector<double>> augmented(n, std::vector<double>(n + 1));
        for (size_t row = 0; row < n; ++row) { augmented[row] = matrix[row]; augmented[row][n] = number(*vector[row]); }
        for (size_t column = 0; column < n; ++column) {
            size_t pivot = column; for (size_t row = column + 1; row < n; ++row) if (std::abs(augmented[row][column]) > std::abs(augmented[pivot][column])) pivot = row;
            if (std::abs(augmented[pivot][column]) < 1e-12) throw Error("linear system is singular");
            std::swap(augmented[pivot], augmented[column]); const double diagonal = augmented[column][column];
            for (size_t k = column; k <= n; ++k) augmented[column][k] /= diagonal;
            for (size_t row = 0; row < n; ++row) if (row != column) { const double factor = augmented[row][column]; for (size_t k = column; k <= n; ++k) augmented[row][k] -= factor * augmented[column][k]; }
        }
        std::vector<double> result(n); for (size_t row = 0; row < n; ++row) result[row] = augmented[row][n]; return result;
    }
    static Value arithmetic(const std::string& op, const Value& left, const Value& right) {
        const auto left_array = std::get_if<Array>(&left), right_array = std::get_if<Array>(&right);
        if ((op == "*" || op == "@") && left_array && right_array) return matrix_multiply(left, right);
        if (op == "*" && (left_array || right_array)) return elementwise(op, left, right);
        if (left_array || right_array) return elementwise(op, left, right);
        if (op == "+" && (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right)))
            return display(left) + display(right);
        double a = number(left), b = number(right);
        if (op == "+") return a + b; if (op == "-") return a - b; if (op == "*" || op == ".*") return a * b;
        if (op == "/" || op == "./") return a / b;
        if (op == "%") return std::fmod(a, b);
        return std::pow(a, b);
    }
    static Value elementwise(const std::string& op, const Value& left, const Value& right) {
        const auto left_array = std::get_if<Array>(&left), right_array = std::get_if<Array>(&right);
        if (left_array && right_array && left_array->size() != right_array->size()) throw Error("array operands must have equal lengths");
        if (left_array) { Array result; for (size_t i = 0; i < left_array->size(); ++i) result.push_back(std::make_shared<Value>(elementwise(op, *(*left_array)[i], right_array ? *(*right_array)[i] : right))); return result; }
        if (right_array) { Array result; for (const auto& item : *right_array) result.push_back(std::make_shared<Value>(elementwise(op, left, *item))); return result; }
        return arithmetic(op, left, right);
    }
    static Value matrix_multiply(const Value& left, const Value& right) {
        const auto& a = array(left); const auto& b = array(right);
        if (a.empty() || b.empty()) return Array{};
        const bool left_matrix = std::holds_alternative<Array>(*a[0]);
        const bool right_matrix = std::holds_alternative<Array>(*b[0]);
        if (!left_matrix && !right_matrix) {
            if (a.size() != b.size()) throw Error("matrix dimensions do not align");
            double result = 0; for (size_t i = 0; i < a.size(); ++i) result += number(*a[i]) * number(*b[i]); return result;
        }
        if (left_matrix && !right_matrix) {
            const auto& first_row = array(*a[0]); if (first_row.size() != b.size()) throw Error("matrix dimensions do not align");
            Array result;
            for (const auto& row_value : a) {
                const auto& row = array(*row_value); if (row.size() != b.size()) throw Error("matrix must be rectangular");
                double sum = 0; for (size_t k = 0; k < b.size(); ++k) sum += number(*row[k]) * number(*b[k]);
                result.push_back(std::make_shared<Value>(sum));
            }
            return result;
        }
        if (!left_matrix && right_matrix) {
            if (a.size() != b.size()) throw Error("matrix dimensions do not align");
            const auto& first_row = array(*b[0]); const size_t columns = first_row.size();
            Array result; for (size_t column = 0; column < columns; ++column) {
                double sum = 0; for (size_t row = 0; row < b.size(); ++row) { const auto& values = array(*b[row]); if (values.size() != columns) throw Error("matrix must be rectangular"); sum += number(*a[row]) * number(*values[column]); }
                result.push_back(std::make_shared<Value>(sum));
            }
            return result;
        }
        const auto& first_row = array(*a[0]); const size_t inner = first_row.size(), columns = array(*b[0]).size();
        for (const auto& row_value : a) if (array(*row_value).size() != inner) throw Error("matrix must be rectangular");
        for (const auto& row_value : b) if (array(*row_value).size() != columns) throw Error("matrix must be rectangular");
        if (inner != b.size()) throw Error("matrix dimensions do not align");
        Array result;
        for (const auto& row_value : a) {
            const auto& row = array(*row_value); Array output;
            for (size_t column = 0; column < columns; ++column) { double sum = 0; for (size_t k = 0; k < inner; ++k) sum += number(*row[k]) * number(*array(*b[k])[column]); output.push_back(std::make_shared<Value>(sum)); }
            result.push_back(std::make_shared<Value>(output));
        }
        return result;
    }
    static bool truthy(const Value& value) { if (auto boolean = std::get_if<bool>(&value)) return *boolean; if (auto number = std::get_if<double>(&value)) return *number != 0; if (std::holds_alternative<std::nullptr_t>(value)) return false; return true; }
    static bool equal_values(const Value& left, const Value& right) {
        if (left.index() != right.index()) return false;
        if (auto left_array = std::get_if<Array>(&left)) {
            const auto& right_array = std::get<Array>(right); if (left_array->size() != right_array.size()) return false;
            for (size_t index = 0; index < left_array->size(); ++index) if (!equal_values(*(*left_array)[index], *right_array[index])) return false;
            return true;
        }
        if (auto left_object = std::get_if<Object>(&left)) {
            const auto& right_object = std::get<Object>(right); if (left_object->size() != right_object.size()) return false;
            for (const auto& entry : *left_object) {
                auto found = right_object.find(entry.first); if (found == right_object.end() || !equal_values(*entry.second, *found->second)) return false;
            }
            return true;
        }
        return left == right;
    }
    static std::string display(const Value& value) {
        if (auto number = std::get_if<double>(&value)) { std::ostringstream stream; stream << *number; return stream.str(); }
        if (auto text = std::get_if<std::string>(&value)) return *text;
        if (auto boolean = std::get_if<bool>(&value)) return *boolean ? "true" : "false";
        if (auto object = std::get_if<Object>(&value)) { std::string result = "{"; bool first = true; for (const auto& entry : *object) { if (!first) result += ", "; first = false; result += entry.first + ": " + display(*entry.second); } return result + "}"; }
        std::string result = "["; bool first = true; for (const auto& item : std::get<Array>(value)) { if (!first) result += ", "; first = false; result += display(*item); } return result + "]";
    }
    static std::string json_quote(const std::string& text) {
        std::string result = "\"";
        for (unsigned char character : text) {
            if (character == '\\') result += "\\\\";
            else if (character == '"') result += "\\\"";
            else if (character == '\n') result += "\\n";
            else if (character == '\r') result += "\\r";
            else if (character == '\t') result += "\\t";
            else if (character == '\b') result += "\\b";
            else if (character == '\f') result += "\\f";
            else if (character < 0x20) { std::ostringstream escaped; escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(character); result += escaped.str(); }
            else result += static_cast<char>(character);
        }
        return result + "\"";
    }
    static std::string json(const Value& value) {
        if (auto number = std::get_if<double>(&value)) { std::ostringstream output; output << *number; return output.str(); }
        if (auto text = std::get_if<std::string>(&value)) return json_quote(*text);
        if (auto boolean = std::get_if<bool>(&value)) return *boolean ? "true" : "false";
        if (std::holds_alternative<std::nullptr_t>(value)) return "null";
        if (auto object = std::get_if<Object>(&value)) { std::string result = "{"; bool first = true; for (const auto& entry : *object) { if (!first) result += ","; first = false; result += json_quote(entry.first) + ":" + json(*entry.second); } return result + "}"; }
        if (auto array = std::get_if<Array>(&value)) { std::string result = "["; for (size_t index = 0; index < array->size(); ++index) { if (index) result += ","; result += json(*(*array)[index]); } return result + "]"; }
        return "null";
    }
    static void json_space(const std::string& input, size_t& position) { while (position < input.size() && std::isspace(static_cast<unsigned char>(input[position]))) ++position; }
    static Value parse_json(const std::string& input, size_t& position) {
        json_space(input, position); if (position >= input.size()) throw Error("unexpected end of JSON");
        if (input[position] == '{') {
            ++position; Object object; json_space(input, position);
            while (position < input.size() && input[position] != '}') {
                Value key = parse_json(input, position); auto key_text = std::get_if<std::string>(&key); if (!key_text) throw Error("JSON object key must be a string");
                json_space(input, position); if (position >= input.size() || input[position] != ':') throw Error("JSON object missing colon"); ++position;
                object[*key_text] = std::make_shared<Value>(parse_json(input, position)); json_space(input, position);
                if (position < input.size() && input[position] == ',') { ++position; json_space(input, position); } else break;
            }
            if (position >= input.size() || input[position] != '}') throw Error("unterminated JSON object"); ++position; return object;
        }
        if (input[position] == '[') {
            ++position; Array values; json_space(input, position);
            while (position < input.size() && input[position] != ']') { values.push_back(std::make_shared<Value>(parse_json(input, position))); json_space(input, position); if (position < input.size() && input[position] == ',') { ++position; json_space(input, position); } else break; }
            if (position >= input.size() || input[position] != ']') throw Error("unterminated JSON array"); ++position; return values;
        }
        if (input[position] == '"') {
            ++position; std::string value;
            while (position < input.size() && input[position] != '"') {
                if (input[position] == '\\' && position + 1 < input.size()) {
                    ++position; const char escaped = input[position++];
                    if (escaped == 'n') value += '\n'; else if (escaped == 'r') value += '\r'; else if (escaped == 't') value += '\t';
                    else if (escaped == 'b') value += '\b'; else if (escaped == 'f') value += '\f'; else value += escaped;
                } else value += input[position++];
            }
            if (position >= input.size()) throw Error("unterminated JSON string"); ++position; return value;
        }
        if (input.compare(position, 4, "true") == 0) { position += 4; return true; }
        if (input.compare(position, 5, "false") == 0) { position += 5; return false; }
        if (input.compare(position, 4, "null") == 0) { position += 4; return nullptr; }
        size_t end = position; while (end < input.size() && std::string("0123456789+-.eE").find(input[end]) != std::string::npos) ++end;
        if (end == position) throw Error("invalid JSON value"); double value = std::stod(input.substr(position, end - position)); position = end; return value;
    }
    std::shared_ptr<Environment> environment_;
    std::string base_dir_;
    std::shared_ptr<std::set<std::string>> import_stack_;
};

namespace {

std::string ast_escape(const std::string& value) {
    std::ostringstream output;
    output << '"';
    for (unsigned char character : value) {
        if (character == '"') output << "\\\"";
        else if (character == '\\') output << "\\\\";
        else if (character == '\n') output << "\\n";
        else if (character == '\r') output << "\\r";
        else if (character == '\t') output << "\\t";
        else if (character < 0x20) output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(character) << std::dec << std::setfill(' ');
        else output << character;
    }
    output << '"';
    return output.str();
}

std::string ast_expression(const marila::ExprPtr& expression);

std::string ast_expressions(const std::vector<marila::ExprPtr>& expressions) {
    std::string result = "[";
    for (size_t index = 0; index < expressions.size(); ++index) {
        if (index) result += ',';
        result += ast_expression(expressions[index]);
    }
    return result + ']';
}

std::string ast_expression(const marila::ExprPtr& expression) {
    return std::visit([](const auto& node) -> std::string {
        using Node = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<Node, marila::Expr::Literal>) {
            return std::visit([](const auto& value) -> std::string {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, std::string>) return "{\"kind\":\"string\",\"value\":" + ast_escape(value) + "}";
                else if constexpr (std::is_same_v<Value, bool>) return std::string("{\"kind\":\"bool\",\"value\":") + (value ? "true}" : "false}");
                else if constexpr (std::is_same_v<Value, std::nullptr_t>) return "{\"kind\":\"none\"}";
                else { std::ostringstream number; number << std::setprecision(17) << value; return "{\"kind\":\"number\",\"value\":" + number.str() + "}"; }
            }, node.value);
        } else if constexpr (std::is_same_v<Node, marila::Expr::Name>) {
            return "{\"kind\":\"name\",\"value\":" + ast_escape(node.value) + "}";
        } else if constexpr (std::is_same_v<Node, marila::Expr::Array>) {
            return "{\"kind\":\"array\",\"values\":" + ast_expressions(node.values) + "}";
        } else if constexpr (std::is_same_v<Node, marila::Expr::Object>) {
            std::string values = "[";
            for (size_t index = 0; index < node.values.size(); ++index) {
                if (index) values += ',';
                values += "{\"key\":" + ast_escape(node.values[index].first) + ",\"value\":" + ast_expression(node.values[index].second) + "}";
            }
            return "{\"kind\":\"object\",\"values\":" + values + "]}";
        } else if constexpr (std::is_same_v<Node, marila::Expr::Unary>) {
            return "{\"kind\":\"unary\",\"op\":" + ast_escape(node.op) + ",\"value\":" + ast_expression(node.value) + "}";
        } else if constexpr (std::is_same_v<Node, marila::Expr::Binary>) {
            return "{\"kind\":\"binary\",\"op\":" + ast_escape(node.op) + ",\"left\":" + ast_expression(node.left) + ",\"right\":" + ast_expression(node.right) + "}";
        } else if constexpr (std::is_same_v<Node, marila::Expr::Call>) {
            return "{\"kind\":\"call\",\"callee\":" + ast_expression(node.callee) + ",\"args\":" + ast_expressions(node.args) + "}";
        } else if constexpr (std::is_same_v<Node, marila::Expr::Index>) {
            return "{\"kind\":\"index\",\"collection\":" + ast_expression(node.collection) + ",\"index\":" + ast_expression(node.index) + "}";
        } else if constexpr (std::is_same_v<Node, marila::Expr::Range>) {
            return "{\"kind\":\"range\",\"start\":" + ast_expression(node.start) + ",\"step\":" + ast_expression(node.step) + ",\"stop\":" + ast_expression(node.stop) + "}";
        } else {
            std::string parameters = "[";
            for (size_t index = 0; index < node.parameters.size(); ++index) { if (index) parameters += ','; parameters += ast_escape(node.parameters[index]); }
            return "{\"kind\":\"lambda\",\"parameters\":" + parameters + "],\"body\":" + ast_expression(node.body) + "}";
        }
    }, expression->value);
}

std::string ast_statements(const std::vector<marila::Statement>& statements) {
    std::string result = "[";
    for (size_t index = 0; index < statements.size(); ++index) {
        if (index) result += ',';
        const auto& statement = statements[index];
        std::string kind;
        switch (statement.kind) {
        case marila::Statement::Kind::Let: kind = "let"; break;
        case marila::Statement::Kind::Assign: kind = "assign"; break;
        case marila::Statement::Kind::AssignIndex: kind = "assign_index"; break;
        case marila::Statement::Kind::Print: kind = "print"; break;
        case marila::Statement::Kind::Expression: kind = "expression"; break;
        case marila::Statement::Kind::If: kind = "if"; break;
        case marila::Statement::Kind::While: kind = "while"; break;
        case marila::Statement::Kind::For: kind = "for"; break;
        case marila::Statement::Kind::Function: kind = "function"; break;
        case marila::Statement::Kind::Return: kind = "return"; break;
        case marila::Statement::Kind::Import: kind = "import"; break;
        case marila::Statement::Kind::Break: kind = "break"; break;
        case marila::Statement::Kind::Continue: kind = "continue"; break;
        case marila::Statement::Kind::Try: kind = "try"; break;
        case marila::Statement::Kind::Throw: kind = "throw"; break;
        }
        result += "{\"kind\":" + ast_escape(kind);
        if (!statement.name.empty()) result += ",\"name\":" + ast_escape(statement.name);
        if (!statement.type.empty()) result += ",\"type\":" + ast_escape(statement.type);
        if (!statement.module.empty()) result += ",\"module\":" + ast_escape(statement.module);
        if (statement.expression) result += ",\"expression\":" + ast_expression(statement.expression);
        if (statement.target) result += ",\"target\":" + ast_expression(statement.target);
        if (!statement.parameters.empty()) {
            result += ",\"parameters\":[";
            for (size_t parameter = 0; parameter < statement.parameters.size(); ++parameter) { if (parameter) result += ','; result += ast_escape(statement.parameters[parameter]); }
            result += "]";
        }
        if (!statement.body.empty()) result += ",\"body\":" + ast_statements(statement.body);
        if (!statement.alternate.empty()) result += ",\"else\":" + ast_statements(statement.alternate);
        result += '}';
    }
    return result + ']';
}

std::string ast_program(const std::vector<marila::Statement>& statements) {
    return "{\"kind\":\"program\",\"statements\":" + ast_statements(statements) + "}\n";
}

std::string ast_number(double value) {
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    return stream.str();
}

std::string visual_kind(marila::SceneNode::Kind kind) {
    switch (kind) {
    case marila::SceneNode::Kind::Point: return "point";
    case marila::SceneNode::Kind::Move: return "move";
    case marila::SceneNode::Kind::Fade: return "fade";
    case marila::SceneNode::Kind::Line: return "line";
    case marila::SceneNode::Kind::Triangle: return "triangle";
    case marila::SceneNode::Kind::Circle: return "circle";
    case marila::SceneNode::Kind::Plot2D: return "plot2d";
    case marila::SceneNode::Kind::Data2D: return "data2d";
    case marila::SceneNode::Kind::Parametric2D: return "parametric2d";
    case marila::SceneNode::Kind::VectorField2D: return "vectorfield2d";
    case marila::SceneNode::Kind::Implicit2D: return "implicit2d";
    case marila::SceneNode::Kind::Plot3D: return "plot3d";
    case marila::SceneNode::Kind::Data3D: return "data3d";
    case marila::SceneNode::Kind::Plot4D: return "plot4d";
    case marila::SceneNode::Kind::Sphere: return "sphere";
    case marila::SceneNode::Kind::Cube: return "cube";
    case marila::SceneNode::Kind::Morph: return "morph";
    }
    return "unknown";
}

std::string ast_visual_node(const marila::SceneNode& node) {
    std::string result = "{\"kind\":" + ast_escape(visual_kind(node.kind));
    auto add_string = [&result](const char* key, const std::string& value) {
        if (!value.empty()) result += ",\"" + std::string(key) + "\":" + ast_escape(value);
    };
    auto add_number = [&result](const char* key, double value) {
        result += ",\"" + std::string(key) + "\":" + ast_number(value);
    };
    add_string("id", node.id); add_string("first", node.first); add_string("second", node.second);
    add_string("third", node.third); add_string("label", node.label); add_string("title", node.title);
    add_string("color", node.color); add_string("expression", node.expression);
    if (node.kind == marila::SceneNode::Kind::Move || node.kind == marila::SceneNode::Kind::Fade || node.kind == marila::SceneNode::Kind::Morph) add_string("easing", node.easing);
    if (node.kind == marila::SceneNode::Kind::Data2D) {
        result += ",\"samples\":[";
        for (size_t index = 0; index < node.samples.size(); ++index) {
            if (index) result += ',';
            result += "[" + ast_number(node.samples[index].first) + "," + ast_number(node.samples[index].second) + "]";
        }
        result += "]";
    }
    if (node.kind == marila::SceneNode::Kind::Data3D) {
        result += ",\"grid\":[";
        for (size_t row = 0; row < node.grid.size(); ++row) {
            if (row) result += ',';
            result += '[';
            for (size_t column = 0; column < node.grid[row].size(); ++column) { if (column) result += ','; result += ast_number(node.grid[row][column]); }
            result += ']';
        }
        result += "]";
    }
    if (node.kind == marila::SceneNode::Kind::Point || node.kind == marila::SceneNode::Kind::Sphere || node.kind == marila::SceneNode::Kind::Cube) {
        add_number("x", node.x); add_number("y", node.y); add_number("z", node.z);
    }
    if (node.kind == marila::SceneNode::Kind::Move) {
        add_number("from_x", node.from_x); add_number("from_y", node.from_y); add_number("x", node.x); add_number("y", node.y); add_number("duration", node.duration);
    }
    if (node.kind == marila::SceneNode::Kind::Fade) { add_number("duration", node.duration); add_string("target", node.first); if (node.fade_in) result += ",\"fade_in\":true"; else result += ",\"fade_in\":false"; }
    if (node.kind == marila::SceneNode::Kind::Circle) add_number("radius", node.radius);
    if (node.kind == marila::SceneNode::Kind::Cube) add_number("size", node.size);
    if (node.kind == marila::SceneNode::Kind::Sphere) add_number("radius", node.radius);
    if (node.kind == marila::SceneNode::Kind::Morph) { add_string("from", node.first); add_string("to", node.second); add_number("from_size", node.x); add_number("to_size", node.y); add_number("duration", node.duration); }
    if (node.kind == marila::SceneNode::Kind::Plot2D || node.kind == marila::SceneNode::Kind::Parametric2D || node.kind == marila::SceneNode::Kind::VectorField2D || node.kind == marila::SceneNode::Kind::Implicit2D || node.kind == marila::SceneNode::Kind::Plot3D) {
        add_number("xmin", node.xmin); add_number("xmax", node.xmax); add_number("ymin", node.ymin); add_number("ymax", node.ymax);
    }
    if (node.dashed) result += ",\"dashed\":true";
    if (node.show_angles) result += ",\"show_angles\":true";
    if (node.animate_trace) result += ",\"animate_trace\":true";
    return result + "}";
}

std::string ast_visual(const std::string& type, const std::vector<marila::SceneNode>& nodes) {
    std::string result = "{\"kind\":\"visual-program\",\"type\":" + ast_escape(type) + ",\"nodes\":[";
    for (size_t index = 0; index < nodes.size(); ++index) { if (index) result += ','; result += ast_visual_node(nodes[index]); }
    return result + "]}\n";
}

}  // namespace

} // namespace marila

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: marila-native <run|test|check|ast|render|format|lint|diagnose|repl> [file.marila] [output]\n";
        return 2;
    }
    try {
        const std::string command = argv[1];
        if (command == "version" || command == "--version") {
            std::cout << "Marila native 0.1.0\n";
            return 0;
        }
        if (command == "help" || command == "--help") {
            std::cout << "Marila native 0.1.0\n"
                      << "Usage: marila-native <run|test|check|ast|render|format|lint|diagnose|repl> [file.marila] [output]\n"
                      << "  run     execute a program or render a typed source file\n"
                      << "  test    execute a program and report a passing test\n"
                      << "  check   parse or validate a source file\n"
                      << "  ast     emit a machine-readable program syntax tree\n"
                      << "  render  write a renderer output file\n"
                      << "  format  format source indentation\n"
                      << "  lint    report source issues\n"
                      << "  diagnose emit structured JSON diagnostics\n"
                      << "  repl    start an interactive program session\n";
            return 0;
        }
        if (command == "repl") {
            marila::Runtime runtime;
            std::string line;
            while (std::cout << "marila> " && std::getline(std::cin, line) && line != "exit" && line != "quit") {
                if (line.empty()) continue;
                try {
                    runtime.run(marila::Parser(marila::Lexer(line + "\n").tokenize()).parse());
                } catch (const std::exception& error) {
                    std::cerr << "Marila repl error: " << error.what() << '\n';
                }
            }
            return 0;
        }
        if (argc < 3) throw marila::Error("a source file is required");
        std::ifstream input(argv[2]);
        if (!input) throw marila::Error("unable to open " + std::string(argv[2]));
        std::stringstream source_stream; source_stream << input.rdbuf();
        const std::string source = source_stream.str();
        if (command == "render") {
            const std::string output = argc >= 4 ? argv[3] : "marila-output.html";
            marila::render_source(source, output);
            std::cout << "Rendered " << output << '\n';
        } else if (command == "format") {
            const std::string type = marila::detect_file_type(source);
            if (type == "program") {
                marila::Parser(marila::Lexer(source).tokenize()).parse();
            } else {
                if (marila::lint_source(source)) return 1;
                const bool supported = type == "document" || type == "latex" || type == "tikz" || type == "sheet" || type == "image" || type == "video" || type == "scene";
                if (!supported) throw marila::Error("unsupported file type '" + type + "'");
                if (source.find('[') == std::string::npos || source.find(']') == std::string::npos)
                    throw marila::Error("renderer source requires a bracketed statement block");
                if ((type == "image" || type == "scene" || type == "video") && marila::parse_visual_source(source).empty())
                    throw marila::Error("renderer source contains no recognized visual nodes");
            }
            std::cout << marila::format_source(source);
        } else if (command == "ast") {
            const std::string type = marila::detect_file_type(source);
            if (type == "program") {
                auto program = marila::Parser(marila::Lexer(source).tokenize()).parse();
                std::cout << marila::ast_program(program);
            } else {
                std::cout << marila::ast_visual(type, marila::parse_visual_source(source));
            }
        } else if (command == "lint") {
            const int lint_status = marila::lint_source(source);
            if (lint_status) return lint_status;
            if (marila::detect_file_type(source) == "program") {
                auto parsed = marila::Parser(marila::Lexer(source).tokenize()).parse();
                std::cout << "OK: linted " << parsed.size() << " statement(s)\n";
            }
            return 0;
        } else if (command == "diagnose") {
            const std::string type = marila::detect_file_type(source);
            try {
                const bool supported = type == "program" || type == "document" || type == "latex" || type == "tikz" || type == "sheet" || type == "image" || type == "video" || type == "scene";
                if (!supported) throw marila::Error("unsupported file type '" + type + "'");
                if (type == "program") {
                    auto parsed = marila::Parser(marila::Lexer(source).tokenize()).parse();
                    std::cout << "{\"ok\":true,\"type\":" << marila::ast_escape(type) << ",\"statements\":" << parsed.size() << ",\"diagnostics\":[]}\n";
                } else {
                    if (source.find('[') == std::string::npos || source.find(']') == std::string::npos) throw marila::Error("renderer source requires a bracketed statement block");
                    const auto nodes = marila::parse_visual_source(source);
                if ((type == "image" || type == "scene" || type == "video") && nodes.empty()) throw marila::Error("renderer source contains no recognized visual nodes");
                    std::cout << "{\"ok\":true,\"type\":" << marila::ast_escape(type) << ",\"nodes\":" << nodes.size() << ",\"diagnostics\":[]}\n";
                }
                return 0;
            } catch (const std::exception& error) {
                std::cout << "{\"ok\":false,\"type\":" << marila::ast_escape(type) << ",\"diagnostics\":[{\"message\":" << marila::ast_escape(error.what()) << "}]}\n";
                return 1;
            }
        } else {
            const std::string type = marila::detect_file_type(source);
            if (command == "check" && type != "program") {
                if (marila::lint_source(source)) return 1;
                if (type != "document" && type != "latex" && type != "tikz" && type != "sheet" && type != "image" && type != "video" && type != "scene")
                    throw marila::Error("unsupported file type '" + type + "'");
                if (source.find('[') == std::string::npos || source.find(']') == std::string::npos)
                    throw marila::Error("renderer source requires a bracketed statement block");
                if ((type == "image" || type == "scene" || type == "video") && marila::parse_visual_source(source).empty())
                    throw marila::Error("renderer source contains no recognized visual nodes");
                std::cout << "OK: recognized " << type << " source\n";
                return 0;
            }
            if (command == "run" && type != "program") {
                const std::string output = argc >= 4 ? argv[3] :
                    (type == "image" ? "marila-output.svg" : (type == "latex" || type == "tikz") ? "marila-output.tex" : "marila-output.html");
                marila::render_source(source, output);
                std::cout << "Rendered " << output << '\n';
                return 0;
            }
            if (command == "test" && type != "program") throw marila::Error("test expects a program source");
            auto program = marila::Parser(marila::Lexer(source).tokenize()).parse();
            if (command == "check") {
                std::cout << "OK: parsed " << program.size() << " statement(s)\n";
            } else if (command == "run") {
                const std::filesystem::path source_path = std::filesystem::absolute(argv[2]);
                marila::Runtime runtime(nullptr, source_path.parent_path().string());
                runtime.run(program);
            } else if (command == "test") {
                const std::filesystem::path source_path = std::filesystem::absolute(argv[2]);
                marila::Runtime runtime(nullptr, source_path.parent_path().string());
                runtime.run(program);
                std::cout << "PASS: " << argv[2] << '\n';
            } else {
                throw marila::Error("unknown command '" + command + "'");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Marila native error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
