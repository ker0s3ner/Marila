"""
Marila Programming Language - Token Definitions
"""

from enum import Enum, auto

class TokenType(Enum):
    # Keywords
    KEYWORD = auto()
    
    # Literals & Identifiers
    IDENTIFIER = auto()
    NUMBER = auto()
    STRING = auto()
    
    # Marila Symbols (:: / \ ^ {} [] <> , . () & $ | ^ !)
    DOUBLE_COLON = auto()  # ::
    SLASH = auto()         # /
    BACKSLASH = auto()     # \
    CARET = auto()         # ^
    LBRACE = auto()        # {
    RBRACE = auto()        # }
    LBRACKET = auto()      # [
    RBRACKET = auto()      # ]
    LANGLE = auto()        # <
    RANGLE = auto()        # >
    COMMA = auto()         # ,
    DOT = auto()           # .
    LPAREN = auto()        # (
    RPAREN = auto()        # )
    AMPERSAND = auto()     # &
    DOLLAR = auto()        # $
    PIPE = auto()          # |
    EXCLAMATION = auto()   # !
    MINUS = auto()         # -
    PLUS = auto()          # +
    STAR = auto()          # *
    EQUALS = auto()        # =
    COLON = auto()         # :
    
    EOF = auto()

KEYWORDS = {
    'file', 'type', 'video', 'document', 'image', 'render',
    'morphs', 'to', 'over', 'draw', 'plot', 'point', 'line',
    'segment', 'circle', 'square', 'triangle', 'polygon', 'ellipse',
    'axes', 'function', 'label', 'theorem', 'with', 'radius', 'size',
    'color', 'at', 'from', 'fill', 'stroke', 'dash', 'show', 'angles',
    'shape'
}

class Token:
    def __init__(self, type_: TokenType, value: str, line: int, col: int):
        self.type = type_
        self.value = value
        self.line = line
        self.col = col

    def __repr__(self):
        return f"Token({self.type.name}, '{self.value}', L{self.line}:C{self.col})"
