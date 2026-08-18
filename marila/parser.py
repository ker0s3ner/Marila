"""
Marila Programming Language - Recursive Descent Parser
"""

from marila.tokens import Token, TokenType
from marila.ast_nodes import (
    ProgramNode, DocumentNode, MorphNode, PlotNode,
    PointNode, LineNode, TriangleNode, CircleNode
)

class ParserError(Exception):
    def __init__(self, message: str, token: Token = None):
        if token:
            super().__init__(f"ParserError at Line {token.line}, Col {token.col}: {message} (got '{token.value}')")
        else:
            super().__init__(f"ParserError: {message}")

class Parser:
    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.pos = 0

    def peek(self, offset: int = 0) -> Token:
        idx = self.pos + offset
        return self.tokens[idx] if idx < len(self.tokens) else self.tokens[-1]

    def advance() -> Token:
        tok = self.peek()
        self.pos += 1
        return tok

    def match(type_: TokenType, value: str = None) -> bool:
        tok = self.peek()
        if tok.type == type_ and (value is None or tok.value.lower() == value.lower()):
            self.advance()
            return True
        return False

    def expect(type_: TokenType, value: str = None) -> Token:
        tok = self.peek()
        if tok.type == type_ and (value is None or tok.value.lower() == value.lower()):
            return self.advance()
        raise ParserError(f"Expected {value or type_.name}", tok)

    def parse() -> ProgramNode:
        file_type = "video"

        # file type <type>: [ ... ] render <type>
        if self.match(TokenType.KEYWORD, "file"):
            self.expect(TokenType.KEYWORD, "type")
            file_type_tok = self.advance()
            file_type = file_type_tok.value.lower()
            if self.peek().type == TokenType.COLON:
                self.advance()

        if self.peek().type == TokenType.LBRACKET:
            self.advance()

        statements = []
        while self.pos < len(self.tokens):
            tok = self.peek()
            if tok.type == TokenType.EOF or tok.type == TokenType.RBRACKET:
                if tok.type == TokenType.RBRACKET:
                    self.advance()
                break

            stmt = self.parse_statement(file_type)
            if stmt:
                statements.append(stmt)
            else:
                self.advance()

        render_target = None
        if self.match(TokenType.KEYWORD, "render"):
            render_target = self.advance().value

        return ProgramNode(file_type=file_type, statements=statements, render_target=render_target)

    def parse_statement(file_type: str):
        if file_type == "document":
            return self.parse_document_statement()
        elif file_type == "video":
            return self.parse_video_statement()
        elif file_type == "image":
            return self.parse_image_statement()
        return None

    def parse_document_statement() -> DocumentNode:
        tok = self.peek()
        node_id = "p"

        if tok.type in (TokenType.IDENTIFIER, TokenType.KEYWORD):
            node_id = self.advance().value
            if self.peek().type == TokenType.MINUS:
                self.advance()

        text = ""
        if self.peek().type == TokenType.STRING:
            text = self.advance().value
        else:
            words = []
            while self.peek().type not in (TokenType.DOUBLE_COLON, TokenType.RBRACKET, TokenType.EOF):
                words.append(self.advance().value)
            text = " ".join(words)

        subtitle = None
        if self.peek().type == TokenType.DOUBLE_COLON:
            self.advance() # consume ::
            if self.peek().type == TokenType.STRING:
                subtitle = self.advance().value
            else:
                sub_words = []
                while self.peek().type not in (TokenType.RBRACKET, TokenType.EOF):
                    sub_words.append(self.advance().value)
                subtitle = " ".join(sub_words)

        return DocumentNode(node_id=node_id, text=text, subtitle=subtitle)

    def parse_video_statement():
        # Check for morph animation: shape(args) morphs to shape(args)
        shape1 = self.parse_shape_call()
        if shape1:
            if self.match(TokenType.KEYWORD, "morphs"):
                self.expect(TokenType.KEYWORD, "to")
                shape2 = self.parse_shape_call()
                duration = "3s"
                if self.match(TokenType.KEYWORD, "over"):
                    duration = self.advance().value
                return MorphNode(from_shape=shape1, to_shape=shape2, duration=duration)

        if self.match(TokenType.KEYWORD, "plot") or self.match(TokenType.KEYWORD, "draw"):
            expr_words = []
            while self.peek().type not in (TokenType.AMPERSAND, TokenType.RBRACKET, TokenType.EOF):
                expr_words.append(self.advance().value)
            expr = " ".join(expr_words) or "sin(x)"
            
            animate_trace = True
            if self.peek().type == TokenType.AMPERSAND:
                self.advance()
                while self.peek().type not in (TokenType.RBRACKET, TokenType.EOF):
                    self.advance()

            return PlotNode(expression=expr, animate_trace=animate_trace)

        return None

    def parse_shape_call() -> dict:
        tok = self.peek()
        if tok.type in (TokenType.KEYWORD, TokenType.IDENTIFIER):
            name = tok.value.lower()
            if name in ('square', 'circle', 'triangle', 'rectangle', 'axes'):
                self.advance()
                args = {}
                if self.peek().type == TokenType.LPAREN:
                    self.advance()
                    while self.peek().type != TokenType.RPAREN and self.peek().type != TokenType.EOF:
                        key = self.advance().value
                        val = True
                        if self.peek().type in (TokenType.NUMBER, TokenType.STRING, TokenType.IDENTIFIER):
                            val = self.advance().value
                        args[key] = val
                        if self.peek().type == TokenType.COMMA:
                            self.advance()
                    if self.peek().type == TokenType.RPAREN:
                        self.advance()
                return {'name': name, 'args': args}
        return None

    def parse_image_statement():
        if self.match(TokenType.KEYWORD, "point"):
            point_id = self.expect(TokenType.IDENTIFIER).value
            x, y = 0.0, 0.0
            if self.match(TokenType.KEYWORD, "at") or self.peek().type == TokenType.LPAREN:
                if self.peek().type == TokenType.LPAREN: self.advance()
                x = float(self.expect(TokenType.NUMBER).value)
                if self.peek().type == TokenType.COMMA: self.advance()
                y = float(self.expect(TokenType.NUMBER).value)
                if self.peek().type == TokenType.RPAREN: self.advance()

            label = point_id
            if self.match(TokenType.KEYWORD, "label"):
                label = self.advance().value
            return PointNode(point_id=point_id, x=x, y=y, label=label)

        if self.match(TokenType.KEYWORD, "line") or self.match(TokenType.KEYWORD, "segment"):
            from_id = "A"
            to_id = "B"
            if self.match(TokenType.KEYWORD, "from"): from_id = self.advance().value
            if self.match(TokenType.KEYWORD, "to"): to_id = self.advance().value
            return LineNode(from_id=from_id, to_id=to_id)

        if self.match(TokenType.KEYWORD, "triangle"):
            pts = list(self.advance().value)
            return TriangleNode(point_ids=pts, color="amber", show_angles=True)

        if self.match(TokenType.KEYWORD, "circle"):
            center_id = "A"
            radius = 50.0
            dash = False
            while self.peek().type not in (TokenType.RBRACKET, TokenType.EOF):
                val = self.peek().value.lower()
                if val == "at":
                    self.advance()
                    center_id = self.advance().value
                elif val == "radius":
                    self.advance()
                    radius = float(self.expect(TokenType.NUMBER).value)
                elif val in ("dash", "dashed"):
                    dash = True
                    self.advance()
                else:
                    self.advance()
            return CircleNode(center_id=center_id, radius=radius, dash=dash)

        return None
