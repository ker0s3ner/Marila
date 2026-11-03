"""Formatting and static diagnostics for Marila source files."""

from __future__ import annotations

from dataclasses import dataclass

from marila.lexer import Lexer, LexerError
from marila.parser import Parser, ParserError
from marila.ast_nodes import (
    AssignmentNode, FunctionNode, ImportNode, ProgramNode, VariableNode,
)


@dataclass(frozen=True)
class Diagnostic:
    severity: str
    message: str
    line: int = 1
    col: int = 1

    def __str__(self) -> str:
        return f"{self.severity}: line {self.line}, column {self.col}: {self.message}"


def parse_source(source: str) -> ProgramNode:
    return Parser(Lexer(source).tokenize()).parse()


def lint_source(source: str) -> list[Diagnostic]:
    try:
        program = parse_source(source)
    except (LexerError, ParserError) as error:
        return [Diagnostic("error", str(error))]

    diagnostics: list[Diagnostic] = []
    if not program.statements:
        diagnostics.append(Diagnostic("warning", "program has no statements"))
    names: set[str] = set()
    for statement in program.statements:
        if isinstance(statement, (VariableNode, FunctionNode)):
            name = statement.name
            if name in names:
                diagnostics.append(Diagnostic("warning", f"duplicate top-level definition {name!r}"))
            names.add(name)
        if isinstance(statement, AssignmentNode) and statement.name not in names:
            diagnostics.append(Diagnostic("warning", f"assignment to undefined name {statement.name!r}"))
        if isinstance(statement, ImportNode) and not statement.module:
            diagnostics.append(Diagnostic("error", "import path cannot be empty"))
    return diagnostics


def format_source(source: str) -> str:
    """Apply conservative formatting without changing the source grammar."""
    lines = []
    indent = 0
    for raw_line in source.splitlines():
        stripped = raw_line.strip()
        if not stripped:
            if lines and lines[-1] != "":
                lines.append("")
            continue
        if stripped.startswith("}") or stripped == "]":
            indent = max(0, indent - 1)
        lines.append(("  " * indent) + stripped)
        if stripped.endswith("{") or stripped == "[":
            indent += 1
        if stripped.endswith("}") or stripped.endswith("]"):
            indent = max(0, indent - 1)
    while lines and lines[-1] == "":
        lines.pop()
    return "\n".join(lines) + "\n"
