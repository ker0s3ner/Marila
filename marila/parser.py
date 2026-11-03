"""Recursive-descent parser for Marila.

The renderer commands remain intentionally concise, while the general
expression grammar gives Marila a computational core.
"""

from __future__ import annotations

from marila.ast_nodes import (
    AssignmentNode, BinaryNode, BlockNode, CallNode, CircleNode, DocumentNode,
    ForNode, FunctionNode, IdentifierNode, IfNode, ImportNode, IndexNode,
    LambdaNode, LineNode, ListNode, LiteralNode,
    MorphNode, PlotNode, PointNode, PrintNode, ProgramNode, ReturnNode,
    TriangleNode, UnaryNode, VariableNode, WhileNode,
)
from marila.tokens import Token, TokenType


class ParserError(Exception):
    def __init__(self, message: str, token: Token | None = None):
        if token:
            message = f"line {token.line}, column {token.col}: {message} (got {token.value!r})"
        super().__init__(f"ParserError: {message}")


class Parser:
    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.pos = 0

    def peek(self, offset: int = 0) -> Token:
        return self.tokens[min(self.pos + offset, len(self.tokens) - 1)]

    def advance(self) -> Token:
        token = self.peek()
        self.pos += 1
        return token

    def check(self, value: object) -> bool:
        return self.peek().value == value

    def match(self, value: object) -> bool:
        if self.check(value):
            self.advance()
            return True
        return False

    def expect(self, value: object) -> Token:
        if not self.check(value):
            raise ParserError(f"expected {value!r}", self.peek())
        return self.advance()

    def skip_lines(self) -> None:
        while self.peek().type == TokenType.NEWLINE or self.check(";"):
            self.advance()

    def parse(self) -> ProgramNode:
        self.skip_lines()
        file_type = "program"
        if self.match("file"):
            self.expect("type")
            file_type = str(self.advance().value).lower()
            self.match(":")
        self.skip_lines()
        statements = []
        if self.match("["):
            while not self.check("]") and self.peek().type != TokenType.EOF:
                self.skip_lines()
                if self.check("]"):
                    break
                statement = self.parse_statement(file_type)
                if statement is not None:
                    statements.append(statement)
                self.skip_lines()
            self.expect("]")
        else:
            while self.peek().type != TokenType.EOF:
                statements.append(self.parse_statement(file_type))
                self.skip_lines()
        render_target = None
        if self.match("render"):
            render_target = str(self.advance().value)
        return ProgramNode(file_type, statements, render_target)

    def parse_statement(self, file_type: str):
        if file_type == "document":
            return self._parse_document()
        if file_type == "image":
            return self._parse_image()
        if file_type in ("video", "scene"):
            return self._parse_video()
        return self._parse_general()

    def _parse_document(self):
        token = self.peek()
        node_id = "p"
        if token.type in (TokenType.IDENTIFIER, TokenType.KEYWORD):
            node_id = str(self.advance().value)
            self.match("-")
        text = self._consume_text_until("::", "]")
        subtitle = None
        if self.match("::"):
            subtitle = self._consume_text_until("]")
        return DocumentNode(node_id, text.strip(), subtitle.strip() if subtitle else None)

    def _parse_video(self):
        if self.check("plot") or self.check("draw") or self.check("plot2d") or self.check("plot3d") or self.check("plot4d"):
            command = str(self.advance().value).lower()
            dimensions = {"plot2d": 2, "plot3d": 3, "plot4d": 4}.get(command, 2)
            expression = self._consume_text_until("&", "]")
            animate = False
            if self.match("&"):
                words = self._consume_text_until("]")
                animate = "animate" in words.lower() or "trace" in words.lower()
            return PlotNode(expression.strip(), animate, dimensions)
        first = self._parse_shape_call()
        if first and self.match("morphs"):
            self.expect("to")
            second = self._parse_shape_call()
            duration = "3s"
            if self.match("over"):
                duration = str(self.advance().value)
            return MorphNode(first, second, duration)
        if first:
            return PlotNode(str(first), False)
        return self._parse_general()

    def _parse_shape_call(self):
        if self.peek().value not in {"square", "circle", "triangle", "rectangle", "axes"}:
            return None
        name = str(self.advance().value)
        args = {}
        if self.match("("):
            while not self.check(")") and self.peek().type != TokenType.EOF:
                key = str(self.advance().value)
                value = True
                if self.peek().type in (TokenType.NUMBER, TokenType.STRING, TokenType.IDENTIFIER):
                    value = self.advance().value
                args[key] = value
                self.match(",")
            self.expect(")")
        return {"name": name, "args": args}

    def _parse_image(self):
        if self.match("point"):
            point_id = str(self.advance().value)
            self.match("at")
            self.expect("(")
            x = float(self.advance().value)
            self.match(",")
            y = float(self.advance().value)
            self.expect(")")
            label = point_id
            if self.match("label"):
                label = str(self.advance().value)
            return PointNode(point_id, x, y, label)
        if self.match("line") or self.match("segment"):
            self.match("from")
            start = str(self.advance().value)
            self.match("to")
            return LineNode(start, str(self.advance().value))
        if self.match("triangle"):
            ids = list(str(self.advance().value))
            color = "amber"
            show_angles = False
            while self.peek().type not in (TokenType.NEWLINE, TokenType.EOF) and not self.check("]"):
                if self.match("color"):
                    color = str(self.advance().value)
                elif self.match("show"):
                    self.match("angles")
                    show_angles = True
                else:
                    self.advance()
            return TriangleNode(ids, color, show_angles)
        if self.match("circle"):
            center = "A"
            radius = 50.0
            dash = False
            while self.peek().type not in (TokenType.NEWLINE, TokenType.EOF) and not self.check("]"):
                if self.match("at"):
                    center = str(self.advance().value)
                elif self.match("radius"):
                    radius = float(self.advance().value)
                elif self.match("dash"):
                    dash = True
                else:
                    self.advance()
            return CircleNode(center, radius, dash)
        return self._parse_general()

    def _parse_general(self):
        if self.match("let"):
            name = self.expect_identifier()
            self.expect("=")
            return VariableNode(name, self.parse_expression())
        if self.match("print"):
            return PrintNode(self.parse_expression())
        if self.match("return"):
            return ReturnNode(None if self.peek().type == TokenType.NEWLINE else self.parse_expression())
        if self.match("if"):
            condition = self.parse_expression()
            then_branch = self._parse_block()
            else_branch = self._parse_block() if self.match("else") else None
            return IfNode(condition, then_branch, else_branch)
        if self.match("while"):
            return WhileNode(self.parse_expression(), self._parse_block())
        if self.match("for"):
            name = self.expect_identifier()
            self.expect("in")
            return ForNode(name, self.parse_expression(), self._parse_block())
        if self.match("import"):
            token = self.advance()
            if token.type not in (TokenType.IDENTIFIER, TokenType.KEYWORD, TokenType.STRING):
                raise ParserError("expected a module name", token)
            return ImportNode(str(token.value))
        if self.match("def") or self.match("function"):
            name = self.expect_identifier()
            self.expect("(")
            params = []
            while not self.check(")"):
                params.append(self.expect_identifier())
                if not self.match(","):
                    break
            self.expect(")")
            return FunctionNode(name, params, self._parse_block())
        expression = self.parse_expression()
        if isinstance(expression, IdentifierNode) and self.match("="):
            return AssignmentNode(expression.name, self.parse_expression())
        return expression

    def _parse_block(self) -> BlockNode:
        self.skip_lines()
        if self.match("{"):
            statements = []
            while not self.check("}") and self.peek().type != TokenType.EOF:
                self.skip_lines()
                if not self.check("}"):
                    statements.append(self._parse_general())
                self.skip_lines()
            self.expect("}")
            return BlockNode(statements)
        return BlockNode([self._parse_general()])

    def parse_expression(self, minimum_precedence: int = 0):
        token = self.advance()
        if token.type == TokenType.NUMBER or token.type == TokenType.STRING:
            left = LiteralNode(token.value)
        elif token.value in ("true", "false", "none"):
            left = LiteralNode({"true": True, "false": False, "none": None}[token.value])
        elif token.value in ("-", "!", "not"):
            left = UnaryNode(str(token.value), self.parse_expression(6))
        elif token.value == "(":
            left = self.parse_expression()
            self.expect(")")
        elif token.value == "[":
            items = []
            while not self.check("]"):
                items.append(self.parse_expression())
                if not self.match(","):
                    break
            self.expect("]")
            left = ListNode(items)
        elif token.type in (TokenType.IDENTIFIER, TokenType.KEYWORD):
            left = IdentifierNode(str(token.value))
        else:
            raise ParserError("expected an expression", token)

        while True:
            if self.match("("):
                args = []
                while not self.check(")"):
                    args.append(self.parse_expression())
                    if not self.match(","):
                        break
                self.expect(")")
                left = CallNode(left, args)
                continue
            if self.match("["):
                index = self.parse_expression()
                if self.match(","):
                    indices = [index, self.parse_expression()]
                    while self.match(","):
                        indices.append(self.parse_expression())
                    index = ListNode(indices)
                self.expect("]")
                left = IndexNode(left, index)
                continue
            if self.match("=>"):
                if not isinstance(left, IdentifierNode):
                    raise ParserError("lambda parameter must be an identifier", self.peek())
                left = LambdaNode([left.name], self.parse_expression())
                continue
            operator = self.peek().value
            precedence = {"or": 1, "||": 1, "and": 2, "&&": 2, "==": 3, "!=": 3,
                          "<": 4, "<=": 4, ">": 4, ">=": 4, "+": 5, "-": 5,
                          "*": 6, "/": 6, "%": 6, ".*": 6, "./": 6,
                          "@": 6, ".^": 7, "^": 7}.get(operator, -1)
            if precedence < minimum_precedence:
                break
            self.advance()
            right = self.parse_expression(precedence + (0 if operator == "^" else 1))
            left = BinaryNode(left, str(operator), right)
        return left

    def expect_identifier(self) -> str:
        token = self.advance()
        if token.type not in (TokenType.IDENTIFIER, TokenType.KEYWORD):
            raise ParserError("expected an identifier", token)
        return str(token.value)

    def _consume_text_until(self, *stops: object) -> str:
        words = []
        while self.peek().type not in (TokenType.NEWLINE, TokenType.EOF) and self.peek().value not in stops:
            words.append(str(self.advance().value))
        return " ".join(words)
