/**
 * Marila Language Parser
 * Transforms tokens into an AST for video, document, or image file blocks.
 */

class Parser {
  constructor(tokens) {
    this.tokens = tokens;
    this.pos = 0;
  }

  peek() {
    return this.tokens[this.pos];
  }

  advance() {
    const token = this.peek();
    this.pos++;
    return token;
  }

  match(type, val = null) {
    const t = this.peek();
    if (!t) return false;
    if (t.type === type && (val === null || t.value.toLowerCase() === val.toLowerCase())) {
      this.advance();
      return true;
    }
    return false;
  }

  expect(type, val = null) {
    const t = this.peek();
    if (!t) throw new Error(`Unexpected end of input, expected ${val || type}`);
    if (t.type === type && (val === null || t.value.toLowerCase() === val.toLowerCase())) {
      return this.advance();
    }
    throw new Error(`Line ${t.line}, Col ${t.col}: Expected ${val || type} but got '${t.value}'`);
  }

  parse() {
    // Top-level structure:
    // file type <type>: [ ... ] render <type>
    let fileType = 'video';

    if (this.match(TokenType.KEYWORD, 'file')) {
      this.expect(TokenType.KEYWORD, 'type');
      const typeTok = this.advance();
      fileType = typeTok.value.toLowerCase();
      if (this.peek() && this.peek().value === ':') {
        this.advance();
      }
    }

    // Expect opening bracket '['
    if (this.peek() && this.peek().value === '[') {
      this.advance();
    }

    const statements = [];
    while (this.pos < this.tokens.length) {
      const t = this.peek();
      if (!t || t.type === TokenType.EOF) break;
      if (t.value === ']') {
        this.advance();
        break;
      }

      const stmt = this.parseStatement(fileType);
      if (stmt) {
        statements.push(stmt);
      } else {
        // Skip unknown token to prevent infinite loop
        this.advance();
      }
    }

    // Optional: render <type>
    if (this.match(TokenType.KEYWORD, 'render')) {
      if (this.peek() && (this.peek().type === TokenType.KEYWORD || this.peek().type === TokenType.IDENTIFIER)) {
        this.advance();
      }
    }

    return {
      type: 'Program',
      fileType,
      statements
    };
  }

  parseStatement(fileType) {
    if (fileType === 'document') {
      return this.parseDocumentStatement();
    } else if (fileType === 'video') {
      return this.parseVideoStatement();
    } else if (fileType === 'image') {
      return this.parseImageStatement();
    }
    return this.parseGenericStatement();
  }

  // Parse Document Statement (e.g. p1 - "Header" :: Subtitle or p2 - "Body text <math>")
  parseDocumentStatement() {
    const t = this.peek();
    if (!t) return null;

    let id = null;
    if (t.type === TokenType.IDENTIFIER || t.type === TokenType.KEYWORD) {
      id = this.advance().value;
      if (this.peek() && this.peek().value === '-') {
        this.advance(); // consume '-'
      }
    }

    // Content string or text
    let text = '';
    if (this.peek() && this.peek().type === TokenType.STRING) {
      text = this.advance().value;
    } else {
      // Gather unquoted text until newline or symbol
      while (this.peek() && this.peek().value !== '::' && this.peek().value !== ']' && this.peek().type !== TokenType.EOF) {
        text += (text ? ' ' : '') + this.advance().value;
      }
    }

    // Optional subtitle after '::'
    let subtitle = null;
    if (this.peek() && this.peek().value === '::') {
      this.advance(); // consume ::
      if (this.peek() && this.peek().type === TokenType.STRING) {
        subtitle = this.advance().value;
      } else {
        let subText = '';
        while (this.peek() && this.peek().value !== ']' && this.peek().type !== TokenType.EOF) {
          subText += (subText ? ' ' : '') + this.advance().value;
        }
        subtitle = subText;
      }
    }

    return {
      kind: 'DocumentNode',
      id: id || 'p',
      text: text,
      subtitle: subtitle
    };
  }

  // Parse Video Statement (e.g. square(size 30) morphs to circle(size 10) or plot sin(x))
  parseVideoStatement() {
    const t = this.peek();
    if (!t) return null;

    // Check if statement contains "morphs to"
    const shape1 = this.parseShapeCall();
    if (shape1) {
      if (this.match(TokenType.KEYWORD, 'morphs')) {
        this.expect(TokenType.KEYWORD, 'to');
        const shape2 = this.parseShapeCall();
        let duration = '3s';
        if (this.match(TokenType.KEYWORD, 'over')) {
          duration = this.advance().value;
        }
        return {
          kind: 'MorphAnimation',
          fromShape: shape1,
          toShape: shape2,
          duration: duration
        };
      }
      return {
        kind: 'StaticShape',
        shape: shape1
      };
    }

    // Check plot or function statement
    if (this.match(TokenType.KEYWORD, 'plot') || this.match(TokenType.KEYWORD, 'draw')) {
      let funcExpr = '';
      while (this.peek() && this.peek().value !== ']' && this.peek().value !== '&' && this.peek().type !== TokenType.EOF) {
        funcExpr += (funcExpr ? ' ' : '') + this.advance().value;
      }
      let animateTrace = false;
      if (this.peek() && this.peek().value === '&') {
        this.advance();
        while (this.peek() && this.peek().value !== ']' && this.peek().type !== TokenType.EOF) {
          if (this.peek().value.toLowerCase() === 'animate' || this.peek().value.toLowerCase() === 'trace') {
            animateTrace = true;
          }
          this.advance();
        }
      }
      return {
        kind: 'PlotAnimation',
        expression: funcExpr || 'x^2',
        animateTrace: animateTrace
      };
    }

    // Fallback unparsed video command
    let lineText = '';
    while (this.peek() && this.peek().value !== ']' && this.peek().type !== TokenType.EOF) {
      lineText += (lineText ? ' ' : '') + this.advance().value;
    }
    return { kind: 'RawCommand', text: lineText };
  }

  // Helper: parse shape call like square(size 30, color "cyan") or point A at (0, 0)
  parseShapeCall() {
    const t = this.peek();
    if (!t) return null;
    const name = t.value.toLowerCase();
    const validShapes = ['square', 'circle', 'triangle', 'rectangle', 'point', 'line', 'polygon', 'axes', 'star'];
    
    if (t.type === TokenType.KEYWORD || t.type === TokenType.IDENTIFIER) {
      if (validShapes.includes(name)) {
        this.advance();
        const args = {};
        if (this.peek() && this.peek().value === '(') {
          this.advance(); // consume '('
          while (this.peek() && this.peek().value !== ')') {
            const key = this.advance().value;
            let val = true;
            if (this.peek() && (this.peek().type === TokenType.NUMBER || this.peek().type === TokenType.STRING || this.peek().type === TokenType.IDENTIFIER)) {
              val = this.advance().value;
            }
            args[key] = val;
            if (this.peek() && this.peek().value === ',') this.advance();
          }
          if (this.peek() && this.peek().value === ')') this.advance();
        }
        return { name, args };
      }
    }
    return null;
  }

  // Parse Image (TikZ) Statement (e.g. point A at (0, 0), line from A to B, circle at C radius 2)
  parseImageStatement() {
    const t = this.peek();
    if (!t) return null;

    if (this.match(TokenType.KEYWORD, 'point')) {
      const id = this.expect(TokenType.IDENTIFIER).value;
      let x = 0, y = 0;
      if (this.match(TokenType.KEYWORD, 'at') || (this.peek() && this.peek().value === '(')) {
        if (this.peek() && this.peek().value === '(') this.advance();
        x = this.expect(TokenType.NUMBER).value;
        if (this.peek() && this.peek().value === ',') this.advance();
        y = this.expect(TokenType.NUMBER).value;
        if (this.peek() && this.peek().value === ')') this.advance();
      }
      let label = id;
      if (this.match(TokenType.KEYWORD, 'label')) {
        label = this.advance().value;
      }
      return { kind: 'Point', id, x, y, label };
    }

    if (this.match(TokenType.KEYWORD, 'line') || this.match(TokenType.KEYWORD, 'segment')) {
      let from = 'A', to = 'B';
      if (this.match(TokenType.KEYWORD, 'from')) from = this.advance().value;
      if (this.match(TokenType.KEYWORD, 'to')) to = this.advance().value;
      return { kind: 'Line', from, to };
    }

    if (this.match(TokenType.KEYWORD, 'triangle')) {
      let pts = [];
      const idStr = this.advance().value; // e.g. ABC
      pts = idStr.split('');
      let color = 'amber';
      let showAngles = false;
      
      // Parse parameters after triangle
      while (this.peek() && this.peek().value !== ']' && this.peek().type !== TokenType.EOF) {
        const val = this.peek().value.toLowerCase();
        if (val === 'color') {
          this.advance();
          color = this.advance().value;
        } else if (val === 'show' || val === 'angles') {
          showAngles = true;
          this.advance();
        } else {
          this.advance();
        }
      }
      return { kind: 'Triangle', points: pts, color, showAngles };
    }

    if (this.match(TokenType.KEYWORD, 'circle')) {
      let center = 'A';
      let radius = 50;
      let dash = false;

      while (this.peek() && this.peek().value !== ']' && this.peek().type !== TokenType.EOF) {
        const val = this.peek().value.toLowerCase();
        if (val === 'at') {
          this.advance();
          center = this.advance().value;
        } else if (val === 'radius') {
          this.advance();
          radius = this.expect(TokenType.NUMBER).value;
        } else if (val === 'dash' || val === 'dashed') {
          dash = true;
          this.advance();
        } else {
          this.advance();
        }
      }
      return { kind: 'Circle', center, radius, dash };
    }

    // Generic fallback image node
    let lineText = '';
    while (this.peek() && this.peek().value !== ']' && this.peek().type !== TokenType.EOF) {
      lineText += (lineText ? ' ' : '') + this.advance().value;
    }
    return { kind: 'GenericDiagram', text: lineText };
  }

  parseGenericStatement() {
    const t = this.advance();
    return { kind: 'Generic', value: t ? t.value : '' };
  }
}

if (typeof module !== 'undefined') {
  module.exports = { Parser };
}
