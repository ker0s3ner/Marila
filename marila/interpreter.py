"""Marila runtime and renderer dispatcher."""

from __future__ import annotations

import math
import operator
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from marila.ast_nodes import (
    AssignmentNode, BinaryNode, BlockNode, CallNode, FunctionNode, IdentifierNode,
    ForNode, IfNode, ImportNode, IndexNode, LambdaNode, ListNode, LiteralNode,
    PrintNode, ProgramNode, ReturnNode, UnaryNode, VariableNode, WhileNode,
)
from marila.lexer import Lexer
from marila.parser import Parser
from marila.renderers.document import DocumentCompiler
from marila.renderers.image import ImageCompiler
from marila.renderers.video import VideoCompiler


class RuntimeError(Exception):
    """A user-facing Marila execution error."""


class Environment:
    def __init__(self, parent: "Environment | None" = None):
        self.values: dict[str, Any] = {}
        self.parent = parent

    def define(self, name: str, value: Any) -> None:
        self.values[name] = value

    def get(self, name: str) -> Any:
        if name in self.values:
            return self.values[name]
        if self.parent:
            return self.parent.get(name)
        raise RuntimeError(f"undefined name {name!r}")

    def assign(self, name: str, value: Any) -> None:
        if name in self.values:
            self.values[name] = value
        elif self.parent:
            self.parent.assign(name, value)
        else:
            raise RuntimeError(f"cannot assign undefined name {name!r}")


@dataclass
class UserFunction:
    declaration: FunctionNode
    closure: Environment

    def __call__(self, arguments: list[Any], runtime: "Runtime") -> Any:
        if len(arguments) != len(self.declaration.parameters):
            raise RuntimeError(
                f"{self.declaration.name} expects {len(self.declaration.parameters)} "
                f"argument(s), got {len(arguments)}"
            )
        environment = Environment(self.closure)
        for name, value in zip(self.declaration.parameters, arguments):
            environment.define(name, value)
        try:
            runtime.execute_block(self.declaration.body, environment)
        except ReturnSignal as signal:
            return signal.value
        return None


@dataclass
class LambdaFunction:
    declaration: LambdaNode
    closure: Environment

    def __call__(self, arguments: list[Any], runtime: "Runtime") -> Any:
        if len(arguments) != len(self.declaration.parameters):
            raise RuntimeError("lambda received the wrong number of arguments")
        environment = Environment(self.closure)
        for name, value in zip(self.declaration.parameters, arguments):
            environment.define(name, value)
        return runtime.evaluate(self.declaration.body, environment)


class ReturnSignal(Exception):
    def __init__(self, value: Any):
        self.value = value


class Runtime:
    def __init__(self, base_dir: Path | None = None):
        self.global_environment = Environment()
        self.base_dir = base_dir or Path.cwd()
        self._install_builtins()

    def _install_builtins(self) -> None:
        for name in ("sin", "cos", "tan", "sqrt", "exp", "log"):
            self.global_environment.define(name, getattr(math, name))
        self.global_environment.define("abs", abs)
        self.global_environment.define("len", len)
        self.global_environment.define("range", self._range)
        self.global_environment.define("sum", sum)
        self.global_environment.define("dot", self._dot)
        self.global_environment.define("transpose", self._transpose)
        self.global_environment.define("linspace", self._linspace)
        self.global_environment.define("zeros", lambda rows, cols=1: self._matrix(rows, cols, 0))
        self.global_environment.define("ones", lambda rows, cols=1: self._matrix(rows, cols, 1))

    def import_module(self, module: str, environment: Environment) -> None:
        path = Path(module)
        if not path.is_absolute():
            path = self.base_dir / path
        if path.suffix != ".marila":
            path = path.with_suffix(".marila")
        if not path.exists():
            raise RuntimeError(f"module {module!r} was not found")
        program = Parser(Lexer(path.read_text(encoding="utf-8")).tokenize()).parse()
        module_runtime = Runtime(path.parent)
        module_runtime.execute(program.statements)
        for name, value in module_runtime.global_environment.values.items():
            if not name.startswith("_"):
                environment.define(name, value)

    @staticmethod
    def _matrix(rows: Any, cols: Any, value: Any) -> list[list[Any]]:
        return [[value for _ in range(int(cols))] for _ in range(int(rows))]

    @staticmethod
    def _range(*args: Any) -> list[int]:
        if not 1 <= len(args) <= 3:
            raise RuntimeError("range expects one to three arguments")
        return list(range(*(int(value) for value in args)))

    @staticmethod
    def _dot(left: list[Any], right: list[Any]) -> Any:
        if len(left) != len(right):
            raise RuntimeError("dot operands must have equal lengths")
        return sum(a * b for a, b in zip(left, right))

    @staticmethod
    def _transpose(matrix: list[list[Any]]) -> list[list[Any]]:
        if not matrix:
            return []
        return [list(column) for column in zip(*matrix)]

    @staticmethod
    def _linspace(start: Any, stop: Any, count: Any = 100) -> list[float]:
        count = int(count)
        if count < 2:
            return [float(start)]
        step = (float(stop) - float(start)) / (count - 1)
        return [float(start) + step * index for index in range(count)]

    def execute(self, statements: list[Any], environment: Environment | None = None) -> list[Any]:
        environment = environment or self.global_environment
        return [self.execute_statement(statement, environment) for statement in statements]

    def execute_block(self, block: BlockNode, environment: Environment) -> list[Any]:
        return self.execute(block.statements, environment)

    def execute_statement(self, node: Any, environment: Environment) -> Any:
        if isinstance(node, VariableNode):
            value = self.evaluate(node.value, environment)
            environment.define(node.name, value)
            return value
        if isinstance(node, AssignmentNode):
            value = self.evaluate(node.value, environment)
            environment.assign(node.name, value)
            return value
        if isinstance(node, FunctionNode):
            function = UserFunction(node, environment)
            environment.define(node.name, function)
            return function
        if isinstance(node, ImportNode):
            self.import_module(node.module, environment)
            return None
        if isinstance(node, PrintNode):
            value = self.evaluate(node.value, environment)
            print(value)
            return value
        if isinstance(node, ReturnNode):
            raise ReturnSignal(self.evaluate(node.value, environment) if node.value else None)
        if isinstance(node, IfNode):
            if self.evaluate(node.condition, environment):
                return self.execute_block(node.then_branch, Environment(environment))
            if node.else_branch:
                return self.execute_block(node.else_branch, Environment(environment))
            return None
        if isinstance(node, WhileNode):
            result = None
            guard = 0
            while self.evaluate(node.condition, environment):
                result = self.execute_block(node.body, Environment(environment))
                guard += 1
                if guard > 1_000_000:
                    raise RuntimeError("loop exceeded the 1,000,000 iteration safety limit")
            return result
        if isinstance(node, ForNode):
            result = None
            for value in self.evaluate(node.iterable, environment):
                environment.define(node.name, value)
                result = self.execute_block(node.body, environment)
            return result
        if isinstance(node, BlockNode):
            return self.execute_block(node, Environment(environment))
        return self.evaluate(node, environment)

    def evaluate(self, node: Any, environment: Environment) -> Any:
        if node is None:
            return None
        if isinstance(node, LiteralNode):
            return node.value
        if isinstance(node, IdentifierNode):
            return environment.get(node.name)
        if isinstance(node, ListNode):
            return [self.evaluate(item, environment) for item in node.items]
        if isinstance(node, IndexNode):
            collection = self.evaluate(node.collection, environment)
            index = self.evaluate(node.index, environment)
            try:
                if isinstance(index, list):
                    value = collection
                    for part in index:
                        value = value[int(part)]
                    return value
                return collection[int(index)]
            except (IndexError, KeyError, TypeError, ValueError) as error:
                raise RuntimeError(f"invalid index {index!r}") from error
        if isinstance(node, LambdaNode):
            return LambdaFunction(node, environment)
        if isinstance(node, UnaryNode):
            value = self.evaluate(node.operand, environment)
            if node.operator == "-":
                return -value
            if node.operator in ("!", "not"):
                return not value
            raise RuntimeError(f"unsupported unary operator {node.operator!r}")
        if isinstance(node, BinaryNode):
            return self._binary(node.operator, self.evaluate(node.left, environment),
                                self.evaluate(node.right, environment))
        if isinstance(node, CallNode):
            callee = self.evaluate(node.callee, environment)
            if not callable(callee):
                raise RuntimeError("only functions can be called")
            arguments = [self.evaluate(arg, environment) for arg in node.arguments]
            if isinstance(callee, (UserFunction, LambdaFunction)):
                return callee(arguments, self)
            return callee(*arguments)
        return None

    def _binary(self, symbol: str, left: Any, right: Any) -> Any:
        operations = {
            "+": lambda a, b: self._elementwise(a, b, operator.add),
            "-": lambda a, b: self._elementwise(a, b, operator.sub),
            "*": lambda a, b: self._multiply(a, b),
            "/": operator.truediv, "%": operator.mod, "^": operator.pow,
            ".*": lambda a, b: self._elementwise(a, b, operator.mul),
            "./": lambda a, b: self._elementwise(a, b, operator.truediv),
            ".^": lambda a, b: self._elementwise(a, b, operator.pow),
            "@": lambda a, b: self._matrix_multiply(a, b),
            "==": operator.eq, "!=": operator.ne, "<": operator.lt,
            "<=": operator.le, ">": operator.gt, ">=": operator.ge,
            "and": lambda a, b: a and b, "&&": lambda a, b: a and b,
            "or": lambda a, b: a or b, "||": lambda a, b: a or b,
        }
        try:
            return operations[symbol](left, right)
        except KeyError as error:
            raise RuntimeError(f"unsupported binary operator {symbol!r}") from error
        except TypeError as error:
            raise RuntimeError(f"cannot apply {symbol!r} to {left!r} and {right!r}") from error

    def _elementwise(self, left: Any, right: Any, operation) -> Any:
        if isinstance(left, list) and isinstance(right, list):
            if len(left) != len(right):
                raise RuntimeError("array operands must have equal lengths")
            return [self._elementwise(a, b, operation) for a, b in zip(left, right)]
        if isinstance(left, list):
            return [self._elementwise(value, right, operation) for value in left]
        if isinstance(right, list):
            return [self._elementwise(left, value, operation) for value in right]
        return operation(left, right)

    def _multiply(self, left: Any, right: Any) -> Any:
        if isinstance(left, list) and not isinstance(right, list):
            return self._elementwise(left, right, operator.mul)
        if isinstance(right, list) and not isinstance(left, list):
            return self._elementwise(left, right, operator.mul)
        if isinstance(left, list) or isinstance(right, list):
            return self._matrix_multiply(left, right)
        return operator.mul(left, right)

    @staticmethod
    def _matrix_multiply(left: Any, right: Any) -> Any:
        if not isinstance(left, list) or not isinstance(right, list):
            return left * right
        if left and not isinstance(left[0], list):
            if right and not isinstance(right[0], list):
                if len(left) != len(right):
                    raise RuntimeError("matrix dimensions do not align")
                return sum(a * b for a, b in zip(left, right))
            right = [list(row) for row in right]
            if len(left) != len(right):
                raise RuntimeError("matrix dimensions do not align")
            return [sum(a * row[column] for a, row in zip(left, right))
                    for column in range(len(right[0]))]
        if not right or not isinstance(right[0], list):
            raise RuntimeError("matrix dimensions do not align")
        if len(left[0]) != len(right):
            raise RuntimeError("matrix dimensions do not align")
        return [
            [sum(left[row][inner] * right[inner][column] for inner in range(len(right)))
             for column in range(len(right[0]))]
            for row in range(len(left))
        ]


class Interpreter:
    def __init__(self, program: ProgramNode, base_dir: Path | None = None):
        self.program = program
        self.runtime = Runtime(base_dir)

    def execute(self, output_path: str | None = None) -> str | None:
        renderer_statements = []
        for statement in self.program.statements:
            if isinstance(statement, (VariableNode, AssignmentNode, FunctionNode, ImportNode,
                                       PrintNode, IfNode, WhileNode, ForNode, ReturnNode)):
                self.runtime.execute_statement(statement, self.runtime.global_environment)
            else:
                renderer_statements.append(statement)

        self.program.statements = renderer_statements
        file_type = self.program.file_type
        if file_type == "document":
            target = output_path or "output_document.html"
            return DocumentCompiler(self.program).compile_to_html(target)
        if file_type in ("video", "scene"):
            target = output_path or "output_video.html"
            return VideoCompiler(self.program).compile_to_video(target)
        if file_type == "image":
            target = output_path or "output_diagram.svg"
            return ImageCompiler(self.program).compile_to_svg(target)
        if file_type == "program":
            return None
        if file_type:
            raise RuntimeError(f"unknown file type {file_type!r}")
        return None
