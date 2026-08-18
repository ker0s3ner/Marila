"""
Marila Document Renderer (Python)
Compiles document AST nodes into typeset LaTeX / HTML documents.
"""

import re
from marila.ast_nodes import DocumentNode

class DocumentCompiler:
    def __init__(self, program):
        self.program = program

    def compile_to_html(self, output_path: str = "document_output.html") -> str:
        html_nodes = []
        for stmt in self.program.statements:
            if isinstance(stmt, DocumentNode):
                formatted_text = self.format_math(stmt.text)
                if stmt.subtitle:
                    html_nodes.append(f"""
                    <div class="doc-header">
                        <h1>{formatted_text}</h1>
                        <p class="subtitle">{stmt.subtitle}</p>
                    </div>
                    """)
                elif stmt.node_id.startswith("theorem"):
                    html_nodes.append(f"""
                    <div class="theorem-box">
                        <h3>{stmt.node_id.capitalize()}: {stmt.text}</h3>
                    </div>
                    """)
                else:
                    html_nodes.append(f'<p class="paragraph">{formatted_text}</p>')

        content = "\n".join(html_nodes)
        full_html = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Marila Compiled Math Document</title>
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.css">
    <script defer src="https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.js"></script>
    <script defer src="https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/contrib/auto-render.min.js"
        onload="renderMathInElement(document.body);"></script>
    <style>
        body {{ font-family: 'Times New Roman', serif; max-width: 800px; margin: 40px auto; padding: 20px; line-height: 1.6; background: #fff; color: #111; }}
        h1 {{ border-bottom: 2px solid #333; padding-bottom: 8px; }}
        .subtitle {{ font-style: italic; color: #555; margin-bottom: 30px; }}
        .theorem-box {{ background: #f4f4f9; border-left: 4px solid #6366f1; padding: 15px; margin: 20px 0; }}
        .paragraph {{ margin: 15px 0; text-align: justify; }}
    </style>
</head>
<body>
    {content}
</body>
</html>"""
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(full_html)
        return output_path

    def format_math(self, text: str) -> str:
        # Transform <integral |0||1|| x^2 dx> to KaTeX LaTeX \int_{0}^{1} x^2 \, dx
        def replace_math(match):
            inner = match.group(1)
            inner = re.sub(r'integral\s*\|([^|]+)\|\|([^|]+)\|\|\s*(.*)', r'\\int_{\1}^{\2} \3', inner)
            inner = re.sub(r'sum\s*\|([^|]+)\|\|([^|]+)\|\|\s*(.*)', r'\\sum_{\1}^{\2} \3', inner)
            return f"\\\\({inner}\\\\)"
        return re.sub(r'<([^>]+)>', replace_math, text)
