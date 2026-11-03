"""Token definitions for the Marila language."""

from dataclasses import dataclass
from enum import Enum, auto


class TokenType(Enum):
    KEYWORD = auto()
    IDENTIFIER = auto()
    NUMBER = auto()
    STRING = auto()
    NEWLINE = auto()
    EOF = auto()
    OPERATOR = auto()
    SYMBOL = auto()


KEYWORDS = {
    "and", "at", "axes", "circle", "color", "dash", "def", "document",
    "draw", "plot2d", "plot3d", "plot4d", "else", "file", "fill", "for", "from", "function", "if",
    "image", "in", "label", "line", "morphs", "over", "plot", "point",
    "print", "radius", "render", "return", "segment", "shape", "show",
    "size", "square", "theorem", "to", "triangle", "type", "video", "with",
    "while", "xor", "or", "not", "let", "true", "false", "none", "import",
}


@dataclass(frozen=True)
class Token:
    type: TokenType
    value: object
    line: int
    col: int

    def __repr__(self) -> str:
        return f"Token({self.type.name}, {self.value!r}, L{self.line}:C{self.col})"
