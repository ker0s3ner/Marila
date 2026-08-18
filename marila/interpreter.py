"""
Marila Language Interpreter
Executes compiled Marila programs and manages render outputs.
"""

from marila.ast_nodes import ProgramNode
from marila.renderers.document import DocumentCompiler
from marila.renderers.video import VideoCompiler
from marila.renderers.image import ImageCompiler

class Interpreter:
    def __init__(self, program: ProgramNode):
        self.program = program

    def execute(self, output_path: str = None) -> str:
        file_type = self.program.file_type

        if file_type == 'document':
            target_file = output_path or 'output_document.html'
            compiler = DocumentCompiler(self.program)
            out = compiler.compile_to_html(target_file)
            print(f"[Marila Compiler] Compiled document successfully to '{out}'")
            return out
        elif file_type == 'video':
            target_file = output_path or 'output_video.html'
            compiler = VideoCompiler(self.program)
            out = compiler.compile_to_video(target_file)
            print(f"[Marila Motion Graphics] Compiled video timeline successfully to '{out}'")
            return out
        elif file_type == 'image':
            target_file = output_path or 'output_diagram.svg'
            compiler = ImageCompiler(self.program)
            out = compiler.compile_to_svg(target_file)
            print(f"[Marila Euclidean Diagram] Compiled SVG diagram successfully to '{out}'")
            return out
        else:
            raise ValueError(f"Unknown Marila file type: '{file_type}'")
