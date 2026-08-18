"""
Marila Programming Language - Lexical Analyzer
"""

from marila.tokens import Token, TokenType, KEYWORDS

class LexerError(Exception):
    def __init__(self, message: str, line: int, col: int):
        super().__init__(f"LexerError at Line {line}, Col {col}: {message}")
        self.line = line
        self.col = col

class Lexer:
    def __init__(self, source: str):
        self.source = source
        self.pos = 0
        self.line = 1
        self.col = 1

    def peek(self, offset: int = 0) -> str:
        idx = self.pos + offset
        return self.source[idx] if idx < len(self.source) else ''

    def advance(() -> str:
        ch = self.peek()
        self.pos += 1
        if ch == '\n':
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return ch

    def tokenize(list[Token]):
        tokens = []

        while self.pos < len(self.source):
            ch = self.peek()

            # Skip whitespace
            if ch.isspace():
                self.advance()
                continue

            # Skip comments (# or //)
            if ch == '#' or (ch == '/' and self.peek(1) == '/'):
                while self.peek() and self.peek() != '\n':
                    self.advance()
                continue

            # Double colon ::
            if ch == ':' and self.peek(1) == ':':
                line, col = self.line, self.col
                self.advance()
                self.advance()
                tokens.append(Token(TokenType.DOUBLE_COLON, '::', line, col))
                continue

            # String literal ("..." or '...')
            if ch in ('"', "'"):
                tokens.append(self.read_string(ch))
                continue

            # Numbers
            if ch.isdigit() or (ch == '-' and self.peek(1).isdigit()):
                tokens.append(self.read_number())
                continue

            # Identifiers & Keywords
            if ch.isalpha() or ch == '_':
                tokens.append(self.read_identifier())
                continue

            # Symbols: :: / \ ^ {} [] <> , . () & $ | ^ ! - + * = :
            symbol_map = {
                '/': TokenType.SLASH,
                '\\': TokenType.BACKSLASH,
                '^': TokenType.CARET,
                '{': TokenType.LBRACE,
                '}': TokenType.RBRACE,
                '[': TokenType.LBRACKET,
                ']': TokenType.RBRACKET,
                '<': TokenType.LANGLE,
                '>': TokenType.RANGLE,
                ',': TokenType.COMMA,
                '.': TokenType.DOT,
                '(': TokenType.LPAREN,
                ')': TokenType.RPAREN,
                '&': TokenType.AMPERSAND,
                '$': TokenType.DOLLAR,
                '|': TokenType.PIPE,
                '!': TokenType.EXCLAMATION,
                '-': TokenType.MINUS,
                '+': TokenType.PLUS,
                '*': TokenType.STAR,
                '=': TokenType.EQUALS,
                ':': TokenType.COLON,
            }

            if ch in symbol_map:
                line, col = self.line, self.col
                val = self.advance()
                tokens.append(Token(symbol_map[ch], val, line, col))
                continue

            raise LexerError(f"Unexpected character '{ch}'", self.line, self.col)

        tokens.append(Token(TokenType.EOF, '', self.line, self.col))
        return tokens

    def read_string(quote_char: str) -> Token:
        start_line, start_col = self.line, self.col
        self.advance() # consume quote
        str_val = ''
        while self.peek() and self.peek() != quote_char:
            if self.peek() == '\\':
                self.advance()
                escaped = self.advance()
                if escaped == 'n': str_val += '\n'
                elif escaped == 't': str_val += '\t'
                else: str_val += escaped
            else:
                str_val += self.advance()
        if self.peek() == quote_char:
            self.advance()
        return Token(TokenType.STRING, str_val, start_line, start_col)

    def read_number() -> Token:
        start_line, start_col = self.line, self.col
        num_str = ''
        if self.peek() == '-':
            num_str += self.advance()
        while self.peek().isdigit():
            num_str += self.advance()
        if self.peek() == '.' and self.peek(1).isdigit():
            num_str += self.advance()
            while self.peek().isdigit():
                num_str += self.advance()
        return Token(TokenType.NUMBER, num_str, start_line, start_col)

    def read_identifier() -> Token:
        start_line, start_col = self.line, self.col
        id_str = ''
        while self.peek().isalnum() or self.peek() == '_':
            id_str += self.advance()
        type_ = TokenType.KEYWORD if id_str.lower() in KEYWORDS else TokenType.IDENTIFIER
        return Token(type_, id_str, start_line, start_col)
