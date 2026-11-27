#include <cmath>
#include <cctype>
#include <algorithm>
#include <complex>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <map>
#include <regex>
#include <random>
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
            if (c == '/' && peek(1) == '*') {
                const int comment_line = line_, comment_column = column_;
                advance(); advance();
                bool closed = false;
                while (position_ < source_.size()) {
                    if (peek() == '*' && peek(1) == '/') { advance(); advance(); closed = true; break; }
                    advance();
                }
                if (!closed) throw Error("line " + std::to_string(comment_line) + ", column " + std::to_string(comment_column) + ": unterminated block comment");
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
                pair == "&&" || pair == "||" || pair == "::" || pair == "+=" ||
                pair == "-=" || pair == "*=" || pair == "/=" || pair == "%=" ||
                pair == "++" || pair == "--") {
                advance(); advance(); result.push_back({Kind::Operator, pair, 0, line, column}); continue;
            }
            if (std::string("+-*/%^=<>!@&").find(c) != std::string::npos) {
                advance(); result.push_back({Kind::Operator, std::string(1, c), 0, line, column}); continue;
            }
            if (std::string("()[]{};,. :?").find(c) != std::string::npos) {
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
        int line = line_, column = column_; const std::string string_location = "line " + std::to_string(line) + ", column " + std::to_string(column) + ": "; advance();
        std::string value;
        while (peek() && peek() != quote) {
            if (peek() == '\\') {
                advance(); const char escaped = peek(); advance();
                if (escaped == 'n') value += '\n';
                else if (escaped == 'r') value += '\r';
                else if (escaped == 't') value += '\t';
                else if (escaped == 'b') value += '\b';
                else if (escaped == 'f') value += '\f';
                else if (escaped == 'u') {
                    unsigned int codepoint = 0;
                    for (int digit = 0; digit < 4; ++digit) {
                        if (!peek() || !std::isxdigit(static_cast<unsigned char>(peek()))) throw Error(string_location + "invalid Unicode escape");
                        codepoint = codepoint * 16 + static_cast<unsigned int>(std::stoi(std::string(1, peek()), nullptr, 16)); advance();
                    }
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF && peek() == '\\' && peek(1) == 'u') {
                        advance(); advance(); unsigned int low = 0;
                        for (int digit = 0; digit < 4; ++digit) {
                            if (!peek() || !std::isxdigit(static_cast<unsigned char>(peek()))) throw Error(string_location + "invalid Unicode surrogate escape");
                            low = low * 16 + static_cast<unsigned int>(std::stoi(std::string(1, peek()), nullptr, 16)); advance();
                        }
                        if (low < 0xDC00 || low > 0xDFFF) throw Error(string_location + "invalid Unicode surrogate pair");
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                    } else if (codepoint >= 0xD800 && codepoint <= 0xDFFF) throw Error(string_location + "invalid Unicode surrogate escape");
                    if (codepoint <= 0x7F) value += static_cast<char>(codepoint);
                    else if (codepoint <= 0x7FF) { value += static_cast<char>(0xC0 | (codepoint >> 6)); value += static_cast<char>(0x80 | (codepoint & 0x3F)); }
                    else if (codepoint <= 0xFFFF) { value += static_cast<char>(0xE0 | (codepoint >> 12)); value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)); value += static_cast<char>(0x80 | (codepoint & 0x3F)); }
                    else { value += static_cast<char>(0xF0 | (codepoint >> 18)); value += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)); value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)); value += static_cast<char>(0x80 | (codepoint & 0x3F)); }
                } else if (escaped == quote || escaped == '\\' || escaped == '/') value += escaped;
                else throw Error(string_location + "invalid escape sequence");
            } else { value += peek(); advance(); }
        }
        if (!peek()) throw Error(string_location + "unterminated string");
        advance(); return {Kind::String, value, 0, line, column};
    }
    Token read_number() {
        int line = line_, column = column_; std::string value; bool dot = false;
        while (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.') {
            if (peek() == '.' && dot) throw Error(location() + "invalid number");
            dot |= peek() == '.'; value += peek(); advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            value += peek(); advance();
            if (peek() == '+' || peek() == '-') { value += peek(); advance(); }
            if (!std::isdigit(static_cast<unsigned char>(peek()))) throw Error(location() + "invalid exponent");
            while (std::isdigit(static_cast<unsigned char>(peek()))) { value += peek(); advance(); }
        }
        double parsed = 0;
        try { parsed = std::stod(value); }
        catch (const std::out_of_range&) { throw Error("line " + std::to_string(line) + ", column " + std::to_string(column) + ": numeric literal out of range"); }
        catch (const std::exception&) { throw Error("line " + std::to_string(line) + ", column " + std::to_string(column) + ": invalid numeric literal"); }
        return {Kind::Number, value, parsed, line, column};
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
    struct Conditional { ExprPtr condition; ExprPtr when_true; ExprPtr when_false; };
    struct Lambda { std::vector<std::string> parameters; ExprPtr body; };
    std::variant<Literal, Name, Array, Object, Unary, Binary, Call, Index, Range, Conditional, Lambda> value;
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
    std::vector<Statement> finalizer;
    std::string module;
    std::string alias;
    std::string second_name;
    ExprPtr target;
    int line{0};
    int column{0};

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
                throw Error(location() + "expected file type");
            }
            advance();
            expect(":");
            skip_lines();
        }
        std::vector<Statement> statements = match("[") ? block("]") : block("");
        if (match("]")) {}
        if (match("render")) advance();
        skip_lines();
        if (!check_kind(Kind::End)) {
            throw Error(location() + "unexpected token after program: '" + current().text + "'");
        }
        return statements;
    }

private:
    const Token& current() const { return tokens_[position_]; }
    bool check(const std::string& value) const {
        return current().kind != Kind::String && current().kind != Kind::Number && current().text == value;
    }
    bool check_kind(Kind kind) const { return current().kind == kind; }
    bool match(const std::string& value) { if (check(value)) { ++position_; return true; } return false; }
    std::string location() const { return "line " + std::to_string(current().line) + ", column " + std::to_string(current().column) + ": "; }
    void expect(const std::string& value) { if (!match(value)) throw Error(location() + "expected '" + value + "'"); }
    void advance() { if (!check_kind(Kind::End)) ++position_; }
    void skip_lines() { while (check_kind(Kind::Newline) || check(";")) advance(); }

    Statement statement() {
        const bool prefix_increment = match("++");
        const bool prefix_decrement = !prefix_increment && match("--");
        if (prefix_increment || prefix_decrement) {
            const std::string name = identifier();
            const std::string operation = prefix_increment ? "+" : "-";
            return {Statement::Kind::Assign, name, std::make_shared<Expr>(Expr::Binary{std::make_shared<Expr>(Expr::Name{name}), operation, std::make_shared<Expr>(Expr::Literal{1.0})})};
        }
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
                throw Error("line " + std::to_string(module.line) + ", column " + std::to_string(module.column) + ": expected module name");
            Statement result{Statement::Kind::Import}; result.module = module.text;
            if (match("as")) result.alias = identifier();
            return result;
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
            Statement result{Statement::Kind::Try}; result.body = required_block(); skip_lines();
            bool has_catch = false;
            if (match("catch")) {
                has_catch = true;
                if (match("(")) { result.name = identifier(); expect(")"); }
                else if (check_kind(Kind::Identifier)) result.name = identifier();
                result.alternate = required_block();
                skip_lines();
            }
            if (match("finally")) result.finalizer = required_block();
            else if (!has_catch) throw Error(location() + "expected 'catch' or 'finally'");
            return result;
        }
        if (match("while")) {
            Statement result{Statement::Kind::While}; result.expression = expression(); result.body = required_block(); return result;
        }
        if (match("for")) {
            Statement result{Statement::Kind::For}; result.name = identifier();
            if (match(",")) result.second_name = identifier();
            expect("in");
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
        if (auto name = std::get_if<Expr::Name>(&left->value); name) {
            if (match("=")) return {Statement::Kind::Assign, name->value, expression()};
            for (const auto& operation : {std::string("+="), std::string("-="), std::string("*="), std::string("/="), std::string("%=")}) {
                if (match(operation)) {
                    const std::string arithmetic_operation(1, operation.front());
                    return {Statement::Kind::Assign, name->value, std::make_shared<Expr>(Expr::Binary{left, arithmetic_operation, expression()})};
                }
            }
            if (match("++") || match("--")) {
                const std::string arithmetic_operation = tokens_[position_ - 1].text == "++" ? "+" : "-";
                return {Statement::Kind::Assign, name->value, std::make_shared<Expr>(Expr::Binary{left, arithmetic_operation, std::make_shared<Expr>(Expr::Literal{1.0})})};
            }
        }
        if (std::holds_alternative<Expr::Index>(left->value) && match("=")) {
            Statement result{Statement::Kind::AssignIndex};
            result.target = left; result.expression = expression(); return result;
        }
        if (std::holds_alternative<Expr::Index>(left->value)) {
            for (const auto& operation : {std::string("+="), std::string("-="), std::string("*="), std::string("/="), std::string("%=")}) {
                if (match(operation)) {
                    Statement result{Statement::Kind::AssignIndex};
                    result.target = left;
                    result.expression = std::make_shared<Expr>(Expr::Binary{left, std::string(1, operation.front()), expression()});
                    return result;
                }
            }
        }
        return {Statement::Kind::Expression, "", left};
    }
    std::string identifier() {
        if (!check_kind(Kind::Identifier)) throw Error(location() + "expected identifier");
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
        if (current().kind != Kind::Operator && current().kind != Kind::Identifier) break;
            static const std::unordered_map<std::string, int> precedence{
                {"||",1},{"or",1},{"&&",2},{"and",2},{"==",3},{"!=",3},{"<",4},{"<=",4},
                {">",4},{">=",4},{"+",5},{"-",5},{"*",6},{"/",6},{"%",6},{".*",6},{"./",6},
                {".^",7},{"^",7},{"@",6}};
            auto it = precedence.find(current().text);
            if (it == precedence.end() || it->second < minimum) break;
            std::string op = current().text; advance();
            left = std::make_shared<Expr>(Expr::Binary{left, op, expression(it->second + (op == "^" ? 0 : 1))});
        }
        if (minimum == 0 && match("?")) {
            ExprPtr when_true = expression(1); expect(":");
            left = std::make_shared<Expr>(Expr::Conditional{left, when_true, expression(0)});
        }
        return left;
    }
    ExprPtr prefix() {
        if ((check_kind(Kind::Operator) || check_kind(Kind::Identifier)) && (check("-") || check("!") || check("not"))) {
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
                if (!starts_expression()) throw Error(location() + "expected array separator");
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
                else throw Error(location() + "expected object key");
                expect(":"); values.emplace_back(key, expression()); if (!match(",")) break; skip_lines();
            }
            skip_lines(); expect("}"); return std::make_shared<Expr>(Expr::Object{values});
        }
        if (check_kind(Kind::Number)) { double value = current().number; advance(); return suffix(std::make_shared<Expr>(Expr::Literal{value})); }
        if (check_kind(Kind::String)) { std::string value = current().text; advance(); return suffix(std::make_shared<Expr>(Expr::Literal{value})); }
        if (check_kind(Kind::Identifier) && (check("true") || check("false"))) { bool value = check("true"); advance(); return suffix(std::make_shared<Expr>(Expr::Literal{value})); }
        if (check_kind(Kind::Identifier) && check("none")) { advance(); return suffix(std::make_shared<Expr>(Expr::Literal{nullptr})); }
        if (check_kind(Kind::Identifier)) {
            std::string value = current().text; advance();
            ExprPtr result = suffix(std::make_shared<Expr>(Expr::Name{value}));
            if (match("=>")) result = std::make_shared<Expr>(Expr::Lambda{{value}, expression()});
            return result;
        }
        throw Error(location() + "expected expression");
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
            } else if (match(".")) {
                const std::string member = identifier();
                value = std::make_shared<Expr>(Expr::Index{value, std::make_shared<Expr>(Expr::Literal{member})});
            } else break;
        }
        return value;
    }
    std::vector<Statement> required_block() {
        skip_lines();
        if (!match("{")) throw Error(location() + "expected '{'");
        return block("}");
    }
    std::vector<Statement> block(const std::string& terminator) {
        std::vector<Statement> result;
        skip_lines();
        while (!check_kind(Kind::End) && (terminator.empty() || !check(terminator))) {
            if (check("]") && terminator.empty()) break;
            const Token start = current();
            Statement parsed = statement();
            parsed.line = start.line;
            parsed.column = start.column;
            result.push_back(std::move(parsed));
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
struct Value : std::variant<double, std::complex<double>, std::string, bool, Array, Object, FunctionPtr, std::nullptr_t> {
    using std::variant<double, std::complex<double>, std::string, bool, Array, Object, FunctionPtr, std::nullptr_t>::variant;
};
struct Environment {
    std::unordered_map<std::string, Value> values;
    std::unordered_map<std::string, std::string> types;
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
        catch (const ReturnSignal&) { throw Error("return outside a function"); }
        catch (const BreakSignal&) { throw Error("break outside a loop"); }
        catch (const ContinueSignal&) { throw Error("continue outside a loop"); }
    }
private:
    struct ReturnSignal { Value value; };
    struct BreakSignal {};
    struct ContinueSignal {};
    void install_builtins() {
        environment_->values["pi"] = 3.14159265358979323846;
        environment_->values["e"] = 2.71828182845904523536;
        environment_->values["seed"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("seed expects one integer value");
            random_engine_.seed(static_cast<std::mt19937_64::result_type>(integer(args[0], "seed expects a non-negative integer")));
            return Value(nullptr);
        });
        auto random_values = [this](const std::vector<Value>& args, bool normal) -> Value {
            if (args.size() > 2) throw Error("random generator expects zero, one, or two dimensions");
            auto sample = [&]() { return normal ? std::normal_distribution<double>(0.0, 1.0)(random_engine_) : std::uniform_real_distribution<double>(0.0, 1.0)(random_engine_); };
            if (args.empty()) return Value(sample());
            const int rows = integer(args[0], "random rows must be a non-negative integer"); const int columns = args.size() == 2 ? integer(args[1], "random columns must be a non-negative integer") : rows;
            Array result;
            for (int row = 0; row < rows; ++row) { Array values; for (int column = 0; column < columns; ++column) values.push_back(std::make_shared<Value>(sample())); result.push_back(std::make_shared<Value>(values)); }
            return Value(result);
        };
        environment_->values["rand"] = native([random_values](const std::vector<Value>& args) mutable { return random_values(args, false); });
        environment_->values["randn"] = native([random_values](const std::vector<Value>& args) mutable { return random_values(args, true); });
        environment_->values["complex"] = native([](const std::vector<Value>& args) { if (args.size() != 2) throw Error("complex expects real and imaginary parts"); return map_binary(args[0], args[1], [](const Value& real_part, const Value& imaginary_part) { return Value(std::complex<double>(number(real_part), number(imaginary_part))); }); });
        environment_->values["real"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("real expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(value->real()); return Value(number(item)); }); });
        environment_->values["imag"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("imag expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(value->imag()); return Value(0.0); }); });
        environment_->values["conj"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("conj expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::conj(*value)); return item; }); });
        environment_->values["arg"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("arg expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::arg(*value)); return Value(0.0); }); });
        environment_->values["sin"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("sin expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::sin(*value)); return Value(std::sin(number(item))); }); });
        environment_->values["cos"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("cos expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::cos(*value)); return Value(std::cos(number(item))); }); });
        environment_->values["tan"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("tan expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::tan(*value)); return Value(std::tan(number(item))); }); });
        environment_->values["sinh"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("sinh expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::sinh(*value)); return Value(std::sinh(number(item))); }); });
        environment_->values["cosh"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("cosh expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::cosh(*value)); return Value(std::cosh(number(item))); }); });
        environment_->values["tanh"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("tanh expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::tanh(*value)); return Value(std::tanh(number(item))); }); });
        environment_->values["asinh"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("asinh expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::asinh(*value)); return Value(std::asinh(number(item))); }); });
        environment_->values["acosh"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("acosh expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::acosh(*value)); return Value(std::acosh(number(item))); }); });
        environment_->values["atanh"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("atanh expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::atanh(*value)); return Value(std::atanh(number(item))); }); });
        environment_->values["asin"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("asin expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::asin(*value)); return Value(std::asin(number(item))); }); });
        environment_->values["acos"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("acos expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::acos(*value)); return Value(std::acos(number(item))); }); });
        environment_->values["atan"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("atan expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::atan(*value)); return Value(std::atan(number(item))); }); });
        environment_->values["atan2"] = native([](const std::vector<Value>& args) { if (args.size() != 2) throw Error("atan2 expects two arguments"); return map_binary(args[0], args[1], [](const Value& y, const Value& x) { return Value(std::atan2(number(y), number(x))); }); });
        environment_->values["sqrt"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("sqrt expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::sqrt(*value)); return Value(std::sqrt(number(item))); }); });
        environment_->values["floor"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("floor expects one value"); return map_values(args[0], [](const Value& item) { return Value(std::floor(number(item))); }); });
        environment_->values["ceil"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("ceil expects one value"); return map_values(args[0], [](const Value& item) { return Value(std::ceil(number(item))); }); });
        environment_->values["abs"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("abs expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::abs(*value)); return Value(std::abs(number(item))); }); });
        environment_->values["exp"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("exp expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::exp(*value)); return Value(std::exp(number(item))); }); });
        environment_->values["log"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("log expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::log(*value)); return Value(std::log(number(item))); }); });
        environment_->values["log2"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("log2 expects one value"); return map_values(args[0], [](const Value& item) { return Value(std::log2(number(item))); }); });
        environment_->values["log10"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("log10 expects one value"); return map_values(args[0], [](const Value& item) { return Value(std::log10(number(item))); }); });
        environment_->values["pow"] = native([](const std::vector<Value>& args) { if (args.size() != 2) throw Error("pow expects two arguments"); return map_binary(args[0], args[1], [](const Value& base, const Value& exponent) { if (std::holds_alternative<std::complex<double>>(base) || std::holds_alternative<std::complex<double>>(exponent)) return Value(std::pow(as_complex(base), as_complex(exponent))); return Value(std::pow(number(base), number(exponent))); }); });
        environment_->values["sign"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("sign expects one value"); return map_values(args[0], [](const Value& item) { const double value = number(item); return Value(value > 0 ? 1.0 : (value < 0 ? -1.0 : 0.0)); }); });
        environment_->values["hypot"] = native([](const std::vector<Value>& args) { if (args.size() != 2) throw Error("hypot expects two arguments"); return map_binary(args[0], args[1], [](const Value& x, const Value& y) { return Value(std::hypot(number(x), number(y))); }); });
        environment_->values["isfinite"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("isfinite expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::isfinite(value->real()) && std::isfinite(value->imag())); return Value(std::isfinite(number(item))); }); });
        environment_->values["isnan"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("isnan expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::isnan(value->real()) || std::isnan(value->imag())); return Value(std::isnan(number(item))); }); });
        environment_->values["isinf"] = native([](const std::vector<Value>& args) { if (args.size() != 1) throw Error("isinf expects one value"); return map_values(args[0], [](const Value& item) { if (auto value = std::get_if<std::complex<double>>(&item)) return Value(std::isinf(value->real()) || std::isinf(value->imag())); return Value(std::isinf(number(item))); }); });
        environment_->values["isclose"] = native([](const std::vector<Value>& args) { if (args.size() < 2 || args.size() > 3) throw Error("isclose expects two values and an optional tolerance"); const double tolerance = args.size() == 3 ? number(args[2]) : 1e-9; if (tolerance < 0) throw Error("isclose tolerance cannot be negative"); return map_binary(args[0], args[1], [tolerance](const Value& left, const Value& right) { return Value(std::abs(as_complex(left) - as_complex(right)) <= tolerance); }); });
        environment_->values["clamp"] = native([](const std::vector<Value>& args) { if (args.size() != 3) throw Error("clamp expects a value, lower bound, and upper bound"); const double lower = number(args[1]), upper = number(args[2]); if (lower > upper) throw Error("clamp lower bound cannot exceed upper bound"); return map_values(args[0], [lower, upper](const Value& item) { return Value(std::max(lower, std::min(upper, number(item)))); }); });
        environment_->values["differentiate"] = native([this](const std::vector<Value>& args) {
            if (args.size() < 2 || args.size() > 3) throw Error("differentiate expects a function, point, and optional step");
            const double point = number(args, 1); const double step = args.size() == 3 ? number(args, 2) : 1e-5;
            if (step == 0) throw Error("differentiate step cannot be zero");
            const double right = number(invoke(args[0], {Value(point + step)}));
            const double left = number(invoke(args[0], {Value(point - step)}));
            return Value((right - left) / (2 * step));
        });
        environment_->values["jacobian"] = native([this](const std::vector<Value>& args) {
            if (args.size() < 2 || args.size() > 3) throw Error("jacobian expects a function, point vector, and optional step");
            const auto& point = array(args[1]); if (point.empty()) throw Error("jacobian point vector cannot be empty");
            const double step = args.size() == 3 ? number(args[2]) : 1e-5; if (step == 0) throw Error("jacobian step cannot be zero");
            auto evaluate_at = [&](const std::vector<double>& coordinates) {
                Array values; for (double coordinate : coordinates) values.push_back(std::make_shared<Value>(coordinate));
                const Value result = invoke(args[0], {Value(values)}); return Array(array(result));
            };
            std::vector<double> coordinates; for (const auto& value : point) coordinates.push_back(number(*value));
            const auto baseline = evaluate_at(coordinates); const size_t output_size = baseline.size(); std::vector<std::vector<double>> result(output_size, std::vector<double>(coordinates.size(), 0));
            for (size_t column = 0; column < coordinates.size(); ++column) {
                auto left = coordinates, right = coordinates; left[column] -= step; right[column] += step;
                const auto left_values = evaluate_at(left), right_values = evaluate_at(right); if (left_values.size() != output_size || right_values.size() != output_size) throw Error("jacobian function returned inconsistent vector lengths");
                for (size_t row = 0; row < output_size; ++row) result[row][column] = (number(*right_values[row]) - number(*left_values[row])) / (2 * step);
            }
            return matrix_value(result);
        });
        environment_->values["integrate"] = native([this](const std::vector<Value>& args) {
            if (args.size() < 3 || args.size() > 4) throw Error("integrate expects a function, lower bound, upper bound, and optional steps");
            const double lower = number(args, 1), upper = number(args, 2); int steps = args.size() == 4 ? integer(args[3], "integrate steps must be a non-negative integer") : 1000;
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
            if (auto bracket = std::get_if<Array>(&args[1])) {
                if (bracket->size() != 2) throw Error("root bracket must contain exactly two bounds");
                double lower = number(*bracket->at(0)), upper = number(*bracket->at(1));
                if (lower >= upper) throw Error("root bracket lower bound must be less than upper bound");
                const double tolerance = args.size() > 2 ? number(args[2]) : 1e-8; const int iterations = args.size() > 3 ? integer(args[3], "root iterations must be a non-negative integer") : 100;
                if (tolerance <= 0 || iterations < 1) throw Error("root tolerance and iterations must be positive");
                double lower_value = number(invoke(args[0], {Value(lower)})), upper_value = number(invoke(args[0], {Value(upper)}));
                if (std::abs(lower_value) <= tolerance) return Value(lower);
                if (std::abs(upper_value) <= tolerance) return Value(upper);
                if ((lower_value < 0) == (upper_value < 0)) throw Error("root bracket must contain a sign change");
                for (int iteration = 0; iteration < iterations; ++iteration) {
                    const double middle = (lower + upper) / 2; const double middle_value = number(invoke(args[0], {Value(middle)}));
                    if (std::abs(middle_value) <= tolerance || upper - lower <= tolerance) return Value(middle);
                    if ((lower_value < 0) == (middle_value < 0)) { lower = middle; lower_value = middle_value; } else { upper = middle; upper_value = middle_value; }
                }
                throw Error("root did not converge");
            }
            double x = number(args[1]); const double tolerance = args.size() > 2 ? number(args[2]) : 1e-8; const int iterations = args.size() > 3 ? integer(args[3], "root iterations must be a non-negative integer") : 100;
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
            Value state = args[1]; const double start = number(args[2]), end = number(args[3]); const int steps = args.size() == 5 ? integer(args[4], "ode_euler steps must be a non-negative integer") : 100;
            if (steps < 1) throw Error("ode_euler steps must be positive");
            const double width = (end - start) / steps; Array result;
            for (int index = 0; index <= steps; ++index) {
                const double time = start + width * index; Array sample; sample.push_back(std::make_shared<Value>(time)); sample.push_back(std::make_shared<Value>(state)); result.push_back(std::make_shared<Value>(sample));
                if (index < steps) state = arithmetic("+", state, arithmetic("*", invoke(args[0], {Value(time), state}), Value(width)));
            }
            return Value(result);
        });
        environment_->values["ode_rk4"] = native([this](const std::vector<Value>& args) {
            if (args.size() < 4 || args.size() > 5) throw Error("ode_rk4 expects a derivative, initial value, start, end, and optional steps");
            Value state = args[1]; const double start = number(args[2]), end = number(args[3]); const int steps = args.size() == 5 ? integer(args[4], "ode_rk4 steps must be a non-negative integer") : 100;
            if (steps < 1) throw Error("ode_rk4 steps must be positive");
            const double width = (end - start) / steps; auto derivative = [&](double time, const Value& value) { return invoke(args[0], {Value(time), value}); };
            Array result;
            for (int index = 0; index <= steps; ++index) {
                const double time = start + width * index; Array sample; sample.push_back(std::make_shared<Value>(time)); sample.push_back(std::make_shared<Value>(state)); result.push_back(std::make_shared<Value>(sample));
                if (index < steps) {
                    const Value k1 = derivative(time, state);
                    const Value k2 = derivative(time + width / 2, arithmetic("+", state, arithmetic("*", k1, Value(width / 2))));
                    const Value k3 = derivative(time + width / 2, arithmetic("+", state, arithmetic("*", k2, Value(width / 2))));
                    const Value k4 = derivative(time + width, arithmetic("+", state, arithmetic("*", k3, Value(width))));
                    const Value weighted = arithmetic("+", arithmetic("+", k1, arithmetic("*", k2, Value(2.0))), arithmetic("+", arithmetic("*", k3, Value(2.0)), k4));
                    state = arithmetic("+", state, arithmetic("*", weighted, Value(width / 6)));
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
                for (size_t sample = 0; sample < size; ++sample) { const double angle = tau * frequency * sample / size; const auto value = as_complex(*input[sample]); re += value.real() * std::cos(angle) + value.imag() * std::sin(angle); im += value.imag() * std::cos(angle) - value.real() * std::sin(angle); }
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
            for (size_t sample = 0; sample < size; ++sample) { double re = 0, im = 0; for (size_t frequency = 0; frequency < size; ++frequency) { const double angle = tau * frequency * sample / size; const double real_value = number(*real[frequency]), imaginary_value = number(*imaginary[frequency]); re += real_value * std::cos(angle) - imaginary_value * std::sin(angle); im += real_value * std::sin(angle) + imaginary_value * std::cos(angle); } re /= size; im /= size; if (std::abs(re) < 1e-12) re = 0; if (std::abs(im) < 1e-12) im = 0; if (im == 0) result.push_back(std::make_shared<Value>(re)); else result.push_back(std::make_shared<Value>(std::complex<double>(re, im))); }
            return Value(result);
        });
        auto shift_spectrum = [](const std::vector<Value>& args, bool inverse) -> Value {
            if (args.size() != 1) throw Error(std::string(inverse ? "ifftshift" : "fftshift") + " expects one vector");
            const auto& values = array(args[0]); Array result; const size_t size = values.size();
            const size_t offset = inverse ? (size + 1) / 2 : size / 2;
            for (size_t index = 0; index < size; ++index) result.push_back(std::make_shared<Value>(*values[(index + offset) % size]));
            return Value(result);
        };
        environment_->values["fftshift"] = native([shift_spectrum](const std::vector<Value>& args) { return shift_spectrum(args, false); });
        environment_->values["ifftshift"] = native([shift_spectrum](const std::vector<Value>& args) { return shift_spectrum(args, true); });
        environment_->values["polyfit"] = native([](const std::vector<Value>& args) {
            if (args.size() != 3) throw Error("polyfit expects x values, y values, and a degree");
            const auto& x = array(args[0]); const auto& y = array(args[1]); const int degree = integer(args[2], "polyfit degree must be a non-negative integer");
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
            const auto& coefficients = array(args[0]);
            auto evaluate = [&](double x) { double result = 0; for (const auto& coefficient : coefficients) result = result * x + number(*coefficient); return result; };
            if (auto values = std::get_if<Array>(&args[1])) { Array result; for (const auto& value : *values) result.push_back(std::make_shared<Value>(evaluate(number(*value)))); return Value(result); }
            return Value(evaluate(number(args[1])));
        });
        environment_->values["len"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("len expects one argument");
            if (auto values = std::get_if<Array>(&args[0])) return Value(static_cast<double>(values->size()));
            if (auto text = std::get_if<std::string>(&args[0])) return Value(static_cast<double>(text->size()));
            if (auto object = std::get_if<Object>(&args[0])) return Value(static_cast<double>(object->size()));
            throw Error("len expects an array, string, or object");
        });
        environment_->values["keys"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1 || !std::holds_alternative<Object>(args[0])) throw Error("keys expects one object");
            const auto& object = std::get<Object>(args[0]); Array result;
            for (const auto& entry : object) result.push_back(std::make_shared<Value>(entry.first));
            return Value(result);
        });
        environment_->values["values"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1 || !std::holds_alternative<Object>(args[0])) throw Error("values expects one object");
            const auto& object = std::get<Object>(args[0]); Array result;
            for (const auto& entry : object) result.push_back(std::make_shared<Value>(*entry.second));
            return Value(result);
        });
        environment_->values["has_key"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2 || !std::holds_alternative<Object>(args[0]) || !std::holds_alternative<std::string>(args[1])) throw Error("has_key expects an object and a key");
            const auto& object = std::get<Object>(args[0]); const auto& key = std::get<std::string>(args[1]);
            return Value(object.find(key) != object.end());
        });
        environment_->values["str"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("str expects one argument");
            return Value(display(args[0]));
        });
        environment_->values["type_of"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("type_of expects one value");
            const Value& value = args[0];
            if (std::holds_alternative<double>(value)) return Value(std::string("number"));
            if (std::holds_alternative<std::complex<double>>(value)) return Value(std::string("complex"));
            if (std::holds_alternative<std::string>(value)) return Value(std::string("string"));
            if (std::holds_alternative<bool>(value)) return Value(std::string("bool"));
            if (std::holds_alternative<Object>(value)) return Value(std::string("object"));
            if (std::holds_alternative<FunctionPtr>(value)) return Value(std::string("function"));
            if (std::holds_alternative<std::nullptr_t>(value)) return Value(std::string("none"));
            const auto& values = std::get<Array>(value);
            if (values.empty()) return Value(std::string("array"));
            if (std::all_of(values.begin(), values.end(), [](const auto& item) { return std::holds_alternative<double>(*item); })) return Value(std::string("vector"));
            if (std::all_of(values.begin(), values.end(), [](const auto& item) { return std::holds_alternative<Array>(*item); })) {
                size_t width = 0;
                for (const auto& item : values) {
                    const auto& row = std::get<Array>(*item);
                    if (row.empty() || (width != 0 && row.size() != width) || !std::all_of(row.begin(), row.end(), [](const auto& element) { return std::holds_alternative<double>(*element); })) return Value(std::string("array"));
                    width = row.size();
                }
                return Value(std::string("matrix"));
            }
            return Value(std::string("array"));
        });
        environment_->values["typeof"] = environment_->values["type_of"];
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
        environment_->values["trim"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) throw Error("trim expects one string");
            const auto& text = std::get<std::string>(args[0]); const size_t first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return Value(std::string());
            return Value(text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1));
        });
        environment_->values["replace"] = native([](const std::vector<Value>& args) {
            if (args.size() != 3 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1]) || !std::holds_alternative<std::string>(args[2])) throw Error("replace expects text, search string, and replacement string");
            std::string result = std::get<std::string>(args[0]); const std::string search = std::get<std::string>(args[1]), replacement = std::get<std::string>(args[2]);
            if (search.empty()) throw Error("replace search string cannot be empty");
            size_t position = 0; while ((position = result.find(search, position)) != std::string::npos) { result.replace(position, search.size(), replacement); position += replacement.size(); }
            return Value(result);
        });
        environment_->values["starts_with"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) throw Error("starts_with expects two strings");
            const auto& text = std::get<std::string>(args[0]); const auto& prefix = std::get<std::string>(args[1]); return Value(text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0);
        });
        environment_->values["ends_with"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) throw Error("ends_with expects two strings");
            const auto& text = std::get<std::string>(args[0]); const auto& suffix = std::get<std::string>(args[1]); return Value(text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0);
        });
        environment_->values["split"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) throw Error("split expects a string and a delimiter");
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
            if (args.size() != 2 || !std::holds_alternative<Array>(args[0]) || !std::holds_alternative<std::string>(args[1])) throw Error("join expects an array and a delimiter");
            const auto& values = array(args[0]); const std::string delimiter = std::get<std::string>(args[1]); std::string result;
            for (size_t index = 0; index < values.size(); ++index) { if (index) result += delimiter; result += display(*values[index]); }
            return Value(result);
        });
        environment_->values["range"] = native([](const std::vector<Value>& args) {
            if (args.empty() || args.size() > 3) throw Error("range expects one to three arguments");
            const double start = args.size() == 1 ? 0.0 : number(args, 0);
            const double stop = number(args, args.size() == 1 ? 0 : 1);
            const double step = args.size() == 3 ? number(args, 2) : 1.0;
            Array result; if (!std::isfinite(start) || !std::isfinite(stop) || !std::isfinite(step)) throw Error("range bounds and step must be finite"); if (step == 0) throw Error("range step cannot be zero");
            for (double value = start; step > 0 ? value < stop : value > stop; value += step) {
                if (result.size() > 1000000) throw Error("range exceeded safety limit");
                result.push_back(std::make_shared<Value>(value));
            }
            return Value(result);
        });
        environment_->values["linspace"] = native([](const std::vector<Value>& args) {
            if (args.size() < 2 || args.size() > 3) throw Error("linspace expects two or three arguments");
            int count = args.size() == 3 ? integer(args[2], "linspace count must be a non-negative integer") : 100;
            if (count < 1) throw Error("linspace count must be positive");
            Array result; if (count < 2) { result.push_back(std::make_shared<Value>(number(args, 0))); return Value(result); }
            double step = (number(args, 1) - number(args, 0)) / (count - 1);
            for (int i = 0; i < count; ++i) result.push_back(std::make_shared<Value>(number(args, 0) + step * i));
            return Value(result);
        });
        environment_->values["logspace"] = native([](const std::vector<Value>& args) {
            if (args.size() < 2 || args.size() > 3) throw Error("logspace expects two or three arguments");
            const double start = number(args[0]), stop = number(args[1]);
            if (!std::isfinite(start) || !std::isfinite(stop)) throw Error("logspace exponents must be finite");
            const int count = args.size() == 3 ? integer(args[2], "logspace count must be a non-negative integer") : 50;
            if (count < 1) throw Error("logspace count must be positive");
            Array result; if (count == 1) { result.push_back(std::make_shared<Value>(std::pow(10.0, start))); return Value(result); }
            const double step = (stop - start) / (count - 1);
            for (int index = 0; index < count; ++index) result.push_back(std::make_shared<Value>(std::pow(10.0, start + step * index)));
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
        environment_->values["cumtrapz"] = native([](const std::vector<Value>& args) {
            if (args.size() < 1 || args.size() > 2) throw Error("cumtrapz expects a vector and optional spacing or coordinate vector");
            const auto& values = array(args[0]);
            Array result; result.push_back(std::make_shared<Value>(0.0));
            if (values.size() < 2) return Value(result);
            const Array* coordinates = nullptr; double spacing = 1.0;
            if (args.size() == 2) {
                if (auto vector = std::get_if<Array>(&args[1])) {
                    if (vector->size() != values.size()) throw Error("cumtrapz coordinate vector must match the values length");
                    coordinates = vector;
                } else {
                    spacing = number(args[1]);
                    if (spacing == 0 || !std::isfinite(spacing)) throw Error("cumtrapz spacing must be finite and non-zero");
                }
            }
            double total = 0;
            for (size_t index = 1; index < values.size(); ++index) {
                const double width = coordinates ? number(*(*coordinates)[index]) - number(*(*coordinates)[index - 1]) : spacing;
                total += (number(*values[index - 1]) + number(*values[index])) * width / 2;
                result.push_back(std::make_shared<Value>(total));
            }
            return Value(result);
        });
        environment_->values["interp1"] = native([](const std::vector<Value>& args) {
            if (args.size() < 3 || args.size() > 4) throw Error("interp1 expects a query, two sample vectors, and an optional method");
            const auto& x = array(args[1]); const auto& y = array(args[2]);
            if (x.empty() || x.size() != y.size()) throw Error("interp1 sample vectors must have equal nonzero lengths");
            for (size_t index = 1; index < x.size(); ++index) if (number(*x[index]) <= number(*x[index - 1])) throw Error("interp1 x samples must be strictly increasing");
            std::string method = "linear";
            if (args.size() == 4) { if (!std::holds_alternative<std::string>(args[3])) throw Error("interp1 method must be a string"); method = std::get<std::string>(args[3]); }
            if (method != "linear" && method != "nearest" && method != "previous" && method != "next") throw Error("interp1 method must be linear, nearest, previous, or next");
            auto interpolate = [&](double query) {
                if (x.size() == 1 || query <= number(*x.front())) return number(*y.front());
                if (query >= number(*x.back())) return number(*y.back());
                size_t right = 1; while (right < x.size() && query > number(*x[right])) ++right;
                const double left_x = number(*x[right - 1]), right_x = number(*x[right]);
                const double left_y = number(*y[right - 1]), right_y = number(*y[right]);
                if (method == "previous") return left_y;
                if (method == "next") return right_y;
                const double fraction = (query - left_x) / (right_x - left_x);
                if (method == "nearest") return query - left_x <= right_x - query ? left_y : right_y;
                return left_y + fraction * (right_y - left_y);
            };
            if (auto queries = std::get_if<Array>(&args[0])) { Array result; for (const auto& query : *queries) result.push_back(std::make_shared<Value>(interpolate(number(*query)))); return Value(result); }
            return Value(interpolate(number(args[0])));
        });
        environment_->values["interp2"] = native([](const std::vector<Value>& args) {
            if (args.size() != 5) throw Error("interp2 expects query x, query y, x samples, y samples, and a grid");
            const auto& x = array(args[2]); const auto& y = array(args[3]); const auto& rows = array(args[4]);
            if (x.size() < 2 || y.size() < 2 || rows.size() != y.size()) throw Error("interp2 samples and grid dimensions do not agree");
            for (size_t index = 1; index < x.size(); ++index) if (number(*x[index]) <= number(*x[index - 1])) throw Error("interp2 x samples must be strictly increasing");
            for (size_t index = 1; index < y.size(); ++index) if (number(*y[index]) <= number(*y[index - 1])) throw Error("interp2 y samples must be strictly increasing");
            std::vector<std::vector<double>> grid; grid.reserve(rows.size());
            for (const auto& row : rows) { const auto& values = array(*row); if (values.size() != x.size()) throw Error("interp2 samples and grid dimensions do not agree"); std::vector<double> converted; for (const auto& value : values) converted.push_back(number(*value)); grid.push_back(std::move(converted)); }
            auto bracket = [](double query, const auto& samples) {
                if (query <= samples.front()) return size_t(1);
                if (query >= samples.back()) return samples.size() - 1;
                return static_cast<size_t>(std::upper_bound(samples.begin(), samples.end(), query) - samples.begin());
            };
            std::vector<double> x_values, y_values; for (const auto& value : x) x_values.push_back(number(*value)); for (const auto& value : y) y_values.push_back(number(*value));
            auto interpolate = [&](double query_x, double query_y) {
                const size_t right_x = bracket(query_x, x_values), upper_y = bracket(query_y, y_values); const size_t left_x = right_x - 1, lower_y = upper_y - 1;
                const double tx = (std::clamp(query_x, x_values[left_x], x_values[right_x]) - x_values[left_x]) / (x_values[right_x] - x_values[left_x]);
                const double ty = (std::clamp(query_y, y_values[lower_y], y_values[upper_y]) - y_values[lower_y]) / (y_values[upper_y] - y_values[lower_y]);
                const double lower = grid[lower_y][left_x] + tx * (grid[lower_y][right_x] - grid[lower_y][left_x]);
                const double upper = grid[upper_y][left_x] + tx * (grid[upper_y][right_x] - grid[upper_y][left_x]);
                return lower + ty * (upper - lower);
            };
            const auto query_x_values = std::get_if<Array>(&args[0]), query_y_values = std::get_if<Array>(&args[1]);
            if (query_x_values || query_y_values) {
                const size_t count = std::max(query_x_values ? query_x_values->size() : 1UL, query_y_values ? query_y_values->size() : 1UL);
                if (query_x_values && query_y_values && query_x_values->size() != query_y_values->size() && query_x_values->size() != 1 && query_y_values->size() != 1) throw Error("interp2 query vectors must have equal lengths or be scalar-broadcastable");
                Array result;
                for (size_t index = 0; index < count; ++index) {
                    const double query_x = query_x_values ? number(*query_x_values->at(query_x_values->size() == 1 ? 0 : index)) : number(args[0]);
                    const double query_y = query_y_values ? number(*query_y_values->at(query_y_values->size() == 1 ? 0 : index)) : number(args[1]);
                    result.push_back(std::make_shared<Value>(interpolate(query_x, query_y)));
                }
                return Value(result);
            }
            return Value(interpolate(number(args[0]), number(args[1])));
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
        environment_->values["quantile"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("quantile expects an array and a probability");
            std::vector<double> sorted; for (const auto& value : array(args[0])) sorted.push_back(number(*value));
            const double probability = number(args[1]); if (sorted.empty()) throw Error("quantile of an empty array"); if (probability < 0 || probability > 1) throw Error("quantile probability must be between zero and one");
            std::sort(sorted.begin(), sorted.end()); const double position = probability * (sorted.size() - 1); const size_t lower = static_cast<size_t>(std::floor(position)); const size_t upper = static_cast<size_t>(std::ceil(position));
            return Value(sorted[lower] + (sorted[upper] - sorted[lower]) * (position - lower));
        });
        environment_->values["cov"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("cov expects two arrays");
            const auto& left = array(args[0]); const auto& right = array(args[1]); if (left.empty() || left.size() != right.size()) throw Error("covariance arrays must have equal nonzero lengths");
            double left_mean = 0, right_mean = 0; for (size_t index = 0; index < left.size(); ++index) { left_mean += number(*left[index]); right_mean += number(*right[index]); } left_mean /= left.size(); right_mean /= right.size();
            double result = 0; for (size_t index = 0; index < left.size(); ++index) result += (number(*left[index]) - left_mean) * (number(*right[index]) - right_mean);
            return Value(result / left.size());
        });
        environment_->values["corr"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("corr expects two arrays");
            const auto& left = array(args[0]); const auto& right = array(args[1]); if (left.empty() || left.size() != right.size()) throw Error("correlation arrays must have equal nonzero lengths");
            double left_mean = 0, right_mean = 0; for (size_t index = 0; index < left.size(); ++index) { left_mean += number(*left[index]); right_mean += number(*right[index]); } left_mean /= left.size(); right_mean /= right.size();
            double numerator = 0, left_sum = 0, right_sum = 0; for (size_t index = 0; index < left.size(); ++index) { const double a = number(*left[index]) - left_mean, b = number(*right[index]) - right_mean; numerator += a * b; left_sum += a * a; right_sum += b * b; }
            if (left_sum == 0 || right_sum == 0) throw Error("correlation is undefined for a constant array");
            return Value(numerator / std::sqrt(left_sum * right_sum));
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
        environment_->values["cumprod"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("cumprod expects one argument");
            std::vector<double> result; double product = 1; for (const auto& value : array(args[0])) { product *= number(*value); result.push_back(product); }
            return vector_value(result);
        });
        environment_->values["diff"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("diff expects one argument");
            const auto& values = array(args[0]); std::vector<double> result; if (values.size() < 2) return vector_value(result);
            result.reserve(values.size() - 1); for (size_t index = 1; index < values.size(); ++index) result.push_back(number(*values[index]) - number(*values[index - 1]));
            return vector_value(result);
        });
        environment_->values["convolve"] = native([](const std::vector<Value>& args) {
            if (args.size() < 2 || args.size() > 3) throw Error("convolve expects two vectors and an optional mode");
            const auto& signal = array(args[0]); const auto& kernel = array(args[1]);
            if (signal.empty() || kernel.empty()) throw Error("convolve vectors cannot be empty");
            std::string mode = "full";
            if (args.size() == 3) {
                if (!std::holds_alternative<std::string>(args[2])) throw Error("convolve mode must be a string");
                mode = std::get<std::string>(args[2]);
            }
            if (mode != "full" && mode != "same" && mode != "valid") throw Error("convolve mode must be full, same, or valid");
            if (mode == "valid" && signal.size() < kernel.size()) throw Error("valid convolution requires the first vector to be at least as long as the kernel");
            const size_t full_size = signal.size() + kernel.size() - 1;
            std::vector<double> full(full_size, 0);
            for (size_t index = 0; index < signal.size(); ++index)
                for (size_t offset = 0; offset < kernel.size(); ++offset)
                    full[index + offset] += number(*signal[index]) * number(*kernel[offset]);
            if (mode == "full") return vector_value(full);
            if (mode == "valid") return vector_value(std::vector<double>(full.begin() + kernel.size() - 1, full.begin() + signal.size()));
            const size_t start = (kernel.size() - 1) / 2;
            return vector_value(std::vector<double>(full.begin() + start, full.begin() + start + signal.size()));
        });
        environment_->values["histogram"] = native([](const std::vector<Value>& args) {
            if (args.size() < 1 || args.size() > 2) throw Error("histogram expects values and an optional bin count");
            const auto& values = array(args[0]);
            const int bins = args.size() == 2 ? integer(args[1], "histogram bin count must be a positive integer") : 10;
            if (bins < 1) throw Error("histogram bin count must be positive");
            if (values.empty()) return Value(Object{{"counts", std::make_shared<Value>(Array{})}, {"edges", std::make_shared<Value>(Array{})}});
            double minimum = number(*values.front()), maximum = minimum;
            for (const auto& value : values) { const double current = number(*value); minimum = std::min(minimum, current); maximum = std::max(maximum, current); }
            if (minimum == maximum) maximum = minimum + 1.0;
            const double width = (maximum - minimum) / bins;
            std::vector<double> counts(static_cast<size_t>(bins), 0);
            for (const auto& value : values) {
                const double current = number(*value);
                const int bin = std::min(bins - 1, std::max(0, static_cast<int>((current - minimum) / width)));
                counts[static_cast<size_t>(bin)] += 1;
            }
            Array edges; for (int index = 0; index <= bins; ++index) edges.push_back(std::make_shared<Value>(minimum + width * index));
            Object result; result["counts"] = std::make_shared<Value>(vector_value(counts)); result["edges"] = std::make_shared<Value>(edges); return Value(result);
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
            if (args.size() < 1 || args.size() > 2) throw Error("norm expects a vector or matrix and optional order");
            const auto& values = array(args[0]); const bool matrix_input = !values.empty() && std::holds_alternative<Array>(*values.front());
            const double order = args.size() == 2 ? number(args[1]) : 2.0;
            if (order <= 0) throw Error("norm order must be positive");
            if (!matrix_input) { if (order != 2.0) { double result = 0; for (const auto& value : values) result += std::pow(std::abs(number(*value)), order); return Value(std::pow(result, 1.0 / order)); } double result = 0; for (const auto& value : values) result += number(*value) * number(*value); return Value(std::sqrt(result)); }
            const auto matrix = rectangular_matrix(args[0]);
            if (order == 1.0) { double result = 0; for (size_t column = 0; column < matrix.front().size(); ++column) { double total = 0; for (const auto& row : matrix) total += std::abs(row[column]); result = std::max(result, total); } return Value(result); }
            if (order != 2.0) throw Error("matrix norm supports orders 1 and 2");
            const auto decomposition = symmetric_eigen(dense_multiply(dense_transpose(matrix), matrix));
            return Value(std::sqrt(std::max(0.0, *std::max_element(decomposition.first.begin(), decomposition.first.end()))));
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
            if (!values.empty()) {
                const bool matrix = std::holds_alternative<Array>(*values.front());
                size_t width = 0;
                for (const auto& item : values) {
                    if (std::holds_alternative<Array>(*item) != matrix) throw Error("shape expects a rectangular array");
                    if (matrix) {
                        const size_t item_width = array(*item).size();
                        if (&item == &values.front()) width = item_width;
                        else if (item_width != width) throw Error("shape expects a rectangular matrix");
                    }
                }
                if (matrix) result.push_back(std::make_shared<Value>(static_cast<double>(width)));
            }
            return Value(result);
        });
        environment_->values["reshape"] = native([](const std::vector<Value>& args) {
            if (args.size() != 3) throw Error("reshape expects an array, rows, and columns");
            const auto& values = array(args[0]); const int rows = integer(args[1], "reshape rows must be a non-negative integer"); const int columns = integer(args[2], "reshape columns must be a non-negative integer");
            if (static_cast<size_t>(rows) * static_cast<size_t>(columns) != values.size()) throw Error("reshape dimensions do not match array length");
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
        environment_->values["vstack"] = native([](const std::vector<Value>& args) {
            if (args.empty()) throw Error("vstack expects one or more arrays");
            std::vector<std::vector<double>> result;
            size_t width = 0;
            for (const auto& value : args) {
                const auto& outer = array(value);
                if (outer.empty()) continue;
                if (std::holds_alternative<Array>(*outer.front())) {
                    const auto matrix = rectangular_matrix(value);
                    if (width == 0) width = matrix.front().size();
                    if (matrix.front().size() != width) throw Error("vstack arrays must have equal column counts");
                    result.insert(result.end(), matrix.begin(), matrix.end());
                } else {
                    std::vector<double> row; for (const auto& item : outer) row.push_back(number(*item));
                    if (width == 0) width = row.size();
                    if (row.size() != width) throw Error("vstack arrays must have equal column counts");
                    result.push_back(std::move(row));
                }
            }
            return matrix_value(result);
        });
        environment_->values["hstack"] = native([](const std::vector<Value>& args) {
            if (args.empty()) throw Error("hstack expects one or more arrays");
            std::vector<std::vector<double>> result;
            for (const auto& value : args) {
                const auto& outer = array(value);
                std::vector<std::vector<double>> part;
                if (!outer.empty() && std::holds_alternative<Array>(*outer.front())) part = rectangular_matrix(value);
                else { std::vector<double> row; for (const auto& item : outer) row.push_back(number(*item)); part.push_back(std::move(row)); }
                if (result.empty()) result = std::move(part);
                else {
                    if (result.size() != part.size()) throw Error("hstack arrays must have equal row counts");
                    for (size_t row = 0; row < result.size(); ++row) result[row].insert(result[row].end(), part[row].begin(), part[row].end());
                }
            }
            return matrix_value(result);
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
        environment_->values["expm"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("expm expects one square matrix");
            const auto input = numeric_matrix(args[0]); const size_t size = input.size();
            double norm = 0; for (const auto& row : input) { double sum = 0; for (double value : row) sum += std::abs(value); norm = std::max(norm, sum); }
            const int scale = norm > 1 ? static_cast<int>(std::ceil(std::log2(norm))) : 0; const double divisor = std::ldexp(1.0, scale);
            auto scaled = input; for (auto& row : scaled) for (double& value : row) value /= divisor;
            std::vector<std::vector<double>> result(size, std::vector<double>(size, 0)), term(size, std::vector<double>(size, 0));
            for (size_t index = 0; index < size; ++index) result[index][index] = term[index][index] = 1;
            for (int order = 1; order <= 50; ++order) {
                term = dense_multiply(term, scaled); for (auto& row : term) for (double& value : row) value /= order;
                double term_norm = 0; for (const auto& row : term) { double sum = 0; for (double value : row) sum += std::abs(value); term_norm = std::max(term_norm, sum); }
                for (size_t row = 0; row < size; ++row) for (size_t column = 0; column < size; ++column) result[row][column] += term[row][column];
                if (term_norm < 1e-15) break;
                if (order == 50) throw Error("expm series did not converge");
            }
            for (int step = 0; step < scale; ++step) result = dense_multiply(result, result);
            return matrix_value(result);
        });
        environment_->values["solve"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("solve expects a matrix and a vector");
            const auto& right = array(args[1]);
            if (!right.empty() && std::holds_alternative<Array>(*right.front())) {
                const auto matrix = rectangular_matrix(args[1]); const auto coefficient = numeric_matrix(args[0]);
                if (coefficient.size() != matrix.size()) throw Error("solve right-hand side rows do not match matrix");
                const size_t columns = matrix.front().size(); std::vector<std::vector<double>> result(coefficient.size(), std::vector<double>(columns));
                for (size_t column = 0; column < columns; ++column) { std::vector<double> target(matrix.size()); for (size_t row = 0; row < matrix.size(); ++row) target[row] = matrix[row][column]; const auto solution = solve_linear(args[0], vector_value(target)); for (size_t row = 0; row < solution.size(); ++row) result[row][column] = solution[row]; }
                return matrix_value(result);
            }
            return vector_value(solve_linear(args[0], args[1]));
        });
        environment_->values["rank"] = native([](const std::vector<Value>& args) {
            if (args.size() < 1 || args.size() > 2) throw Error("rank expects a matrix and optional tolerance");
            const auto matrix = rectangular_matrix(args[0]);
            const double tolerance = args.size() == 2 ? number(args[1]) : 1e-10;
            if (tolerance <= 0) throw Error("rank tolerance must be positive");
            return Value(static_cast<double>(matrix_rank(matrix, tolerance)));
        });
        environment_->values["pinv"] = native([](const std::vector<Value>& args) {
            if (args.size() < 1 || args.size() > 2) throw Error("pinv expects a matrix and optional tolerance");
            const double tolerance = args.size() == 2 ? number(args[1]) : 1e-10;
            if (tolerance <= 0) throw Error("pinv tolerance must be positive");
            return matrix_value(pseudoinverse(rectangular_matrix(args[0]), tolerance));
        });
        environment_->values["lstsq"] = native([](const std::vector<Value>& args) {
            if (args.size() < 2 || args.size() > 3) throw Error("lstsq expects a matrix, vector, and optional tolerance");
            const auto matrix = rectangular_matrix(args[0]);
            const auto& target = array(args[1]);
            if (matrix.size() != target.size()) throw Error("lstsq target length does not match matrix rows");
            const double tolerance = args.size() == 3 ? number(args[2]) : 1e-10;
            if (tolerance <= 0) throw Error("lstsq tolerance must be positive");
            const auto inverse = pseudoinverse(matrix, tolerance);
            std::vector<double> result(inverse.size(), 0);
            for (size_t row = 0; row < inverse.size(); ++row) for (size_t column = 0; column < inverse[row].size(); ++column) result[row] += inverse[row][column] * number(*target[column]);
            return vector_value(result);
        });
        environment_->values["kron"] = native([](const std::vector<Value>& args) {
            if (args.size() != 2) throw Error("kron expects two matrices");
            const auto left = rectangular_matrix(args[0]), right = rectangular_matrix(args[1]);
            std::vector<std::vector<double>> result(left.size() * right.size(), std::vector<double>(left.front().size() * right.front().size()));
            for (size_t row = 0; row < left.size(); ++row) for (size_t column = 0; column < left[row].size(); ++column)
                for (size_t right_row = 0; right_row < right.size(); ++right_row) for (size_t right_column = 0; right_column < right[right_row].size(); ++right_column)
                    result[row * right.size() + right_row][column * right.front().size() + right_column] = left[row][column] * right[right_row][right_column];
            return matrix_value(result);
        });
        environment_->values["eigvals"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("eigvals expects one square matrix");
            const auto matrix = numeric_matrix(args[0]);
            if (matrix.size() == 2) {
                const std::complex<double> trace = matrix[0][0] + matrix[1][1];
                const std::complex<double> discriminant = std::sqrt(std::complex<double>((matrix[0][0] - matrix[1][1]) * (matrix[0][0] - matrix[1][1]) + 4 * matrix[0][1] * matrix[1][0], 0));
                return complex_vector_value({(trace + discriminant) / 2.0, (trace - discriminant) / 2.0});
            }
            const auto decomposition = symmetric_eigen(matrix);
            return vector_value(decomposition.first);
        });
        environment_->values["eig"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("eig expects one square matrix");
            const auto decomposition = symmetric_eigen(numeric_matrix(args[0]));
            Object result;
            result["values"] = std::make_shared<Value>(vector_value(decomposition.first));
            result["vectors"] = std::make_shared<Value>(matrix_value(decomposition.second));
            return Value(result);
        });
        environment_->values["svd"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("svd expects one matrix");
            const auto matrix = rectangular_matrix(args[0]); const size_t rows = matrix.size(), columns = matrix.front().size();
            const auto transpose = dense_transpose(matrix); const auto decomposition = symmetric_eigen(dense_multiply(transpose, matrix));
            std::vector<size_t> order(columns); for (size_t index = 0; index < columns; ++index) order[index] = index;
            std::sort(order.begin(), order.end(), [&](size_t left, size_t right) { return decomposition.first[left] > decomposition.first[right]; });
            std::vector<std::vector<double>> right_vectors(columns, std::vector<double>(columns, 0)); std::vector<double> singular_values(columns, 0);
            for (size_t column = 0; column < columns; ++column) { const size_t source = order[column]; singular_values[column] = std::sqrt(std::max(0.0, decomposition.first[source])); for (size_t row = 0; row < columns; ++row) right_vectors[row][column] = decomposition.second[row][source]; }
            std::vector<std::vector<double>> left_vectors(rows, std::vector<double>(columns, 0));
            for (size_t column = 0; column < columns; ++column) if (singular_values[column] > 1e-12) for (size_t row = 0; row < rows; ++row) for (size_t item = 0; item < columns; ++item) left_vectors[row][column] += matrix[row][item] * right_vectors[item][column] / singular_values[column];
            Object result; result["U"] = std::make_shared<Value>(matrix_value(left_vectors)); result["S"] = std::make_shared<Value>(vector_value(singular_values)); result["V"] = std::make_shared<Value>(matrix_value(right_vectors)); return Value(result);
        });
        environment_->values["qr"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("qr expects one matrix");
            const auto decomposition = qr_decompose(rectangular_matrix(args[0]));
            Object result;
            result["Q"] = std::make_shared<Value>(matrix_value(decomposition.first));
            result["R"] = std::make_shared<Value>(matrix_value(decomposition.second));
            return Value(result);
        });
        environment_->values["cholesky"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("cholesky expects one matrix");
            return matrix_value(cholesky_factor(numeric_matrix(args[0])));
        });
        environment_->values["eye"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("eye expects a matrix size");
            const int size = integer(args[0], "eye size must be a non-negative integer");
            Array result; for (int row = 0; row < size; ++row) { Array values; for (int column = 0; column < size; ++column) values.push_back(std::make_shared<Value>(row == column ? 1.0 : 0.0)); result.push_back(std::make_shared<Value>(values)); } return Value(result);
        });
        environment_->values["diag"] = native([](const std::vector<Value>& args) {
            if (args.size() < 1 || args.size() > 2) throw Error("diag expects a vector or matrix and optional offset");
            const int offset = args.size() == 2 ? signed_integer(args[1], "diag offset must be an integer") : 0;
            if (auto values = std::get_if<Array>(&args[0])) {
                if (!values->empty() && std::holds_alternative<Array>(*values->front())) {
                    const auto matrix = rectangular_matrix(args[0]); const int rows = static_cast<int>(matrix.size()), columns = static_cast<int>(matrix.front().size());
                    const int row_start = std::max(0, -offset), column_start = std::max(0, offset); Array diagonal;
                    for (int row = row_start, column = column_start; row < rows && column < columns; ++row, ++column) diagonal.push_back(std::make_shared<Value>(matrix[row][column]));
                    return Value(diagonal);
                }
                const int size = static_cast<int>(values->size()) + std::abs(offset); Array result;
                for (int row = 0; row < size; ++row) { Array line; for (int column = 0; column < size; ++column) { const int source = row - std::max(0, -offset), target = row + offset; line.push_back(std::make_shared<Value>(source >= 0 && source < static_cast<int>(values->size()) && column == target ? number(*(*values)[source]) : 0.0)); } result.push_back(std::make_shared<Value>(line)); }
                return Value(result);
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
            Array rows; std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF && static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF) content.erase(0, 3);
            std::vector<std::string> fields; std::string field; bool quoted = false;
            auto append_row = [&]() {
                if (fields.size() == 1 && fields.front().empty()) { fields.clear(); return; }
                Array row;
                for (const auto& value : fields) {
                    try {
                        size_t parsed = 0; const double number_value = std::stod(value, &parsed);
                        if (parsed == value.size()) row.push_back(std::make_shared<Value>(number_value));
                        else row.push_back(std::make_shared<Value>(value));
                    } catch (...) { row.push_back(std::make_shared<Value>(value)); }
                }
                rows.push_back(std::make_shared<Value>(row)); fields.clear();
            };
            for (size_t index = 0; index <= content.size(); ++index) {
                const char character = index < content.size() ? content[index] : '\n';
                if (character == '"') {
                    if (quoted && index + 1 < content.size() && content[index + 1] == '"') { field += '"'; ++index; }
                    else quoted = !quoted;
                } else if (character == separator && !quoted) { fields.push_back(field); field.clear(); }
                else if ((character == '\n' || character == '\r') && !quoted) {
                    if (character == '\r' && index + 1 < content.size() && content[index + 1] == '\n') ++index;
                    fields.push_back(field); field.clear(); append_row();
                } else field += character;
            }
            if (quoted) throw Error(format + " contains an unterminated quoted field");
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
            if (args.size() != 2 || !std::holds_alternative<std::string>(args[0])) throw Error("write_json expects a file path and a value");
            auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]);
            std::ofstream output(path); if (!output) throw Error("unable to write JSON '" + path.string() + "'");
            output << json(args[1]); return Value(nullptr);
        });
        environment_->values["read_json"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) throw Error("read_json expects a file path");
            auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]);
            std::ifstream input(path); if (!input) throw Error("unable to open JSON '" + path.string() + "'");
            std::stringstream contents; contents << input.rdbuf(); std::string text = contents.str();
            if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) text.erase(0, 3);
            size_t position = 0;
            Value result = parse_json(text, position); json_space(text, position);
            if (position != text.size()) throw Error("JSON contains trailing content");
            return result;
        });
        environment_->values["write_jsonl"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 2 || !std::holds_alternative<std::string>(args[0])) throw Error("write_jsonl expects a file path and an array");
            const auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]);
            std::ofstream output(path); if (!output) throw Error("unable to write JSON Lines '" + path.string() + "'");
            for (const auto& item : array(args[1])) output << json(*item) << '\n';
            return Value(nullptr);
        });
        environment_->values["read_jsonl"] = native([this](const std::vector<Value>& args) {
            if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) throw Error("read_jsonl expects a file path");
            const auto path = std::filesystem::path(base_dir_) / std::get<std::string>(args[0]);
            std::ifstream input(path); if (!input) throw Error("unable to open JSON Lines '" + path.string() + "'");
            Array result; std::string line; size_t line_number = 0;
            while (std::getline(input, line)) {
                ++line_number; if (line_number == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) line.erase(0, 3);
                if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
                size_t position = 0; Value item = parse_json(line, position); json_space(line, position);
                if (position != line.size()) throw Error("JSON Lines contains trailing content on line " + std::to_string(line_number));
                result.push_back(std::make_shared<Value>(std::move(item)));
            }
            return Value(result);
        });
    }
    static FunctionPtr native(std::function<Value(const std::vector<Value>&)> function) {
        auto result = std::make_shared<Function>(); result->native = std::move(function); return result;
    }
    void import_module(const std::string& module, const std::string& alias, const std::shared_ptr<Environment>& environment) {
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
        if (!alias.empty()) {
            Object namespace_value;
            for (const auto& entry : module_runtime.environment_->values)
                if (!entry.first.empty() && entry.first[0] != '_') namespace_value[entry.first] = std::make_shared<Value>(entry.second);
            environment->values[alias] = Value(namespace_value);
        } else {
            for (const auto& entry : module_runtime.environment_->values)
                if (!entry.first.empty() && entry.first[0] != '_') environment->values[entry.first] = entry.second;
            for (const auto& entry : module_runtime.environment_->types)
                if (!entry.first.empty() && entry.first[0] != '_') environment->types[entry.first] = entry.second;
        }
    }
    void execute(const std::vector<Statement>& statements, const std::shared_ptr<Environment>& environment) {
        for (const auto& statement : statements) {
            try {
                execute_statement(statement, environment);
            } catch (const Error& error) {
                const std::string message = error.what();
                if (message.rfind("line ", 0) == 0 || statement.line == 0) throw;
                throw Error("line " + std::to_string(statement.line) + ", column " + std::to_string(statement.column) + ": " + message);
            }
        }
    }
    void execute_statement(const Statement& statement, const std::shared_ptr<Environment>& environment) {
        switch (statement.kind) {
        case Statement::Kind::Let: {
            Value value = evaluate(statement.expression, environment);
            if (!statement.type.empty() && !matches_type(value, statement.type)) throw Error("value for '" + statement.name + "' is not of type " + statement.type);
            environment->values[statement.name] = value;
            if (statement.type.empty()) environment->types.erase(statement.name); else environment->types[statement.name] = statement.type;
            break;
        }
        case Statement::Kind::Assign: assign(environment, statement.name, evaluate(statement.expression, environment)); break;
        case Statement::Kind::AssignIndex: {
            std::vector<ExprPtr> ignored_indices; std::string base_name;
            collect_indices(statement.target, ignored_indices, base_name);
            Value* base = base_name.empty() ? nullptr : reference(environment, base_name);
            Value previous = base ? *base : Value(nullptr);
            assign_index(statement.target, evaluate(statement.expression, environment), environment);
            if (base) {
                for (auto current = environment; current; current = current->parent) {
                    const auto type = current->types.find(base_name);
                    if (type == current->types.end()) continue;
                    if (!matches_type(*base, type->second)) { *base = std::move(previous); throw Error("indexed assignment would violate type " + type->second + " for '" + base_name + "'"); }
                    break;
                }
            }
            break;
        }
        case Statement::Kind::Print: std::cout << display(evaluate(statement.expression, environment)) << '\n'; break;
        case Statement::Kind::Expression: evaluate(statement.expression, environment); break;
        case Statement::Kind::Import: import_module(statement.module, statement.alias, environment); break;
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
        case Statement::Kind::Try: {
            std::exception_ptr pending;
            try {
                execute(statement.body, child(environment));
            } catch (...) {
                pending = std::current_exception();
                try {
                    std::rethrow_exception(pending);
                } catch (const ThrowSignal& signal) {
                    if (!statement.alternate.empty()) {
                        try {
                            auto catch_environment = child(environment);
                            if (!statement.name.empty()) catch_environment->values[statement.name] = signal.value;
                            execute(statement.alternate, catch_environment);
                            pending = nullptr;
                        } catch (...) { pending = std::current_exception(); }
                    }
                } catch (const Error& error) {
                    if (!statement.alternate.empty()) {
                        try {
                            auto catch_environment = child(environment);
                            if (!statement.name.empty()) catch_environment->values[statement.name] = std::string(error.what());
                            execute(statement.alternate, catch_environment);
                            pending = nullptr;
                        } catch (...) { pending = std::current_exception(); }
                    }
                }
            }
            try {
                if (!statement.finalizer.empty()) execute(statement.finalizer, child(environment));
            } catch (...) {
                throw;
            }
            if (pending) std::rethrow_exception(pending);
            break;
        }
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
            if (auto object = std::get_if<Object>(&iterable)) {
                if (statement.second_name.empty()) throw Error("object iteration expects key and value variables");
                for (const auto& entry : *object) {
                    environment->values[statement.name] = entry.first;
                    environment->values[statement.second_name] = *entry.second;
                    try { execute(statement.body, environment); }
                    catch (const ContinueSignal&) { continue; }
                    catch (const BreakSignal&) { break; }
                }
            } else if (auto text = std::get_if<std::string>(&iterable)) {
                size_t byte_index = 0, character_index = 0;
                while (byte_index < text->size()) {
                    const unsigned char lead = static_cast<unsigned char>((*text)[byte_index]);
                    size_t width = lead < 0x80 ? 1 : (lead >= 0xF0 ? 4 : (lead >= 0xE0 ? 3 : 2));
                    if (byte_index + width > text->size()) width = 1;
                    for (size_t offset = 1; offset < width; ++offset)
                        if ((static_cast<unsigned char>((*text)[byte_index + offset]) & 0xC0) != 0x80) { width = 1; break; }
                    const std::string character = text->substr(byte_index, width);
                    if (statement.second_name.empty()) environment->values[statement.name] = character;
                    else {
                        environment->values[statement.name] = static_cast<double>(character_index);
                        environment->values[statement.second_name] = character;
                    }
                    byte_index += width;
                    ++character_index;
                    try { execute(statement.body, environment); }
                    catch (const ContinueSignal&) { continue; }
                    catch (const BreakSignal&) { break; }
                }
            } else {
                const auto& values = array(iterable);
                for (size_t index = 0; index < values.size(); ++index) {
                    if (statement.second_name.empty()) environment->values[statement.name] = *values[index];
                    else {
                        environment->values[statement.name] = static_cast<double>(index);
                        environment->values[statement.second_name] = *values[index];
                    }
                    try { execute(statement.body, environment); }
                    catch (const ContinueSignal&) { continue; }
                    catch (const BreakSignal&) { break; }
                }
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
        if (node.op == "-") { if (auto complex = std::get_if<std::complex<double>>(&value)) return -*complex; return -number(value); }
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
            if (i < (*function)->parameter_types.size() && !(*function)->parameter_types[i].empty())
                call_environment->types[(*function)->parameters[i]] = (*function)->parameter_types[i];
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
                for (const auto& item : indices_value) selected.push_back(std::make_shared<Value>(index_value(result, index_number(*item))));
                return selected;
            }
        }
        Value evaluated_index = evaluate(node.index, environment);
        if (auto selections = std::get_if<Array>(&evaluated_index); selections && selections->size() == 2 && (std::holds_alternative<Array>(*(selections->at(0))) || std::holds_alternative<Array>(*(selections->at(1))))) {
            auto selected_indices = [](const Value& selection) {
                std::vector<size_t> values;
                if (auto array_selection = std::get_if<Array>(&selection)) for (const auto& item : *array_selection) values.push_back(index_number(*item));
                else values.push_back(index_number(selection));
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
            else result = index_value(result, index_number(index));
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
    Value eval(const Expr::Conditional& node, const std::shared_ptr<Environment>& environment) {
        return truthy(evaluate(node.condition, environment)) ? evaluate(node.when_true, environment) : evaluate(node.when_false, environment);
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
            if (current->values.count(name)) {
                const auto type = current->types.find(name);
                if (type != current->types.end() && !matches_type(value, type->second)) throw Error("value for '" + name + "' is not of type " + type->second);
                current->values[name] = value; return;
            }
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
                if (auto values = std::get_if<Array>(&value)) { sliced = true; for (const auto& item : *values) result.push_back(index_number(*item)); }
                else { sliced = false; result.push_back(index_number(value)); }
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
                    const size_t target_index = index_number(*range->at(index)); if (target_index >= values->size()) throw Error("slice assignment index out of range");
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
            else path.emplace_back(index_number(index));
        }
        set_path(*base, path, 0, replacement);
    }
    static double number(const std::vector<Value>& values, size_t index) {
        if (index >= values.size()) throw Error("missing numeric argument");
        return number(values[index]);
    }
    static int integer(const Value& value, const std::string& description) {
        const double candidate = number(value);
        if (!std::isfinite(candidate) || candidate < 0 || candidate != std::floor(candidate) || candidate > static_cast<double>(std::numeric_limits<int>::max())) throw Error(description);
        return static_cast<int>(candidate);
    }
    static int signed_integer(const Value& value, const std::string& description) {
        const double candidate = number(value);
        if (!std::isfinite(candidate) || candidate != std::floor(candidate) || candidate < static_cast<double>(std::numeric_limits<int>::min()) || candidate > static_cast<double>(std::numeric_limits<int>::max())) throw Error(description);
        return static_cast<int>(candidate);
    }
    static size_t index_number(const Value& value) {
        const double candidate = number(value);
        if (!std::isfinite(candidate) || candidate < 0 || candidate != std::floor(candidate) || static_cast<long double>(candidate) > static_cast<long double>(std::numeric_limits<size_t>::max()))
            throw Error("array index must be a non-negative integer");
        return static_cast<size_t>(candidate);
    }
    static Value map_values(const Value& value, const std::function<Value(const Value&)>& function) {
        if (auto values = std::get_if<Array>(&value)) {
            Array result; result.reserve(values->size());
            for (const auto& item : *values) result.push_back(std::make_shared<Value>(map_values(*item, function)));
            return result;
        }
        return function(value);
    }
    static Value map_binary(const Value& left, const Value& right, const std::function<Value(const Value&, const Value&)>& function) {
        const auto left_values = std::get_if<Array>(&left), right_values = std::get_if<Array>(&right);
        if (left_values && right_values) {
            if (left_values->size() != right_values->size()) throw Error("array operands must have equal lengths");
            Array result; result.reserve(left_values->size());
            for (size_t index = 0; index < left_values->size(); ++index) result.push_back(std::make_shared<Value>(map_binary(*left_values->at(index), *right_values->at(index), function)));
            return result;
        }
        if (left_values) { Array result; result.reserve(left_values->size()); for (const auto& item : *left_values) result.push_back(std::make_shared<Value>(map_binary(*item, right, function))); return result; }
        if (right_values) { Array result; result.reserve(right_values->size()); for (const auto& item : *right_values) result.push_back(std::make_shared<Value>(map_binary(left, *item, function))); return result; }
        return function(left, right);
    }
    static double number(const Value& value) { if (auto number = std::get_if<double>(&value)) return *number; throw Error("expected a number"); }
    static std::complex<double> as_complex(const Value& value) {
        if (auto complex = std::get_if<std::complex<double>>(&value)) return *complex;
        if (auto number_value = std::get_if<double>(&value)) return {*number_value, 0};
        throw Error("expected a number or complex value");
    }
    static bool matches_type(const Value& value, const std::string& type) {
        if (type == "any") return true;
        if (type == "number") return std::holds_alternative<double>(value);
        if (type == "complex") return std::holds_alternative<std::complex<double>>(value);
        if (type == "string") return std::holds_alternative<std::string>(value);
        if (type == "bool" || type == "boolean") return std::holds_alternative<bool>(value);
        if (type == "array" || type == "list") return std::holds_alternative<Array>(value);
        if (type == "vector") {
            const auto values = std::get_if<Array>(&value); if (!values) return false;
            return std::all_of(values->begin(), values->end(), [](const auto& item) { return std::holds_alternative<double>(*item); });
        }
        if (type == "matrix") {
            const auto values = std::get_if<Array>(&value); if (!values || values->empty()) return false;
            size_t width = 0;
            for (const auto& item : *values) {
                const auto row = std::get_if<Array>(&*item); if (!row || row->empty()) return false;
                if (width == 0) width = row->size(); else if (row->size() != width) return false;
                for (const auto& element : *row) if (!std::holds_alternative<double>(*element)) return false;
            }
            return true;
        }
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
        int rows = integer(args[0], "matrix rows must be a non-negative integer"), cols = args.size() == 2 ? integer(args[1], "matrix columns must be a non-negative integer") : rows;
        Array result; for (int row = 0; row < rows; ++row) {
            Array line; for (int column = 0; column < cols; ++column) line.push_back(std::make_shared<Value>(fill));
            result.push_back(std::make_shared<Value>(line));
        }
        return result;
    }
    static std::vector<std::vector<double>> rectangular_matrix(const Value& value) {
        const auto& rows = array(value); if (rows.empty()) throw Error("matrix cannot be empty");
        std::vector<std::vector<double>> result; size_t width = 0;
        for (const auto& row_value : rows) {
            if (!std::holds_alternative<Array>(*row_value)) throw Error("expected a matrix");
            const auto& row = array(*row_value); if (!width) width = row.size();
            if (row.empty() || row.size() != width) throw Error("matrix must be rectangular and non-empty");
            std::vector<double> converted; for (const auto& item : row) converted.push_back(number(*item));
            result.push_back(std::move(converted));
        }
        return result;
    }
    static size_t matrix_rank(std::vector<std::vector<double>> matrix, double tolerance) {
        const size_t rows = matrix.size(), columns = matrix.front().size(); size_t rank = 0;
        for (size_t column = 0; column < columns && rank < rows; ++column) {
            size_t pivot = rank; for (size_t row = rank + 1; row < rows; ++row) if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
            if (std::abs(matrix[pivot][column]) <= tolerance) continue;
            std::swap(matrix[rank], matrix[pivot]);
            for (size_t row = rank + 1; row < rows; ++row) { const double factor = matrix[row][column] / matrix[rank][column]; for (size_t item = column; item < columns; ++item) matrix[row][item] -= factor * matrix[rank][item]; }
            ++rank;
        }
        return rank;
    }
    static std::vector<std::vector<double>> dense_transpose(const std::vector<std::vector<double>>& matrix) {
        std::vector<std::vector<double>> result(matrix.front().size(), std::vector<double>(matrix.size()));
        for (size_t row = 0; row < matrix.size(); ++row) for (size_t column = 0; column < matrix[row].size(); ++column) result[column][row] = matrix[row][column];
        return result;
    }
    static std::vector<std::vector<double>> dense_multiply(const std::vector<std::vector<double>>& left, const std::vector<std::vector<double>>& right) {
        if (left.empty() || right.empty() || left.front().size() != right.size()) throw Error("matrix dimensions do not agree");
        std::vector<std::vector<double>> result(left.size(), std::vector<double>(right.front().size(), 0));
        for (size_t row = 0; row < left.size(); ++row) for (size_t pivot = 0; pivot < right.size(); ++pivot) for (size_t column = 0; column < right[pivot].size(); ++column) result[row][column] += left[row][pivot] * right[pivot][column];
        return result;
    }
    static std::pair<std::vector<std::vector<double>>, std::vector<std::vector<double>>> qr_decompose(const std::vector<std::vector<double>>& matrix) {
        const size_t rows = matrix.size(), columns = matrix.front().size();
        if (rows < columns) throw Error("qr requires at least as many rows as columns");
        std::vector<std::vector<double>> q(rows, std::vector<double>(columns, 0));
        std::vector<std::vector<double>> r(columns, std::vector<double>(columns, 0));
        for (size_t column = 0; column < columns; ++column) {
            std::vector<double> vector(rows); for (size_t row = 0; row < rows; ++row) vector[row] = matrix[row][column];
            for (size_t previous = 0; previous < column; ++previous) {
                double projection = 0; for (size_t row = 0; row < rows; ++row) projection += q[row][previous] * matrix[row][column];
                r[previous][column] = projection; for (size_t row = 0; row < rows; ++row) vector[row] -= projection * q[row][previous];
            }
            double length = 0; for (double value : vector) length += value * value; length = std::sqrt(length);
            if (length < 1e-12) throw Error("qr requires linearly independent columns");
            r[column][column] = length; for (size_t row = 0; row < rows; ++row) q[row][column] = vector[row] / length;
        }
        return {q, r};
    }
    static std::vector<std::vector<double>> cholesky_factor(const std::vector<std::vector<double>>& matrix) {
        const size_t size = matrix.size(); if (matrix.front().size() != size) throw Error("cholesky requires a square matrix");
        std::vector<std::vector<double>> lower(size, std::vector<double>(size, 0));
        for (size_t row = 0; row < size; ++row) for (size_t column = 0; column <= row; ++column) {
            double value = matrix[row][column]; for (size_t item = 0; item < column; ++item) value -= lower[row][item] * lower[column][item];
            if (row == column) { if (value <= 1e-12) throw Error("cholesky requires a positive-definite matrix"); lower[row][column] = std::sqrt(value); }
            else lower[row][column] = value / lower[column][column];
        }
        return lower;
    }
    static std::vector<std::vector<double>> dense_inverse(std::vector<std::vector<double>> matrix, double tolerance) {
        const size_t n = matrix.size(); if (n == 0 || matrix.front().size() != n) throw Error("matrix must be square");
        std::vector<std::vector<double>> augmented(n, std::vector<double>(2 * n, 0));
        for (size_t row = 0; row < n; ++row) for (size_t column = 0; column < n; ++column) { augmented[row][column] = matrix[row][column]; augmented[row][n + column] = row == column ? 1 : 0; }
        for (size_t column = 0; column < n; ++column) {
            size_t pivot = column; for (size_t row = column + 1; row < n; ++row) if (std::abs(augmented[row][column]) > std::abs(augmented[pivot][column])) pivot = row;
            if (std::abs(augmented[pivot][column]) <= tolerance) throw Error("matrix is rank deficient for pseudoinverse");
            std::swap(augmented[pivot], augmented[column]); const double diagonal = augmented[column][column];
            for (double& item : augmented[column]) item /= diagonal;
            for (size_t row = 0; row < n; ++row) if (row != column) { const double factor = augmented[row][column]; for (size_t item = 0; item < 2 * n; ++item) augmented[row][item] -= factor * augmented[column][item]; }
        }
        std::vector<std::vector<double>> result(n, std::vector<double>(n)); for (size_t row = 0; row < n; ++row) for (size_t column = 0; column < n; ++column) result[row][column] = augmented[row][n + column]; return result;
    }
    static std::pair<std::vector<double>, std::vector<std::vector<double>>> symmetric_eigen(std::vector<std::vector<double>> matrix) {
        const size_t size = matrix.size();
        std::vector<std::vector<double>> vectors(size, std::vector<double>(size, 0));
        for (size_t index = 0; index < size; ++index) vectors[index][index] = 1;
        for (size_t iteration = 0; iteration < 100 * size * size; ++iteration) {
            size_t p = 0, q = 0; double largest = 0;
            for (size_t row = 0; row < size; ++row) for (size_t column = row + 1; column < size; ++column) if (std::abs(matrix[row][column]) > largest) { largest = std::abs(matrix[row][column]); p = row; q = column; }
            if (largest < 1e-14) break;
            const double angle = 0.5 * std::atan2(2 * matrix[p][q], matrix[q][q] - matrix[p][p]);
            const double cosine = std::cos(angle), sine = std::sin(angle);
            const double app = matrix[p][p], aqq = matrix[q][q], apq = matrix[p][q];
            matrix[p][p] = cosine * cosine * app - 2 * sine * cosine * apq + sine * sine * aqq;
            matrix[q][q] = sine * sine * app + 2 * sine * cosine * apq + cosine * cosine * aqq;
            matrix[p][q] = matrix[q][p] = 0;
            for (size_t index = 0; index < size; ++index) if (index != p && index != q) {
                const double aip = matrix[index][p], aiq = matrix[index][q];
                matrix[index][p] = matrix[p][index] = cosine * aip - sine * aiq;
                matrix[index][q] = matrix[q][index] = sine * aip + cosine * aiq;
            }
            for (size_t index = 0; index < size; ++index) {
                const double vip = vectors[index][p], viq = vectors[index][q];
                vectors[index][p] = cosine * vip - sine * viq;
                vectors[index][q] = sine * vip + cosine * viq;
            }
        }
        std::vector<double> values(size); for (size_t index = 0; index < size; ++index) values[index] = matrix[index][index];
        return {values, vectors};
    }
    static std::vector<std::vector<double>> pseudoinverse(const std::vector<std::vector<double>>& matrix, double tolerance) {
        const size_t rows = matrix.size(), columns = matrix.front().size();
        const auto transpose = dense_transpose(matrix);
        const double threshold = tolerance;
        if (rows >= columns) {
            const auto [values, vectors] = symmetric_eigen(dense_multiply(transpose, matrix));
            std::vector<std::vector<double>> filtered(columns, std::vector<double>(columns, 0));
            for (size_t row = 0; row < columns; ++row) for (size_t column = 0; column < columns; ++column) for (size_t component = 0; component < columns; ++component) if (values[component] > threshold) filtered[row][column] += vectors[row][component] * vectors[column][component] / values[component];
            return dense_multiply(filtered, transpose);
        }
        const auto [values, vectors] = symmetric_eigen(dense_multiply(matrix, transpose));
        std::vector<std::vector<double>> filtered(rows, std::vector<double>(rows, 0));
        for (size_t row = 0; row < rows; ++row) for (size_t column = 0; column < rows; ++column) for (size_t component = 0; component < rows; ++component) if (values[component] > threshold) filtered[row][column] += vectors[row][component] * vectors[column][component] / values[component];
        return dense_multiply(transpose, filtered);
    }
    static std::vector<std::vector<double>> numeric_matrix(const Value& value) {
        const auto& rows = array(value); if (rows.empty()) throw Error("matrix cannot be empty");
        std::vector<std::vector<double>> result; size_t width = 0;
        for (const auto& row_value : rows) {
            if (!std::holds_alternative<Array>(*row_value)) throw Error("expected a matrix");
            const auto& row = array(*row_value); if (!width) width = row.size();
            if (row.empty() || row.size() != width) throw Error("matrix must be rectangular and non-empty");
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
    static Value complex_vector_value(const std::vector<std::complex<double>>& values) {
        Array result; for (const auto& value : values) { const double real = std::abs(value.real()) < 1e-12 ? 0.0 : value.real(); const double imaginary = std::abs(value.imag()) < 1e-12 ? 0.0 : value.imag(); result.push_back(std::make_shared<Value>(std::abs(imaginary) < 1e-12 ? Value(real) : Value(std::complex<double>(real, imaginary)))); } return result;
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
        if (std::holds_alternative<std::complex<double>>(left) || std::holds_alternative<std::complex<double>>(right)) {
            const std::complex<double> a = as_complex(left), b = as_complex(right);
            if (op == "+") return a + b; if (op == "-") return a - b; if (op == "*" || op == ".*") return a * b;
            if (op == "/" || op == "./") return a / b; if (op == "^") return std::pow(a, b);
            throw Error("operator '" + op + "' is not defined for complex values");
        }
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
    static bool truthy(const Value& value) { if (auto boolean = std::get_if<bool>(&value)) return *boolean; if (auto number = std::get_if<double>(&value)) return *number != 0; if (auto complex = std::get_if<std::complex<double>>(&value)) return std::abs(*complex) != 0; if (std::holds_alternative<std::nullptr_t>(value)) return false; return true; }
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
        if (auto complex = std::get_if<std::complex<double>>(&value)) { std::ostringstream stream; stream << complex->real() << (complex->imag() < 0 ? " - " : " + ") << std::abs(complex->imag()) << "i"; return stream.str(); }
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
        if (auto number = std::get_if<double>(&value)) {
            if (!std::isfinite(*number)) throw Error("cannot serialize a non-finite number to JSON");
            std::ostringstream output; output << *number; return output.str();
        }
        if (auto complex = std::get_if<std::complex<double>>(&value)) {
            if (!std::isfinite(complex->real()) || !std::isfinite(complex->imag())) throw Error("cannot serialize a non-finite complex value to JSON");
            std::ostringstream output; output << "{\"real\":" << complex->real() << ",\"imag\":" << complex->imag() << "}"; return output.str();
        }
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
                if (position < input.size() && input[position] == ',') {
                    ++position; json_space(input, position);
                    if (position >= input.size() || input[position] == '}') throw Error("JSON object has a trailing comma");
                } else break;
            }
            if (position >= input.size() || input[position] != '}') throw Error("unterminated JSON object");
            ++position;
            if (object.size() == 2 && object.count("real") && object.count("imag")) {
                const auto real_value = std::get_if<double>(object["real"].get());
                const auto imaginary_value = std::get_if<double>(object["imag"].get());
                if (real_value && imaginary_value) return std::complex<double>(*real_value, *imaginary_value);
            }
            return object;
        }
        if (input[position] == '[') {
            ++position; Array values; json_space(input, position);
            while (position < input.size() && input[position] != ']') {
                values.push_back(std::make_shared<Value>(parse_json(input, position))); json_space(input, position);
                if (position < input.size() && input[position] == ',') {
                    ++position; json_space(input, position);
                    if (position >= input.size() || input[position] == ']') throw Error("JSON array has a trailing comma");
                } else break;
            }
            if (position >= input.size() || input[position] != ']') throw Error("unterminated JSON array"); ++position; return values;
        }
        if (input[position] == '"') {
            ++position; std::string value;
            auto hex_value = [](char character) -> int {
                if (character >= '0' && character <= '9') return character - '0';
                if (character >= 'a' && character <= 'f') return character - 'a' + 10;
                if (character >= 'A' && character <= 'F') return character - 'A' + 10;
                return -1;
            };
            auto append_utf8 = [&](unsigned codepoint) {
                if (codepoint <= 0x7f) value += static_cast<char>(codepoint);
                else if (codepoint <= 0x7ff) { value += static_cast<char>(0xc0 | (codepoint >> 6)); value += static_cast<char>(0x80 | (codepoint & 0x3f)); }
                else if (codepoint <= 0xffff) { value += static_cast<char>(0xe0 | (codepoint >> 12)); value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)); value += static_cast<char>(0x80 | (codepoint & 0x3f)); }
                else { value += static_cast<char>(0xf0 | (codepoint >> 18)); value += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)); value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)); value += static_cast<char>(0x80 | (codepoint & 0x3f)); }
            };
            while (position < input.size() && input[position] != '"') {
                if (input[position] == '\\' && position + 1 < input.size()) {
                    ++position; const char escaped = input[position++];
                    if (escaped == 'n') value += '\n'; else if (escaped == 'r') value += '\r'; else if (escaped == 't') value += '\t';
                    else if (escaped == 'b') value += '\b'; else if (escaped == 'f') value += '\f';
                    else if (escaped == 'u') {
                        if (position + 4 > input.size()) throw Error("incomplete JSON unicode escape");
                        unsigned codepoint = 0; for (size_t index = 0; index < 4; ++index) { const int digit = hex_value(input[position + index]); if (digit < 0) throw Error("invalid JSON unicode escape"); codepoint = (codepoint << 4) | static_cast<unsigned>(digit); } position += 4;
                        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                            if (position + 6 > input.size() || input[position] != '\\' || input[position + 1] != 'u') throw Error("unpaired JSON unicode surrogate");
                            unsigned low = 0; for (size_t index = 0; index < 4; ++index) { const int digit = hex_value(input[position + 2 + index]); if (digit < 0) throw Error("invalid JSON unicode escape"); low = (low << 4) | static_cast<unsigned>(digit); }
                            if (low < 0xdc00 || low > 0xdfff) throw Error("invalid JSON unicode surrogate pair");
                            position += 6; codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                        } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) throw Error("unpaired JSON unicode surrogate");
                        append_utf8(codepoint);
                    } else if (escaped == '"' || escaped == '\\' || escaped == '/') value += escaped;
                    else throw Error("invalid JSON escape sequence");
                } else value += input[position++];
            }
            if (position >= input.size()) throw Error("unterminated JSON string"); ++position; return value;
        }
        if (input.compare(position, 4, "true") == 0) { position += 4; return true; }
        if (input.compare(position, 5, "false") == 0) { position += 5; return false; }
        if (input.compare(position, 4, "null") == 0) { position += 4; return nullptr; }
        size_t end = position; while (end < input.size() && std::string("0123456789+-.eE").find(input[end]) != std::string::npos) ++end;
        if (end == position) throw Error("invalid JSON value");
        const std::string number_text = input.substr(position, end - position);
        if (!std::regex_match(number_text, std::regex(R"(-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)"))) throw Error("invalid JSON number");
        double value = std::stod(number_text); position = end; return value;
    }
    std::shared_ptr<Environment> environment_;
    std::string base_dir_;
    std::shared_ptr<std::set<std::string>> import_stack_;
    std::mt19937_64 random_engine_{0};
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
        } else if constexpr (std::is_same_v<Node, marila::Expr::Conditional>) {
            return "{\"kind\":\"conditional\",\"condition\":" + ast_expression(node.condition) + ",\"when_true\":" + ast_expression(node.when_true) + ",\"when_false\":" + ast_expression(node.when_false) + "}";
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
        if (statement.line > 0) result += ",\"line\":" + std::to_string(statement.line) + ",\"column\":" + std::to_string(statement.column);
        if (!statement.name.empty()) result += ",\"name\":" + ast_escape(statement.name);
        if (!statement.type.empty()) result += ",\"type\":" + ast_escape(statement.type);
        if (!statement.module.empty()) result += ",\"module\":" + ast_escape(statement.module);
        if (!statement.alias.empty()) result += ",\"alias\":" + ast_escape(statement.alias);
        if (!statement.second_name.empty()) result += ",\"second_name\":" + ast_escape(statement.second_name);
        if (statement.expression) result += ",\"expression\":" + ast_expression(statement.expression);
        if (statement.target) result += ",\"target\":" + ast_expression(statement.target);
        if (!statement.parameters.empty()) {
            result += ",\"parameters\":[";
            for (size_t parameter = 0; parameter < statement.parameters.size(); ++parameter) { if (parameter) result += ','; result += ast_escape(statement.parameters[parameter]); }
            result += "]";
            if (!statement.parameter_types.empty()) {
                result += ",\"parameter_types\":[";
                for (size_t parameter = 0; parameter < statement.parameter_types.size(); ++parameter) { if (parameter) result += ','; result += ast_escape(statement.parameter_types[parameter]); }
                result += "]";
            }
        }
        if (!statement.body.empty()) result += ",\"body\":" + ast_statements(statement.body);
        if (!statement.alternate.empty()) result += ",\"else\":" + ast_statements(statement.alternate);
        if (!statement.finalizer.empty()) result += ",\"finally\":" + ast_statements(statement.finalizer);
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
    case marila::SceneNode::Kind::Wait: return "wait";
    case marila::SceneNode::Kind::Line: return "line";
    case marila::SceneNode::Kind::Triangle: return "triangle";
    case marila::SceneNode::Kind::Circle: return "circle";
    case marila::SceneNode::Kind::Plot2D: return "plot2d";
    case marila::SceneNode::Kind::Data2D: return "data2d";
    case marila::SceneNode::Kind::Parametric2D: return "parametric2d";
    case marila::SceneNode::Kind::Parametric4D: return "parametric4d";
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
    add_string("third", node.third); add_string("fourth", node.fourth); add_string("label", node.label); add_string("title", node.title);
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
        add_number("from_x", node.from_x); add_number("from_y", node.from_y); add_number("x", node.x); add_number("y", node.y); add_number("duration", node.duration); add_number("start", node.start_time); result += ",\"repeat\":" + std::to_string(node.repeat) + ",\"yoyo\":" + std::string(node.yoyo ? "true" : "false");
    }
    if (node.kind == marila::SceneNode::Kind::Fade) { add_number("duration", node.duration); add_number("start", node.start_time); add_string("target", node.first); result += ",\"repeat\":" + std::to_string(node.repeat) + ",\"yoyo\":" + std::string(node.yoyo ? "true" : "false"); if (node.fade_in) result += ",\"fade_in\":true"; else result += ",\"fade_in\":false"; }
    if (node.kind == marila::SceneNode::Kind::Wait) { add_number("duration", node.duration); add_number("start", node.start_time); }
    if (node.kind == marila::SceneNode::Kind::Circle) add_number("radius", node.radius);
    if (node.kind == marila::SceneNode::Kind::Cube) add_number("size", node.size);
    if (node.kind == marila::SceneNode::Kind::Sphere) add_number("radius", node.radius);
    if (node.kind == marila::SceneNode::Kind::Morph) { add_string("from", node.first); add_string("to", node.second); add_number("from_size", node.x); add_number("to_size", node.y); add_number("duration", node.duration); add_number("start", node.start_time); result += ",\"repeat\":" + std::to_string(node.repeat) + ",\"yoyo\":" + std::string(node.yoyo ? "true" : "false"); }
    if (node.kind == marila::SceneNode::Kind::Plot2D || node.kind == marila::SceneNode::Kind::Parametric2D || node.kind == marila::SceneNode::Kind::VectorField2D || node.kind == marila::SceneNode::Kind::Implicit2D || node.kind == marila::SceneNode::Kind::Plot3D) {
        add_number("xmin", node.xmin); add_number("xmax", node.xmax); add_number("ymin", node.ymin); add_number("ymax", node.ymax);
    }
    if (node.dashed) result += ",\"dashed\":true";
    if (node.show_angles) result += ",\"show_angles\":true";
    if (node.animate_trace) result += ",\"animate_trace\":true";
    if (node.animate_trace) {
        add_number("duration", node.duration); add_number("start", node.start_time);
        result += ",\"repeat\":" + std::to_string(node.repeat) + ",\"yoyo\":" + std::string(node.yoyo ? "true" : "false") + ",\"easing\":" + ast_escape(node.easing);
    }
    return result + "}";
}

std::string ast_visual(const std::string& type, const std::vector<marila::SceneNode>& nodes) {
    double timeline_duration = 0;
    for (const auto& node : nodes) timeline_duration = std::max(timeline_duration, node.start_time + node.duration * node.repeat);
    std::string result = "{\"kind\":\"visual-program\",\"type\":" + ast_escape(type) + ",\"timeline_duration\":" + ast_number(timeline_duration) + ",\"nodes\":[";
    for (size_t index = 0; index < nodes.size(); ++index) { if (index) result += ','; result += ast_visual_node(nodes[index]); }
    return result + "]}\n";
}

void collect_imports(const std::vector<marila::Statement>& statements, std::vector<std::string>& imports) {
    for (const auto& statement : statements) {
        if (statement.kind == marila::Statement::Kind::Import) imports.push_back(statement.module);
        collect_imports(statement.body, imports);
        collect_imports(statement.alternate, imports);
        collect_imports(statement.finalizer, imports);
    }
}

std::filesystem::path resolve_module_path(const std::filesystem::path& base, const std::string& module) {
    std::filesystem::path path(module);
    if (path.extension() != ".marila") path += ".marila";
    if (path.is_relative()) path = base / path;
    return std::filesystem::weakly_canonical(path);
}

void build_dependency_graph(const std::filesystem::path& path, std::map<std::string, std::vector<std::string>>& graph, std::set<std::string>& visiting, std::set<std::string>& visited) {
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path);
    const std::string key = canonical.string();
    if (visiting.count(key)) throw marila::Error("circular dependency involving '" + key + "'");
    if (visited.count(key)) return;
    std::ifstream input(canonical);
    if (!input) throw marila::Error("module '" + canonical.string() + "' was not found");
    std::stringstream contents; contents << input.rdbuf();
    if (marila::detect_file_type(contents.str()) != "program") throw marila::Error("dependency '" + canonical.string() + "' is not a program source");
    std::vector<std::string> imports;
    collect_imports(marila::Parser(marila::Lexer(contents.str()).tokenize()).parse(), imports);
    visiting.insert(key);
    auto& edges = graph[key];
    for (const auto& module : imports) {
        const std::filesystem::path dependency = resolve_module_path(canonical.parent_path(), module);
        if (!std::filesystem::exists(dependency)) throw marila::Error("module '" + module + "' imported by '" + key + "' was not found");
        edges.push_back(dependency.string());
        build_dependency_graph(dependency, graph, visiting, visited);
    }
    visiting.erase(key);
    visited.insert(key);
}

std::string dependency_graph_json(const std::map<std::string, std::vector<std::string>>& graph) {
    std::string result = "{\"kind\":\"dependency-graph\",\"modules\":[";
    bool first_module = true;
    for (const auto& entry : graph) {
        if (!first_module) result += ',';
        first_module = false;
        result += "{\"path\":" + ast_escape(entry.first) + ",\"imports\":[";
        for (size_t index = 0; index < entry.second.size(); ++index) { if (index) result += ','; result += ast_escape(entry.second[index]); }
        result += "]}";
    }
    return result + "]}\n";
}

std::string dependency_graph_dot(const std::map<std::string, std::vector<std::string>>& graph) {
    std::string result = "digraph marila_dependencies {\n";
    for (const auto& entry : graph) {
        result += "  \"" + entry.first + "\";\n";
        for (const auto& dependency : entry.second) result += "  \"" + entry.first + "\" -> \"" + dependency + "\";\n";
    }
    return result + "}\n";
}

std::string diagnostic_json(const std::string& diagnostic) {
    std::smatch match;
    int line = 1, column = 1;
    std::string message = diagnostic;
    if (std::regex_match(diagnostic, match, std::regex(R"(^line ([0-9]+), column ([0-9]+): (.*)$)"))) {
        line = std::stoi(match[1]); column = std::stoi(match[2]); message = match[3];
    } else if (std::regex_match(diagnostic, match, std::regex(R"(^line ([0-9]+): (.*)$)"))) {
        line = std::stoi(match[1]); message = match[2];
    }
    return "{\"severity\":\"error\",\"line\":" + std::to_string(line) + ",\"column\":" + std::to_string(column) + ",\"message\":" + ast_escape(message) + "}";
}

std::string token_kind_name(Kind kind) {
    switch (kind) {
        case Kind::End: return "end";
        case Kind::Newline: return "newline";
        case Kind::Number: return "number";
        case Kind::String: return "string";
        case Kind::Identifier: return "identifier";
        case Kind::Operator: return "operator";
        case Kind::Symbol: return "symbol";
    }
    return "unknown";
}

std::string tokens_json(const std::vector<Token>& tokens) {
    std::string result = "{\"kind\":\"tokens\",\"tokens\":[";
    for (size_t index = 0; index < tokens.size(); ++index) {
        if (index) result += ',';
        const auto& token = tokens[index];
        result += "{\"kind\":" + ast_escape(token_kind_name(token.kind)) +
                  ",\"text\":" + ast_escape(token.text) +
                  ",\"line\":" + std::to_string(token.line) +
                  ",\"column\":" + std::to_string(token.column) + "}";
    }
    return result + "]}\n";
}

}  // namespace

} // namespace marila

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: marila-native <run|test|check|ast|tokens|render|export|format|lint|diagnose|deps|repl> [file.marila] [format] [output]\n";
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
                      << "Usage: marila-native <run|test|check|ast|tokens|render|export|format|lint|diagnose|deps|repl> [file.marila] [format] [output]\n"
                      << "  run     execute a program or render a typed source file\n"
                      << "  test    execute a program and report a passing test\n"
                      << "  check   parse or validate a source file\n"
                      << "  ast     emit a machine-readable program syntax tree\n"
                      << "  tokens  emit machine-readable lexical tokens and source positions\n"
                      << "  render  write a renderer output file\n"
                      << "  export  export a source (json, svg, png, html, md, csv, tsv, tex, tikz, pdf, obj, stl, ply, gltf, mp4, gif, or webm)\n"
                      << "  format  format source indentation (append --check or --write)\n"
                      << "  lint    report source issues\n"
                      << "  diagnose emit structured JSON diagnostics\n"
                      << "  deps    emit the resolved import dependency graph\n"
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
        if (command == "tokens") {
            std::cout << marila::tokens_json(marila::Lexer(source).tokenize());
            return 0;
        }
        if (command == "deps") {
            if (marila::detect_file_type(source) != "program") throw marila::Error("deps expects a program source");
            const std::filesystem::path source_path = std::filesystem::weakly_canonical(std::filesystem::absolute(argv[2]));
            std::map<std::string, std::vector<std::string>> graph;
            std::set<std::string> visiting, visited;
            marila::build_dependency_graph(source_path, graph, visiting, visited);
            if (argc >= 4 && std::string(argv[3]) == "--dot") std::cout << marila::dependency_graph_dot(graph);
            else std::cout << marila::dependency_graph_json(graph);
            return 0;
        }
        if (command == "export") {
            if (argc < 5) throw marila::Error("export expects a source file, format, and output path");
            const std::string format = argv[3];
            const std::string type = marila::detect_file_type(source);
            const bool json_export = format == "json";
            const bool static_export = format == "svg" || format == "png" || format == "html" || format == "md" || format == "markdown" || format == "tex" || format == "latex" || format == "tikz" || format == "csv" || format == "tsv";
            const bool pdf_export = format == "pdf";
            const bool geometry_export = format == "obj" || format == "stl" || format == "ply" || format == "gltf";
            const bool video_export = format == "mp4" || format == "gif" || format == "webm";
            if (!json_export && !static_export && !pdf_export && !geometry_export && !video_export) throw marila::Error("unsupported export format '" + format + "' (supported: json, svg, png, html, md, csv, tsv, tex, tikz, pdf, obj, stl, ply, gltf, mp4, gif, webm)");
            if (json_export) {
                std::string output_text;
                if (type == "program") output_text = marila::ast_program(marila::Parser(marila::Lexer(source).tokenize()).parse());
                else if (type == "image" || type == "scene" || type == "video") {
                    const auto diagnostics = marila::validate_visual_source(source);
                    if (!diagnostics.empty()) throw marila::Error(diagnostics.front());
                    output_text = marila::ast_visual(type, marila::parse_visual_source(source));
                }
                else if (type == "sheet") output_text = marila::sheet_json(source);
                else if (type == "document" || type == "latex" || type == "tikz") output_text = marila::text_source_json(type, source);
                else throw marila::Error("JSON export expects a program, sheet, document, latex, tikz, image, scene, or video source");
                std::ofstream output(argv[4]);
                if (!output) throw marila::Error("unable to write JSON export '" + std::string(argv[4]) + "'");
                output << output_text;
            } else if (pdf_export) {
                marila::export_pdf(source, argv[4]);
            } else if (static_export) {
                if ((format == "svg" || format == "png") && type != "image" && type != "scene" && type != "video") throw marila::Error("image export expects an image, scene, or video source");
                if ((format == "html") && type != "document" && type != "sheet" && type != "image" && type != "scene" && type != "video") throw marila::Error("HTML export expects a document, sheet, image, scene, or video source");
                if ((format == "md" || format == "markdown") && type != "document") throw marila::Error("Markdown export expects a document source");
                if ((format == "csv" || format == "tsv") && type != "sheet") throw marila::Error("CSV/TSV export expects a sheet source");
                if ((format == "tex" || format == "latex") && type != "latex") throw marila::Error("TeX export expects a latex source");
                if (format == "tikz" && type != "tikz") throw marila::Error("TikZ export expects a tikz source");
                if (format == "svg") marila::export_svg(source, argv[4]);
                else if (format == "png") marila::export_png(source, argv[4]);
                else if (format == "md" || format == "markdown") { std::ofstream output(argv[4]); if (!output) throw marila::Error("unable to write Markdown export"); output << marila::render_markdown(source); }
                else if (format == "csv" || format == "tsv") { std::ofstream output(argv[4]); if (!output) throw marila::Error("unable to write delimited export"); output << marila::sheet_delimited(source, format == "tsv" ? '\t' : ','); }
                else marila::render_source(source, argv[4]);
            } else if (format == "obj") {
                if (type != "image" && type != "scene" && type != "video") throw marila::Error("OBJ export expects an image, scene, or video source");
                marila::export_obj(source, argv[4]);
            }
            else if (format == "stl") marila::export_stl(source, argv[4]);
            else if (format == "ply") marila::export_ply(source, argv[4]);
            else if (format == "gltf") marila::export_gltf(source, argv[4]);
            else if (format == "mp4" || format == "gif" || format == "webm") marila::export_video(source, format, argv[4]);
            std::cout << "Exported " << argv[4] << '\n';
        } else if (command == "render") {
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
                if (type == "image" || type == "scene" || type == "video") {
                    const auto diagnostics = marila::validate_visual_source(source);
                    if (!diagnostics.empty()) throw marila::Error(diagnostics.front());
                }
            }
            const std::string formatted = marila::format_source(source);
            if (argc >= 4 && std::string(argv[3]) == "--check") {
                if (formatted != source) throw marila::Error("source is not formatted");
                return 0;
            }
            if (argc >= 4 && std::string(argv[3]) == "--write") {
                std::ofstream output(argv[2]);
                if (!output) throw marila::Error("unable to write formatted source");
                output << formatted;
                std::cout << "Formatted " << argv[2] << '\n';
                return 0;
            }
            std::cout << formatted;
        } else if (command == "ast") {
            const std::string type = marila::detect_file_type(source);
            if (type == "program") {
                auto program = marila::Parser(marila::Lexer(source).tokenize()).parse();
                std::cout << marila::ast_program(program);
            } else if (type == "sheet") {
                std::cout << marila::sheet_json(source);
            } else if (type == "document" || type == "latex" || type == "tikz") {
                std::cout << marila::text_source_json(type, source);
            } else {
                if (type == "image" || type == "scene" || type == "video") {
                    const auto diagnostics = marila::validate_visual_source(source);
                    if (!diagnostics.empty()) throw marila::Error(diagnostics.front());
                }
                std::cout << marila::ast_visual(type, marila::parse_visual_source(source));
            }
        } else if (command == "lint") {
            const int lint_status = marila::lint_source(source);
            if (lint_status) return lint_status;
            if (marila::detect_file_type(source) == "program") {
                auto parsed = marila::Parser(marila::Lexer(source).tokenize()).parse();
                std::cout << "OK: linted " << parsed.size() << " statement(s)\n";
            } else {
                const std::string type = marila::detect_file_type(source);
                if (type == "image" || type == "scene" || type == "video") {
                    const auto diagnostics = marila::validate_visual_source(source);
                    if (!diagnostics.empty()) throw marila::Error(diagnostics.front());
                }
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
                } else if (type == "sheet") {
                    std::string model = marila::sheet_json(source);
                    if (!model.empty() && model.back() == '\n') model.pop_back();
                    std::cout << "{\"ok\":true,\"type\":\"sheet\",\"model\":" << model << ",\"diagnostics\":[]}\n";
                } else if (type == "document" || type == "latex" || type == "tikz") {
                    std::string model = marila::text_source_json(type, source);
                    if (!model.empty() && model.back() == '\n') model.pop_back();
                    std::cout << "{\"ok\":true,\"type\":" << marila::ast_escape(type) << ",\"model\":" << model << ",\"diagnostics\":[]}\n";
                } else {
                    if (source.find('[') == std::string::npos || source.find(']') == std::string::npos) throw marila::Error("renderer source requires a bracketed statement block");
                    if (type == "image" || type == "scene" || type == "video") {
                        const auto diagnostics = marila::validate_visual_source(source);
                        if (!diagnostics.empty()) {
                            std::cout << "{\"ok\":false,\"type\":" << marila::ast_escape(type) << ",\"diagnostics\":[";
                            for (size_t index = 0; index < diagnostics.size(); ++index) {
                                if (index) std::cout << ',';
                                std::cout << marila::diagnostic_json(diagnostics[index]);
                            }
                            std::cout << "]}\n";
                            return 1;
                        }
                    }
                    const auto nodes = marila::parse_visual_source(source);
                if ((type == "image" || type == "scene" || type == "video") && nodes.empty()) throw marila::Error("renderer source contains no recognized visual nodes");
                    double timeline_duration = 0;
                    for (const auto& node : nodes) timeline_duration = std::max(timeline_duration, node.start_time + node.duration * node.repeat);
                    std::cout << "{\"ok\":true,\"type\":" << marila::ast_escape(type) << ",\"nodes\":" << nodes.size() << ",\"timeline_duration\":" << timeline_duration << ",\"diagnostics\":[]}\n";
                }
                return 0;
            } catch (const std::exception& error) {
                std::cout << "{\"ok\":false,\"type\":" << marila::ast_escape(type) << ",\"diagnostics\":[" << marila::diagnostic_json(error.what()) << "]}\n";
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
                if (type == "image" || type == "scene" || type == "video") {
                    const auto diagnostics = marila::validate_visual_source(source);
                    if (!diagnostics.empty()) throw marila::Error(diagnostics.front());
                }
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
