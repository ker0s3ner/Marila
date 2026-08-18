"""
Marila Image SVG Geometry Compiler (Python)
Renders Euclidean geometry nodes to SVG files.
"""

from marila.ast_nodes import PointNode, LineNode, TriangleNode, CircleNode

class ImageCompiler:
    def __init__(self, program):
        self.program = program

    def compile_to_svg(self, output_path: str = "diagram_output.svg") -> str:
        points = {}
        elements_svg = []

        for stmt in self.program.statements:
            if isinstance(stmt, PointNode):
                points[stmt.point_id] = (stmt.x, stmt.y)
                elements_svg.append(f'<circle cx="{stmt.x}" cy="{-stmt.y}" r="5" fill="#06b6d4" stroke="#fff" stroke-width="2"/>')
                elements_svg.append(f'<text x="{stmt.x+8}" y="{-stmt.y-8}" fill="#f8fafc" font-family="sans-serif" font-size="14" font-weight="bold">{stmt.label} ({stmt.x}, {stmt.y})</text>')
            elif isinstance(stmt, LineNode):
                p1 = points.get(stmt.from_id, (-50, 0))
                p2 = points.get(stmt.to_id, (50, 0))
                elements_svg.append(f'<line x1="{p1[0]}" y1="{-p1[1]}" x2="{p2[0]}" y2="{-p2[1]}" stroke="#6366f1" stroke-width="3"/>')
            elif isinstance(stmt, TriangleNode):
                pts = [points.get(p_id, (0, 0)) for p_id in stmt.point_ids]
                pts_str = " ".join([f"{x},{-y}" for x, y in pts])
                elements_svg.append(f'<polygon points="{pts_str}" fill="rgba(245, 158, 11, 0.25)" stroke="#f59e0b" stroke-width="3"/>')
            elif isinstance(stmt, CircleNode):
                center = points.get(stmt.center_id, (0, 0))
                dash_attr = ' stroke-dasharray="6 4"' if stmt.dash else ''
                elements_svg.append(f'<circle cx="{center[0]}" cy="{-center[1]}" r="{stmt.radius}" fill="rgba(168, 85, 247, 0.15)" stroke="#a855f7" stroke-width="3"{dash_attr}/>')

        content = "\n  ".join(elements_svg)
        svg_code = f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="-200 -200 400 400" style="background:#0f172a">
  <g class="grid" stroke="rgba(255,255,255,0.08)" stroke-width="1">
    <line x1="-200" y1="0" x2="200" y2="0" stroke="rgba(255,255,255,0.3)" stroke-width="2"/>
    <line x1="0" y1="-200" x2="0" y2="200" stroke="rgba(255,255,255,0.3)" stroke-width="2"/>
  </g>
  {content}
</svg>"""

        with open(output_path, "w", encoding="utf-8") as f:
            f.write(svg_code)
        return output_path
