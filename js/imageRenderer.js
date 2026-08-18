/**
 * Marila Image (TikZ Geometry) SVG Renderer
 * Renders Euclidean points, lines, triangles, circles, and angles.
 */

class ImageRenderer {
  constructor(svgEl, toolbarBtns) {
    this.svg = svgEl;
    this.showGrid = true;
    this.zoomLevel = 1.0;
    this.viewBox = { x: -250, y: -200, w: 500, h: 400 };

    if (toolbarBtns) {
      if (toolbarBtns.gridToggle) {
        toolbarBtns.gridToggle.addEventListener('click', () => {
          this.showGrid = !this.showGrid;
          toolbarBtns.gridToggle.classList.toggle('active', this.showGrid);
          this.renderCurrent();
        });
      }
      if (toolbarBtns.zoomIn) {
        toolbarBtns.zoomIn.addEventListener('click', () => this.zoom(0.8));
      }
      if (toolbarBtns.zoomOut) {
        toolbarBtns.zoomOut.addEventListener('click', () => this.zoom(1.25));
      }
      if (toolbarBtns.resetView) {
        toolbarBtns.resetView.addEventListener('click', () => {
          this.viewBox = { x: -250, y: -200, w: 500, h: 400 };
          this.updateViewBox();
        });
      }
    }
  }

  zoom(factor) {
    this.viewBox.w *= factor;
    this.viewBox.h *= factor;
    this.viewBox.x = -this.viewBox.w / 2;
    this.viewBox.y = -this.viewBox.h / 2;
    this.updateViewBox();
  }

  updateViewBox() {
    this.svg.setAttribute('viewBox', `${this.viewBox.x} ${this.viewBox.y} ${this.viewBox.w} ${this.viewBox.h}`);
  }

  render(evalData) {
    this.lastEvalData = evalData;
    this.renderCurrent();
  }

  renderCurrent() {
    if (!this.svg) return;
    this.svg.innerHTML = '';
    const evalData = this.lastEvalData;

    // Defs for markers (arrowheads)
    const defs = document.createElementNS('http://www.w3.org/2000/svg', 'defs');
    defs.innerHTML = `
      <marker id="arrow" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
        <path d="M 0 0 L 10 5 L 0 10 z" fill="#6366f1"/>
      </marker>
    `;
    this.svg.appendChild(defs);

    // Render Grid if enabled
    if (this.showGrid) {
      const gridGroup = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      gridGroup.setAttribute('class', 'svg-grid');

      for (let x = -300; x <= 300; x += 50) {
        const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        line.setAttribute('x1', x); line.setAttribute('y1', -300);
        line.setAttribute('x2', x); line.setAttribute('y2', 300);
        line.setAttribute('stroke', x === 0 ? 'rgba(255,255,255,0.25)' : 'rgba(255,255,255,0.06)');
        line.setAttribute('stroke-width', x === 0 ? '2' : '1');
        gridGroup.appendChild(line);
      }

      for (let y = -300; y <= 300; y += 50) {
        const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        line.setAttribute('x1', -300); line.setAttribute('y1', y);
        line.setAttribute('x2', 300); line.setAttribute('y2', y);
        line.setAttribute('stroke', y === 0 ? 'rgba(255,255,255,0.25)' : 'rgba(255,255,255,0.06)');
        line.setAttribute('stroke-width', y === 0 ? '2' : '1');
        gridGroup.appendChild(line);
      }
      this.svg.appendChild(gridGroup);
    }

    if (!evalData || !evalData.elements) return;

    // Render Euclidean Elements
    const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');

    evalData.elements.forEach(elem => {
      if (elem.type === 'point') {
        // Render Point
        const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        circle.setAttribute('cx', elem.x);
        circle.setAttribute('cy', -elem.y); // Flip Y axis for Euclidean coords
        circle.setAttribute('r', 5);
        circle.setAttribute('fill', '#06b6d4');
        circle.setAttribute('stroke', '#ffffff');
        circle.setAttribute('stroke-width', 2);
        g.appendChild(circle);

        // Label
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', elem.x + 8);
        text.setAttribute('y', -elem.y - 8);
        text.setAttribute('fill', '#f8fafc');
        text.setAttribute('font-family', 'Inter, sans-serif');
        text.setAttribute('font-weight', '600');
        text.setAttribute('font-size', '14');
        text.textContent = `${elem.label} (${elem.x}, ${elem.y})`;
        g.appendChild(text);
      } else if (elem.type === 'line') {
        const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        line.setAttribute('x1', elem.from.x);
        line.setAttribute('y1', -elem.from.y);
        line.setAttribute('x2', elem.to.x);
        line.setAttribute('y2', -elem.to.y);
        line.setAttribute('stroke', '#6366f1');
        line.setAttribute('stroke-width', 3);
        g.appendChild(line);
      } else if (elem.type === 'triangle') {
        const polygon = document.createElementNS('http://www.w3.org/2000/svg', 'polygon');
        const ptsStr = elem.points.map(p => `${p.x},${-p.y}`).join(' ');
        polygon.setAttribute('points', ptsStr);
        polygon.setAttribute('fill', 'rgba(245, 158, 11, 0.2)');
        polygon.setAttribute('stroke', '#f59e0b');
        polygon.setAttribute('stroke-width', 3);
        g.appendChild(polygon);

        // Right angle arc if right-triangle
        if (elem.showAngles && elem.points.length >= 3) {
          const p2 = elem.points[1];
          const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
          rect.setAttribute('x', p2.x - 15);
          rect.setAttribute('y', -p2.y - 15);
          rect.setAttribute('width', 15);
          rect.setAttribute('height', 15);
          rect.setAttribute('fill', 'none');
          rect.setAttribute('stroke', '#f59e0b');
          rect.setAttribute('stroke-width', 2);
          g.appendChild(rect);
        }
      } else if (elem.type === 'circle') {
        const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        circle.setAttribute('cx', elem.cx);
        circle.setAttribute('cy', -elem.cy);
        circle.setAttribute('r', elem.radius);
        circle.setAttribute('fill', 'rgba(168, 85, 247, 0.15)');
        circle.setAttribute('stroke', '#a855f7');
        circle.setAttribute('stroke-width', 3);
        if (elem.dash) {
          circle.setAttribute('stroke-dasharray', '6 4');
        }
        g.appendChild(circle);
      }
    });

    this.svg.appendChild(g);
  }
}
