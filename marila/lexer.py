"""Lexer for Marila source code."""

from marila.tokens import KEYWORDS, Token, TokenType


class LexerError(Exception):
    def __init__(self, message: str, line: int, col: int):
        super().__init__(f"LexerError at line {line}, column {col}: {message}")
        self.line = line
        self.col = col


class Lexer:
    _operators = {
        "+", "-", "*", "/", "%", "^", "=", "==", "!=", "<", "<=", ">",
        ">=", "->", "=>", "::", "&&", "||", "!", "&", "@", ".*", "./", ".^",
    }
    _symbols = {"(", ")", "[", "]", "{", "}", ",", ":", ";", "."}

    def __init__(self, source: str):
        self.source = source
        self.pos = 0
        self.line = 1
        self.col = 1

    def peek(self, offset: int = 0) -> str:
        index = self.pos + offset
        return self.source[index] if index < len(self.source) else ""

    def advance(self) -> str:
        char = self.peek()
        if not char:
            return ""
        self.pos += 1
        if char == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return char

    def tokenize(self) -> list[Token]:
        tokens: list[Token] = []
        while self.pos < len(self.source):
            char = self.peek()
            if char in " \t\r":
                self.advance()
                continue
            if char == "\n":
                tokens.append(Token(TokenType.NEWLINE, "\n", self.line, self.col))
                self.advance()
                continue
            if char == "#" or (char == "/" and self.peek(1) == "/"):
                while self.peek() and self.peek() != "\n":
                    self.advance()
                continue
            if char in "\"'":
                tokens.append(self._read_string(char))
                continue
            if char.isdigit() or (char == "." and self.peek(1).isdigit()):
                tokens.append(self._read_number())
                continue
            if char.isalpha() or char == "_":
                tokens.append(self._read_identifier())
                continue

            line, col = self.line, self.col
            pair = char + self.peek(1)
            if pair in self._operators:
                self.advance()
                self.advance()
                tokens.append(Token(TokenType.OPERATOR, pair, line, col))
                continue
            if char in self._operators:
                self.advance()
                tokens.append(Token(TokenType.OPERATOR, char, line, col))
                continue
            if char in self._symbols:
                self.advance()
                tokens.append(Token(TokenType.SYMBOL, char, line, col))
                continue
            raise LexerError(f"unexpected character {char!r}", line, col)

        tokens.append(Token(TokenType.EOF, "", self.line, self.col))
        return tokens

    def _read_string(self, quote: str) -> Token:
        line, col = self.line, self.col
        self.advance()
        value: list[str] = []
        while self.peek() and self.peek() != quote:
            if self.peek() == "\\":
                self.advance()
                escaped = self.advance()
                value.append({"n": "\n", "t": "\t", "r": "\r"}.get(escaped, escaped))
            else:
                value.append(self.advance())
        if not self.peek():
            raise LexerError("unterminated string literal", line, col)
        self.advance()
        return Token(TokenType.STRING, "".join(value), line, col)

    def _read_number(self) -> Token:
        line, col = self.line, self.col
        text: list[str] = []
        while self.peek().isdigit() or self.peek() == ".":
            text.append(self.advance())
        raw = "".join(text)
        if raw.count(".") > 1:
            raise LexerError(f"invalid number {raw!r}", line, col)
        value = float(raw) if "." in raw else int(raw)
        return Token(TokenType.NUMBER, value, line, col)

    def _read_identifier(self) -> Token:
        line, col = self.line, self.col
        text: list[str] = []
        while self.peek().isalnum() or self.peek() == "_":
            text.append(self.advance())
        value = "".join(text)
        token_type = TokenType.KEYWORD if value.lower() in KEYWORDS else TokenType.IDENTIFIER
        return Token(token_type, value, line, col)
