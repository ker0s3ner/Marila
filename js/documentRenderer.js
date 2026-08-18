/**
 * Marila Document Renderer
 * Typesets AST nodes into clean paper-styled HTML layout.
 */

class DocumentRenderer {
  constructor(containerEl) {
    this.container = containerEl;
  }

  render(evalData) {
    if (!this.container) return;
    this.container.innerHTML = '';

    if (!evalData || !evalData.nodes || evalData.nodes.length === 0) {
      this.container.innerHTML = '<div class="doc-paragraph"><em>No document content parsed.</em></div>';
      return;
    }

    evalData.nodes.forEach((node, index) => {
      const nodeEl = document.createElement('div');
      
      if (index === 0 && node.subtitle) {
        // First node with subtitle is rendered as Document Title & Author
        nodeEl.className = 'doc-header-block';
        nodeEl.innerHTML = `
          <h1 class="doc-title">${node.text}</h1>
          <div class="doc-subtitle">${node.subtitle}</div>
        `;
      } else if (node.id.startsWith('theorem') || node.id.startsWith('def') || node.id.startsWith('prop')) {
        // Theorem / Definition callout box
        nodeEl.className = 'doc-theorem';
        nodeEl.innerHTML = `
          <div class="doc-theorem-title">${this.capitalize(node.id)}: ${node.text}</div>
          ${node.subtitle ? `<div class="doc-paragraph">${node.subtitle}</div>` : ''}
        `;
      } else {
        // Standard Paragraph
        nodeEl.className = 'doc-paragraph';
        let bodyHtml = node.text;
        if (node.subtitle) {
          bodyHtml += ` <span class="doc-subtitle-inline">— ${node.subtitle}</span>`;
        }
        nodeEl.innerHTML = bodyHtml;
      }

      this.container.appendChild(nodeEl);
    });

    // Re-trigger KaTeX rendering on math elements if needed
    if (typeof window !== 'undefined' && window.renderMathInElement) {
      window.renderMathInElement(this.container, {
        delimiters: [
          { left: '<', right: '>', display: false }
        ]
      });
    }
  }

  capitalize(str) {
    return str.charAt(0).toUpperCase() + str.slice(1);
  }
}
