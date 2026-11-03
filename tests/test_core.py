import io
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from marila.ast_nodes import BinaryNode, DocumentNode, ForNode, IndexNode, LambdaNode
from marila.interpreter import Environment, Runtime
from marila.lexer import Lexer, LexerError
from marila.parser import Parser, ParserError
from marila.tooling import format_source, lint_source


class LexerTests(unittest.TestCase):
    def test_tokens_preserve_newlines_and_numbers(self):
        tokens = Lexer("let answer = 1.5\n").tokenize()
        self.assertEqual([token.value for token in tokens[:5]], ["let", "answer", "=", 1.5, "\n"])

    def test_unterminated_string_is_reported(self):
        with self.assertRaises(LexerError):
            Lexer('"missing').tokenize()


class ParserTests(unittest.TestCase):
    def test_expression_precedence(self):
        program = Parser(Lexer("let x = 2 + 3 * 4\n").tokenize()).parse()
        expression = program.statements[0].value
        self.assertIsInstance(expression, BinaryNode)
        self.assertEqual(expression.operator, "+")
        self.assertEqual(expression.right.operator, "*")

    def test_existing_document_syntax(self):
        source = 'file type document:\n[\np1 - "Title" :: "Author"\n]\nrender document\n'
        program = Parser(Lexer(source).tokenize()).parse()
        self.assertEqual(program.file_type, "document")
        self.assertIsInstance(program.statements[0], DocumentNode)


class RuntimeTests(unittest.TestCase):
    def test_arithmetic_and_builtin_math(self):
        source = "let x = 2 + 3 * 4\nprint sin(x)\n"
        program = Parser(Lexer(source).tokenize()).parse()
        output = io.StringIO()
        with redirect_stdout(output):
            Runtime().execute(program.statements)
        self.assertIn("0.990607", output.getvalue())

    def test_undefined_names_are_errors(self):
        with self.assertRaises(Exception):
            Runtime().evaluate(Parser(Lexer("missing\n").tokenize()).parse().statements[0],
                               Environment())

    def test_for_loop_indexing_and_lambdas(self):
        source = """let values = [1, 2, 3]
let double = x => x * 2
for item in values {
  print double(item)
}
"""
        output = io.StringIO()
        with redirect_stdout(output):
            Runtime().execute(Parser(Lexer(source).tokenize()).parse().statements)
        self.assertEqual(output.getvalue().splitlines(), ["2", "4", "6"])

    def test_elementwise_arrays_and_matrix_helpers(self):
        source = """let values = [1, 2, 3] .* 2
print values[1]
print dot(values, [1, 1, 1])
"""
        output = io.StringIO()
        with redirect_stdout(output):
            Runtime().execute(Parser(Lexer(source).tokenize()).parse().statements)
        self.assertEqual(output.getvalue().splitlines(), ["4", "12"])

    def test_program_file_type_runs_without_renderer(self):
        source = "file type program:\n[\nprint 2 + 2\n]\n"
        output = io.StringIO()
        with redirect_stdout(output):
            from marila.interpreter import Interpreter
            Interpreter(Parser(Lexer(source).tokenize()).parse()).execute()
        self.assertEqual(output.getvalue().splitlines(), ["4"])

    def test_scene_is_a_video_library_surface(self):
        source = 'file type scene:\n[\nsquare(size 10) morphs to circle(size 5)\n]\nrender scene\n'
        program = Parser(Lexer(source).tokenize()).parse()
        self.assertEqual(program.file_type, "scene")
        self.assertEqual(len(program.statements), 1)

    def test_matrix_multiplication_and_multidimensional_indexing(self):
        source = """let left = [[1, 2], [3, 4]]
let right = [[2, 0], [1, 2]]
let product = left * right
print product[1, 0]
print product[0, 1]
"""
        output = io.StringIO()
        with redirect_stdout(output):
            Runtime().execute(Parser(Lexer(source).tokenize()).parse().statements)
        self.assertEqual(output.getvalue().splitlines(), ["10", "4"])

    def test_linspace(self):
        source = "print linspace(0, 1, 3)\n"
        output = io.StringIO()
        with redirect_stdout(output):
            Runtime().execute(Parser(Lexer(source).tokenize()).parse().statements)
        self.assertEqual(output.getvalue().strip(), "[0.0, 0.5, 1.0]")

    def test_graph_dimensions(self):
        source = """file type scene:
[
plot2d sin(x)
plot3d sin(x) * cos(y)
plot4d tesseract
]
render scene
"""
        program = Parser(Lexer(source).tokenize()).parse()
        self.assertEqual([node.dimensions for node in program.statements], [2, 3, 4])

    def test_tooling_formats_and_lints(self):
        source = "file type program:\n[\nlet x = 1\n]\n"
        self.assertIn("  let x = 1", format_source(source))
        self.assertEqual(lint_source(source), [])

    def test_imports_are_resolved_relative_to_the_module(self):
        with tempfile.TemporaryDirectory() as directory:
            module = Path(directory) / "helpers.marila"
            module.write_text("let answer = 42\n", encoding="utf-8")
            runtime = Runtime(Path(directory))
            source = 'import "helpers.marila"\nprint answer\n'
            output = io.StringIO()
            with redirect_stdout(output):
                runtime.execute(Parser(Lexer(source).tokenize()).parse().statements)
            self.assertEqual(output.getvalue().strip(), "42")


if __name__ == "__main__":
    unittest.main()
