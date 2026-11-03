"""AST nodes shared by the Marila compiler and runtime."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any


class ASTNode:
    pass


@dataclass
class ProgramNode(ASTNode):
    file_type: str
    statements: list[ASTNode]
    render_target: str | None = None


@dataclass
class LiteralNode(ASTNode):
    value: Any


@dataclass
class IdentifierNode(ASTNode):
    name: str


@dataclass
class ListNode(ASTNode):
    items: list[ASTNode]


@dataclass
class UnaryNode(ASTNode):
    operator: str
    operand: ASTNode


@dataclass
class BinaryNode(ASTNode):
    left: ASTNode
    operator: str
    right: ASTNode


@dataclass
class CallNode(ASTNode):
    callee: ASTNode
    arguments: list[ASTNode]


@dataclass
class IndexNode(ASTNode):
    collection: ASTNode
    index: ASTNode


@dataclass
class LambdaNode(ASTNode):
    parameters: list[str]
    body: ASTNode


@dataclass
class VariableNode(ASTNode):
    name: str
    value: ASTNode


@dataclass
class AssignmentNode(ASTNode):
    name: str
    value: ASTNode


@dataclass
class BlockNode(ASTNode):
    statements: list[ASTNode]


@dataclass
class IfNode(ASTNode):
    condition: ASTNode
    then_branch: BlockNode
    else_branch: BlockNode | None = None


@dataclass
class WhileNode(ASTNode):
    condition: ASTNode
    body: BlockNode


@dataclass
class ForNode(ASTNode):
    name: str
    iterable: ASTNode
    body: BlockNode


@dataclass
class ImportNode(ASTNode):
    module: str


@dataclass
class FunctionNode(ASTNode):
    name: str
    parameters: list[str]
    body: BlockNode


@dataclass
class ReturnNode(ASTNode):
    value: ASTNode | None = None


@dataclass
class PrintNode(ASTNode):
    value: ASTNode


@dataclass
class DocumentNode(ASTNode):
    node_id: str
    text: str
    subtitle: str | None = None


@dataclass
class MorphNode(ASTNode):
    from_shape: dict
    to_shape: dict
    duration: str = "3s"


@dataclass
class PlotNode(ASTNode):
    expression: str
    animate_trace: bool = True
    dimensions: int = 2


@dataclass
class PointNode(ASTNode):
    point_id: str
    x: float
    y: float
    label: str | None = None


@dataclass
class LineNode(ASTNode):
    from_id: str
    to_id: str


@dataclass
class TriangleNode(ASTNode):
    point_ids: list[str]
    color: str = "amber"
    show_angles: bool = True


@dataclass
class CircleNode(ASTNode):
    center_id: str
    radius: float = 50.0
    dash: bool = False
