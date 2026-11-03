#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

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
            if (c == '#' || (c == '/' && peek(1) == '/')) {
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
    struct Unary { std::string op; ExprPtr value; };
    struct Binary { ExprPtr left; std::string op; ExprPtr right; };
    struct Call { ExprPtr callee; std::vector<ExprPtr> args; };
    std::variant<Literal, Name, Array, Unary, Binary, Call> value;
    template <typename T>
    explicit Expr(T node) : value(std::move(node)) {}
};

struct Statement {
    enum class Kind { Let, Assign, Print, Expression };
    Kind kind; std::string name; ExprPtr expression;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    std::vector<Statement> parse() {
        std::vector<Statement> statements;
        skip_lines();
        if (match("file")) { expect("type"); advance(); match(":"); skip_lines(); }
        if (match("[")) {
            while (!check("]") && !check_kind(Kind::End)) { skip_lines(); if (!check("]")) statements.push_back(statement()); skip_lines(); }
            expect("]");
        } else {
            while (!check_kind(Kind::End)) { statements.push_back(statement()); skip_lines(); }
        }
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
        if (match("let")) { std::string name = identifier(); expect("="); return {Statement::Kind::Let, name, expression()}; }
        if (match("print")) return {Statement::Kind::Print, "", expression()};
        ExprPtr left = expression();
        if (auto name = std::get_if<Expr::Name>(&left->value); name && match("="))
            return {Statement::Kind::Assign, name->value, expression()};
        return {Statement::Kind::Expression, "", left};
    }
    std::string identifier() {
        if (!check_kind(Kind::Identifier)) throw Error("line " + std::to_string(current().line) + ": expected identifier");
        std::string value = current().text; advance(); return value;
    }
    ExprPtr expression(int minimum = 0) {
        ExprPtr left = prefix();
        while (true) {
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
        if (check("(")) { advance(); ExprPtr value = expression(); expect(")"); return suffix(value); }
        if (check("[")) {
            advance(); std::vector<ExprPtr> values;
            while (!check("]")) { values.push_back(expression()); if (!match(",")) break; }
            expect("]"); return std::make_shared<Expr>(Expr::Array{values});
        }
        if (check_kind(Kind::Number)) { double value = current().number; advance(); return suffix(std::make_shared<Expr>(Expr::Literal{value})); }
        if (check_kind(Kind::String)) { std::string value = current().text; advance(); return suffix(std::make_shared<Expr>(Expr::Literal{value})); }
        if (check("true") || check("false")) { bool value = check("true"); advance(); return suffix(std::make_shared<Expr>(Expr::Literal{value})); }
        if (check_kind(Kind::Identifier)) { std::string value = current().text; advance(); return suffix(std::make_shared<Expr>(Expr::Name{value})); }
        throw Error("line " + std::to_string(current().line) + ": expected expression");
    }
    ExprPtr suffix(ExprPtr value) {
        while (match("(")) {
            std::vector<ExprPtr> args;
            while (!check(")")) { args.push_back(expression()); if (!match(",")) break; }
            expect(")"); value = std::make_shared<Expr>(Expr::Call{value, args});
        }
        return value;
    }
    std::vector<Token> tokens_; size_t position_{0};
};

struct Value;
using Array = std::vector<std::shared_ptr<Value>>;
struct Value : std::variant<double, std::string, bool, Array> {
    using std::variant<double, std::string, bool, Array>::variant;
};

class Runtime {
public:
    void run(const std::vector<Statement>& statements) {
        for (const auto& statement : statements) {
            if (statement.kind == Statement::Kind::Let || statement.kind == Statement::Kind::Assign)
                values_[statement.name] = evaluate(statement.expression);
            else if (statement.kind == Statement::Kind::Print)
                std::cout << display(evaluate(statement.expression)) << '\n';
            else evaluate(statement.expression);
        }
    }
private:
    Value evaluate(const ExprPtr& expression) {
        return std::visit([this](const auto& node) -> Value { return eval(node); }, expression->value);
    }
    Value eval(const Expr::Literal& node) { return std::visit([](const auto& value) -> Value { return value; }, node.value); }
    Value eval(const Expr::Name& node) {
        auto it = values_.find(node.value); if (it == values_.end()) throw Error("undefined name '" + node.value + "'");
        return it->second;
    }
    Value eval(const Expr::Array& node) { Array values; for (const auto& item : node.values) values.push_back(std::make_shared<Value>(evaluate(item))); return values; }
    Value eval(const Expr::Unary& node) {
        Value value = evaluate(node.value);
        if (node.op == "-") return -number(value);
        if (node.op == "!" || node.op == "not") return !truthy(value);
        throw Error("unsupported unary operator '" + node.op + "'");
    }
    Value eval(const Expr::Binary& node) {
        Value left = evaluate(node.left), right = evaluate(node.right);
        if (node.op == "and" || node.op == "&&") return truthy(left) && truthy(right);
        if (node.op == "or" || node.op == "||") return truthy(left) || truthy(right);
        if (node.op == "==") return left == right;
        if (node.op == "!=") return left != right;
        double a = number(left), b = number(right);
        if (node.op == "+") return a + b; if (node.op == "-") return a - b;
        if (node.op == "*" || node.op == ".*") return a * b; if (node.op == "/") return a / b;
        if (node.op == "%") return std::fmod(a, b); if (node.op == "^" || node.op == ".^") return std::pow(a, b);
        if (node.op == "<") return a < b; if (node.op == "<=") return a <= b;
        if (node.op == ">") return a > b; if (node.op == ">=") return a >= b;
        throw Error("unsupported binary operator '" + node.op + "'");
    }
    Value eval(const Expr::Call& node) {
        auto name = std::get_if<Expr::Name>(&node.callee->value);
        if (!name) throw Error("only named built-ins are supported by the native bootstrap");
        std::vector<double> args; for (const auto& arg : node.args) args.push_back(number(evaluate(arg)));
        if (name->value == "sin") return std::sin(args.at(0)); if (name->value == "cos") return std::cos(args.at(0));
        if (name->value == "tan") return std::tan(args.at(0)); if (name->value == "sqrt") return std::sqrt(args.at(0));
        if (name->value == "abs") return std::abs(args.at(0)); if (name->value == "exp") return std::exp(args.at(0));
        throw Error("unknown function '" + name->value + "'");
    }
    static double number(const Value& value) { if (auto number = std::get_if<double>(&value)) return *number; throw Error("expected a number"); }
    static bool truthy(const Value& value) { if (auto boolean = std::get_if<bool>(&value)) return *boolean; if (auto number = std::get_if<double>(&value)) return *number != 0; return true; }
    static std::string display(const Value& value) {
        if (auto number = std::get_if<double>(&value)) { std::ostringstream stream; stream << *number; return stream.str(); }
        if (auto text = std::get_if<std::string>(&value)) return *text;
        if (auto boolean = std::get_if<bool>(&value)) return *boolean ? "true" : "false";
        std::string result = "["; bool first = true; for (const auto& item : std::get<Array>(value)) { if (!first) result += ", "; first = false; result += display(*item); } return result + "]";
    }
    std::unordered_map<std::string, Value> values_;
};

} // namespace marila

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[1]) != "run") {
        std::cerr << "Usage: marila-native run <file.marila>\n";
        return 2;
    }
    std::ifstream input(argv[2]);
    if (!input) { std::cerr << "Unable to open " << argv[2] << '\n'; return 1; }
    std::stringstream source; source << input.rdbuf();
    try {
        marila::Runtime runtime;
        runtime.run(marila::Parser(marila::Lexer(source.str()).tokenize()).parse());
    } catch (const std::exception& error) {
        std::cerr << "Marila native error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
