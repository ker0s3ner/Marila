/**
 * Marila Language Lexer / Tokenizer
 * Handles symbol tokens (::, <, >, |, ^, [], {}, (), &, $, !, etc.) and keywords.
 */

const TokenType = {
  KEYWORD: 'KEYWORD',
  IDENTIFIER: 'IDENTIFIER',
  NUMBER: 'NUMBER',
  STRING: 'STRING',
  SYMBOL: 'SYMBOL',
  OPERATOR: 'OPERATOR',
  EOF: 'EOF'
};

const KEYWORDS = new Set([
  'file', 'type', 'video', 'document', 'image', 'render',
  'morphs', 'to', 'over', 'draw', 'plot', 'point', 'line',
  'segment', 'circle', 'square', 'triangle', 'polygon', 'ellipse',
  'axes', 'function', 'label', 'theorem', 'with', 'radius', 'size',
  'color', 'at', 'from', 'fill', 'stroke', 'dash', 'show', 'angles'
]);

class Token {
  constructor(type, value, line, col) {
    this.type = type;
    this.value = value;
    this.line = line;
    this.col = col;
  }
}

class Lexer {
  constructor(source) {
    this.source = source;
    this.pos = 0;
    this.line = 1;
    this.col = 1;
  }

  peek() {
    return this.pos < this.source.length ? this.source[this.pos] : null;
  }

  advance() {
    const ch = this.peek();
    this.pos++;
    if (ch === '\n') {
      this.line++;
      this.col = 1;
    } else {
      this.col++;
    }
    return ch;
  }

  tokenize() {
    const tokens = [];

    while (this.pos < this.source.length) {
      const ch = this.peek();

      // Skip whitespace
      if (/\s/.test(ch)) {
        this.advance();
        continue;
      }

      // Single-line comment (# or //)
      if (ch === '#' || (ch === '/' && this.source[this.pos + 1] === '/')) {
        while (this.peek() && this.peek() !== '\n') {
          this.advance();
        }
        continue;
      }

      // Check double-colon symbol ::
      if (ch === ':' && this.source[this.pos + 1] === ':') {
        const line = this.line, col = this.col;
        this.advance(); this.advance();
        tokens.push(new Token(TokenType.SYMBOL, '::', line, col));
        continue;
      }

      // String literals
      if (ch === '"' || ch === "'") {
        tokens.push(this.readString(ch));
        continue;
      }

      // Numbers
      if (/\d/.test(ch) || (ch === '-' && /\d/.test(this.source[this.pos + 1]))) {
        tokens.push(this.readNumber());
        continue;
      }

      // Identifiers / Keywords
      if (/[a-zA-Z_]/.test(ch)) {
        tokens.push(this.readIdentifier());
        continue;
      }

      // Symbols & Operators (:: / \ ^ {} [] <> , . () & $ | ! - + = *)
      if (/[::/\\^{}\[\]<>,.()&$|^!\-+=\*]/.test(ch)) {
        const line = this.line, col = this.col;
        const symbolChar = this.advance();
        tokens.push(new Token(TokenType.SYMBOL, symbolChar, line, col));
        continue;
      }

      // Fallback: unknown char
      const line = this.line, col = this.col;
      tokens.push(new Token(TokenType.SYMBOL, this.advance(), line, col));
    }

    tokens.push(new Token(TokenType.EOF, null, this.line, this.col));
    return tokens;
  }

  readString(quoteChar) {
    const startLine = this.line, startCol = this.col;
    this.advance(); // consume opening quote
    let str = '';
    while (this.peek() && this.peek() !== quoteChar) {
      if (this.peek() === '\\') {
        this.advance();
        const escaped = this.advance();
        if (escaped === 'n') str += '\n';
        else if (escaped === 't') str += '\t';
        else str += escaped;
      } else {
        str += this.advance();
      }
    }
    if (this.peek() === quoteChar) {
      this.advance(); // consume closing quote
    }
    return new Token(TokenType.STRING, str, startLine, startCol);
  }

  readNumber() {
    const startLine = this.line, startCol = this.col;
    let numStr = '';
    if (this.peek() === '-') {
      numStr += this.advance();
    }
    while (this.peek() && /\d/.test(this.peek())) {
      numStr += this.advance();
    }
    if (this.peek() === '.' && /\d/.test(this.source[this.pos + 1])) {
      numStr += this.advance(); // consume '.'
      while (this.peek() && /\d/.test(this.peek())) {
        numStr += this.advance();
      }
    }
    return new Token(TokenType.NUMBER, parseFloat(numStr), startLine, startCol);
  }

  readIdentifier() {
    const startLine = this.line, startCol = this.col;
    let idStr = '';
    while (this.peek() && /[a-zA-Z0-9_]/.test(this.peek())) {
      idStr += this.advance();
    }
    const type = KEYWORDS.has(idStr.toLowerCase()) ? TokenType.KEYWORD : TokenType.IDENTIFIER;
    return new Token(type, idStr, startLine, startCol);
  }
}

if (typeof module !== 'undefined') {
  module.exports = { Lexer, Token, TokenType };
}
