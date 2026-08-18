/**
 * Marila Evaluator Engine
 * Transforms AST into renderable document models, animation keyframes, and geometry collections.
 */

class Evaluator {
  constructor(ast) {
    this.ast = ast;
  }

  evaluate() {
    if (!this.ast) return null;

    switch (this.ast.fileType) {
      case 'document':
        return this.evalDocument();
      case 'video':
        return this.evalVideo();
      case 'image':
        return this.evalImage();
      default:
        return { type: 'unknown', data: [] };
    }
  }

  evalDocument() {
    const nodes = [];
    for (const stmt of this.ast.statements) {
      if (stmt.kind === 'DocumentNode') {
        const parsedText = this.parseMathInText(stmt.text);
        nodes.push({
          id: stmt.id,
          text: parsedText.html,
          latex: parsedText.latex,
          raw: stmt.text,
          subtitle: stmt.subtitle
        });
      }
    }
    return {
      type: 'document',
      nodes
    };
  }

  /**
   * Translates Marila math syntax into KaTeX/LaTeX strings
   * Examples:
   *  <integral |0||1|| x^2 dx> -> \int_{0}^{1} x^2 \, dx
   *  <a^2 + b^2 = c^2> -> a^2 + b^2 = c^2
   *  <sum |k=1||n|| k> -> \sum_{k=1}^{n} k
   */
  parseMathInText(text) {
    if (!text) return { html: '', latex: '' };

    let html = text;
    // Replace < ... > blocks with rendered LaTeX
    html = html.replace(/<([^>]+)>/g, (match, mathContent) => {
      let latex = mathContent.trim();

      // Replace Marila integral notation: integral |a||b|| expr
      latex = latex.replace(/integral\s*\|([^|]+)\|\|([^|]+)\|\|\s*(.*)/gi, (m, lower, upper, expr) => {
        return `\\int_{${lower}}^{${upper}} ${expr}`;
      });

      // Replace Marila sum notation: sum |a||b|| expr
      latex = latex.replace(/sum\s*\|([^|]+)\|\|([^|]+)\|\|\s*(.*)/gi, (m, lower, upper, expr) => {
        return `\\sum_{${lower}}^{${upper}} ${expr}`;
      });

      // Render KaTeX HTML if window.katex is available, else fallback
      if (typeof window !== 'undefined' && window.katex) {
        try {
          return window.katex.renderToString(latex, { throwOnError: false });
        } catch (e) {
          return `<span class="katex-fallback">${latex}</span>`;
        }
      }
      return `\\(${latex}\\)`;
    });

    return { html, latex: text };
  }

  evalVideo() {
    const animations = [];

    for (const stmt of this.ast.statements) {
      if (stmt.kind === 'MorphAnimation') {
        animations.push({
          type: 'morph',
          from: stmt.fromShape || { name: 'square', args: { size: 30 } },
          to: stmt.toShape || { name: 'circle', args: { size: 10 } },
          duration: parseFloat(stmt.duration) || 3
        });
      } else if (stmt.kind === 'PlotAnimation') {
        animations.push({
          type: 'plot',
          expression: stmt.expression,
          animateTrace: stmt.animateTrace,
          duration: 4
        });
      } else if (stmt.kind === 'StaticShape') {
        animations.push({
          type: 'static',
          shape: stmt.shape
        });
      }
    }

    // Default animation if empty
    if (animations.length === 0) {
      animations.push({
        type: 'morph',
        from: { name: 'square', args: { size: 30, color: '#06b6d4' } },
        to: { name: 'circle', args: { size: 15, color: '#a855f7' } },
        duration: 3
      });
    }

    return {
      type: 'video',
      animations
    };
  }

  evalImage() {
    const points = {};
    const elements = [];

    for (const stmt of this.ast.statements) {
      if (stmt.kind === 'Point') {
        points[stmt.id] = { x: stmt.x, y: stmt.y, label: stmt.label || stmt.id };
        elements.push({ type: 'point', ...points[stmt.id] });
      } else if (stmt.kind === 'Line') {
        elements.push({
          type: 'line',
          from: points[stmt.from] || { x: -50, y: 0 },
          to: points[stmt.to] || { x: 50, y: 0 },
          fromId: stmt.from,
          toId: stmt.to
        });
      } else if (stmt.kind === 'Triangle') {
        const p1 = points[stmt.points[0]] || { x: 0, y: 0 };
        const p2 = points[stmt.points[1]] || { x: 100, y: 0 };
        const p3 = points[stmt.points[2]] || { x: 100, y: 80 };
        elements.push({
          type: 'triangle',
          points: [p1, p2, p3],
          color: stmt.color || 'amber',
          showAngles: stmt.showAngles
        });
      } else if (stmt.kind === 'Circle') {
        const centerPt = points[stmt.center] || { x: 0, y: 0 };
        elements.push({
          type: 'circle',
          cx: centerPt.x,
          cy: centerPt.y,
          radius: stmt.radius || 40,
          dash: stmt.dash
        });
      }
    }

    return {
      type: 'image',
      points,
      elements
    };
  }
}

if (typeof module !== 'undefined') {
  module.exports = { Evaluator };
}
