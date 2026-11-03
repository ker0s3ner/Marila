#include <cmath>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "renderers.hpp"

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
    struct Index { ExprPtr collection; ExprPtr index; };
    struct Lambda { std::vector<std::string> parameters; ExprPtr body; };
    std::variant<Literal, Name, Array, Unary, Binary, Call, Index, Lambda> value;
    template <typename T>
    explicit Expr(T node) : value(std::move(node)) {}
};

struct Statement {
    enum class Kind { Let, Assign, Print, Expression, If, While, For, Function, Return, Import };
    Kind kind;
    std::string name;
    ExprPtr expression;
    std::vector<std::string> parameters;
    std::vector<Statement> body;
    std::vector<Statement> alternate;
    std::string module;
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
        if (match("let")) { std::string name = identifier(); expect("="); return {Statement::Kind::Let, name, expression()}; }
        if (match("print")) return {Statement::Kind::Print, "", expression()};
        if (match("return")) {
            if (check_kind(Kind::Newline) || check("}") || check("]") || check_kind(Kind::End))
                return {Statement::Kind::Return};
            return {Statement::Kind::Return, "", expression()};
        }
        if (match("import")) {
            Token module = current(); advance();
            if (module.kind != Kind::Identifier && module.kind != Kind::String)
                throw Error("line " + std::to_string(module.line) + ": expected module name");
            Statement result{Statement::Kind::Import}; result.module = module.text; return result;
        }
        if (match("if")) {
            Statement result{Statement::Kind::If}; result.expression = expression(); result.body = required_block();
            skip_lines();
            if (match("else")) result.alternate = required_block();
            return result;
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
            while (!check(")")) { result.parameters.push_back(identifier()); if (!match(",")) break; }
            expect(")"); result.body = required_block(); return result;
        }
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
            advance(); std::vector<ExprPtr> values;
            while (!check("]")) { values.push_back(expression()); if (!match(",")) break; }
            expect("]"); return std::make_shared<Expr>(Expr::Array{values});
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
struct Environment;
struct Function;
using FunctionPtr = std::shared_ptr<Function>;
struct Value : std::variant<double, std::string, bool, Array, FunctionPtr, std::nullptr_t> {
    using std::variant<double, std::string, bool, Array, FunctionPtr, std::nullptr_t>::variant;
};
struct Environment {
    std::unordered_map<std::string, Value> values;
    std::shared_ptr<Environment> parent;
};
struct Function {
    std::function<Value(const std::vector<Value>&)> native;
    std::vector<std::string> parameters;
    std::vector<Statement> body;
    std::shared_ptr<Environment> closure;
    ExprPtr lambda_body;
};

class Runtime {
public:
    explicit Runtime(std::shared_ptr<Environment> environment = nullptr, std::string base_dir = ".")
        : environment_(environment ? std::move(environment) : std::make_shared<Environment>()), base_dir_(std::move(base_dir)) {
        if (!environment_->parent && environment_->values.empty()) install_builtins();
    }
    void run(const std::vector<Statement>& statements) { execute(statements, environment_); }
private:
    struct ReturnSignal { Value value; };
    void install_builtins() {
        environment_->values["sin"] = native([](const std::vector<Value>& args) { return std::sin(number(args, 0)); });
        environment_->values["cos"] = native([](const std::vector<Value>& args) { return std::cos(number(args, 0)); });
        environment_->values["tan"] = native([](const std::vector<Value>& args) { return std::tan(number(args, 0)); });
        environment_->values["sqrt"] = native([](const std::vector<Value>& args) { return std::sqrt(number(args, 0)); });
        environment_->values["abs"] = native([](const std::vector<Value>& args) { return std::abs(number(args, 0)); });
        environment_->values["exp"] = native([](const std::vector<Value>& args) { return std::exp(number(args, 0)); });
        environment_->values["log"] = native([](const std::vector<Value>& args) { return std::log(number(args, 0)); });
        environment_->values["len"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("len expects one argument");
            return Value(static_cast<double>(array(args[0]).size()));
        });
        environment_->values["range"] = native([](const std::vector<Value>& args) {
            if (args.empty() || args.size() > 3) throw Error("range expects one to three arguments");
            int start = args.size() == 1 ? 0 : static_cast<int>(number(args, 0));
            int stop = static_cast<int>(number(args, args.size() == 1 ? 0 : 1));
            int step = args.size() == 3 ? static_cast<int>(number(args, 2)) : 1;
            Array result; if (!step) throw Error("range step cannot be zero");
            for (int value = start; step > 0 ? value < stop : value > stop; value += step)
                result.push_back(std::make_shared<Value>(static_cast<double>(value)));
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
        environment_->values["sum"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("sum expects one argument");
            const auto& values = array(args[0]); double result = 0;
            for (const auto& value : values) result += number(*value);
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
        environment_->values["transpose"] = native([](const std::vector<Value>& args) {
            if (args.size() != 1) throw Error("transpose expects one argument");
            const auto& matrix_value = array(args[0]);
            if (matrix_value.empty()) return Value(Array{});
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
        environment_->values["zeros"] = native([](const std::vector<Value>& args) { return matrix(args, 0); });
        environment_->values["ones"] = native([](const std::vector<Value>& args) { return matrix(args, 1); });
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
        Runtime module_runtime(nullptr, path.parent_path().string());
        module_runtime.run(Parser(Lexer(source.str()).tokenize()).parse());
        for (const auto& entry : module_runtime.environment_->values)
            if (!entry.first.empty() && entry.first[0] != '_') environment->values[entry.first] = entry.second;
    }
    void execute(const std::vector<Statement>& statements, const std::shared_ptr<Environment>& environment) {
        for (const auto& statement : statements) execute_statement(statement, environment);
    }
    void execute_statement(const Statement& statement, const std::shared_ptr<Environment>& environment) {
        switch (statement.kind) {
        case Statement::Kind::Let: environment->values[statement.name] = evaluate(statement.expression, environment); break;
        case Statement::Kind::Assign: assign(environment, statement.name, evaluate(statement.expression, environment)); break;
        case Statement::Kind::Print: std::cout << display(evaluate(statement.expression, environment)) << '\n'; break;
        case Statement::Kind::Expression: evaluate(statement.expression, environment); break;
        case Statement::Kind::Import: import_module(statement.module, environment); break;
        case Statement::Kind::Function: {
            auto function = std::make_shared<Function>(); function->parameters = statement.parameters;
            function->body = statement.body; function->closure = environment; environment->values[statement.name] = function; break;
        }
        case Statement::Kind::Return: throw ReturnSignal{statement.expression ? evaluate(statement.expression, environment) : Value(nullptr)};
        case Statement::Kind::If:
            if (truthy(evaluate(statement.expression, environment))) execute(statement.body, child(environment));
            else if (!statement.alternate.empty()) execute(statement.alternate, child(environment));
            break;
        case Statement::Kind::While: {
            size_t guard = 0;
            while (truthy(evaluate(statement.expression, environment))) {
                execute(statement.body, environment); if (++guard > 1000000) throw Error("loop exceeded safety limit");
            }
            break;
        }
        case Statement::Kind::For: {
            Value iterable = evaluate(statement.expression, environment);
            for (const auto& item : array(iterable)) {
                environment->values[statement.name] = *item; execute(statement.body, environment);
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
    Value eval(const Expr::Unary& node, const std::shared_ptr<Environment>& environment) {
        Value value = evaluate(node.value, environment);
        if (node.op == "-") return -number(value);
        if (node.op == "!" || node.op == "not") return !truthy(value);
        throw Error("unsupported unary operator '" + node.op + "'");
    }
    Value eval(const Expr::Binary& node, const std::shared_ptr<Environment>& environment) {
        Value left = evaluate(node.left, environment), right = evaluate(node.right, environment);
        if (node.op == "and" || node.op == "&&") return truthy(left) && truthy(right);
        if (node.op == "or" || node.op == "||") return truthy(left) || truthy(right);
        if (node.op == "==") return left == right;
        if (node.op == "!=") return left != right;
        if (node.op == "+" || node.op == "-" || node.op == "*" || node.op == ".*" || node.op == "/" ||
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
        if ((*function)->native) return (*function)->native(args);
        if (args.size() != (*function)->parameters.size()) throw Error("wrong number of function arguments");
        auto call_environment = child((*function)->closure);
        for (size_t i = 0; i < args.size(); ++i) call_environment->values[(*function)->parameters[i]] = args[i];
        try {
            if ((*function)->lambda_body) return evaluate((*function)->lambda_body, call_environment);
            execute((*function)->body, call_environment);
        } catch (const ReturnSignal& signal) { return signal.value; }
        return Value(nullptr);
    }
    Value eval(const Expr::Index& node, const std::shared_ptr<Environment>& environment) {
        Value result = evaluate(node.collection, environment);
        for (const auto& index : indices(evaluate(node.index, environment))) result = index_value(result, static_cast<size_t>(number(index)));
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
    static double number(const std::vector<Value>& values, size_t index) {
        if (index >= values.size()) throw Error("missing numeric argument");
        return number(values[index]);
    }
    static double number(const Value& value) { if (auto number = std::get_if<double>(&value)) return *number; throw Error("expected a number"); }
    static const Array& array(const Value& value) {
        auto result = std::get_if<Array>(&value); if (!result) throw Error("expected an array"); return *result;
    }
    static std::vector<Value> indices(const Value& value) {
        if (auto values = std::get_if<Array>(&value)) { std::vector<Value> result; for (const auto& item : *values) result.push_back(*item); return result; }
        return {value};
    }
    static Value index_value(const Value& value, size_t index) {
        const auto& values = array(value); if (index >= values.size()) throw Error("index out of range"); return *values[index];
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
    static Value arithmetic(const std::string& op, const Value& left, const Value& right) {
        const auto left_array = std::get_if<Array>(&left), right_array = std::get_if<Array>(&right);
        if (op == "*" && left_array && right_array) return matrix_multiply(left, right);
        if (op == "*" && (left_array || right_array)) return elementwise(op, left, right);
        if (left_array || right_array) return elementwise(op, left, right);
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
        if (!std::holds_alternative<Array>(*a[0]) && !std::holds_alternative<Array>(*b[0])) {
            if (a.size() != b.size()) throw Error("matrix dimensions do not align");
            double result = 0; for (size_t i = 0; i < a.size(); ++i) result += number(*a[i]) * number(*b[i]); return result;
        }
        if (!std::holds_alternative<Array>(*a[0]) || !std::holds_alternative<Array>(*b[0])) throw Error("matrix dimensions do not align");
        const auto& first_row = array(*a[0]); const size_t inner = first_row.size(), columns = array(*b[0]).size();
        if (inner != b.size()) throw Error("matrix dimensions do not align");
        Array result;
        for (const auto& row_value : a) {
            const auto& row = array(*row_value); Array output;
            for (size_t column = 0; column < columns; ++column) { double sum = 0; for (size_t k = 0; k < inner; ++k) sum += number(*row[k]) * number(*array(*b[k])[column]); output.push_back(std::make_shared<Value>(sum)); }
            result.push_back(std::make_shared<Value>(output));
        }
        return result;
    }
    static bool truthy(const Value& value) { if (auto boolean = std::get_if<bool>(&value)) return *boolean; if (auto number = std::get_if<double>(&value)) return *number != 0; return true; }
    static std::string display(const Value& value) {
        if (auto number = std::get_if<double>(&value)) { std::ostringstream stream; stream << *number; return stream.str(); }
        if (auto text = std::get_if<std::string>(&value)) return *text;
        if (auto boolean = std::get_if<bool>(&value)) return *boolean ? "true" : "false";
        std::string result = "["; bool first = true; for (const auto& item : std::get<Array>(value)) { if (!first) result += ", "; first = false; result += display(*item); } return result + "]";
    }
    std::shared_ptr<Environment> environment_;
    std::string base_dir_;
};

} // namespace marila

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: marila-native <run|check|render|format|lint|repl> [file.marila] [output]\n";
        return 2;
    }
    try {
        const std::string command = argv[1];
        if (command == "repl") {
            marila::Runtime runtime;
            std::string line;
            while (std::cout << "marila> " && std::getline(std::cin, line) && line != "exit" && line != "quit") {
                if (line.empty()) continue;
                runtime.run(marila::Parser(marila::Lexer(line + "\n").tokenize()).parse());
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
            std::cout << marila::format_source(source);
        } else if (command == "lint") {
            return marila::lint_source(source);
        } else {
            const std::string type = marila::detect_file_type(source);
            if (command == "check" && type != "program") {
                if (source.find('[') == std::string::npos || source.find(']') == std::string::npos)
                    throw marila::Error("renderer source requires a bracketed statement block");
                std::cout << "OK: recognized " << type << " source\n";
                return 0;
            }
            if (command == "run" && type != "program") {
                const std::string output = argc >= 4 ? argv[3] :
                    (type == "image" ? "marila-output.svg" : "marila-output.html");
                marila::render_source(source, output);
                std::cout << "Rendered " << output << '\n';
                return 0;
            }
            auto program = marila::Parser(marila::Lexer(source).tokenize()).parse();
            if (command == "check") {
                std::cout << "OK: parsed " << program.size() << " statement(s)\n";
            } else if (command == "run") {
                const std::filesystem::path source_path = std::filesystem::absolute(argv[2]);
                marila::Runtime runtime(nullptr, source_path.parent_path().string());
                runtime.run(program);
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
