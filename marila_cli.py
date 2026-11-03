#!/usr/bin/env python3
"""
Marila Programming Language CLI Compiler
Usage:
    python marila_cli.py run <source_file.marila> [-o <output_file>]
    python marila_cli.py check <source_file.marila>
"""

import sys
import os
import argparse
from pathlib import Path
from marila.lexer import Lexer, LexerError
from marila.parser import Parser, ParserError
from marila.interpreter import Interpreter, RuntimeError
from marila.tooling import format_source, lint_source

def compile_file(source_path: str, output_path: str = None):
    if not os.path.exists(source_path):
        print(f"Error: File '{source_path}' not found.", file=sys.stderr)
        sys.exit(1)

    with open(source_path, "r", encoding="utf-8") as f:
        source_code = f.read()

    print(f"[Marila] Lexing & Parsing '{source_path}'...")
    try:
        lexer = Lexer(source_code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
    except (LexerError, ParserError, RuntimeError) as error:
        print(f"[Marila] Compilation failed: {error}", file=sys.stderr)
        sys.exit(1)

    print(f"[Marila] Executing program of type '{ast.file_type}' ({len(ast.statements)} statements)...")
    try:
        interpreter = Interpreter(ast, Path(source_path).resolve().parent)
        result_path = interpreter.execute(output_path)
    except RuntimeError as error:
        print(f"[Marila] Runtime failed: {error}", file=sys.stderr)
        sys.exit(1)
    return result_path

def main():
    parser = argparse.ArgumentParser(description="Marila Programming Language Compiler & Interpreter")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # run command
    run_parser = subparsers.add_parser("run", help="Compile and execute a Marila script")
    run_parser.add_argument("file", help="Path to .marila source file")
    run_parser.add_argument("-o", "--output", help="Output file path (e.g. out.html, out.svg)")

    # check command
    check_parser = subparsers.add_parser("check", help="Check syntax of a Marila script")
    check_parser.add_argument("file", help="Path to .marila source file")

    subparsers.add_parser("repl", help="Start an interactive Marila session")

    format_parser = subparsers.add_parser("format", help="Format a Marila source file")
    format_parser.add_argument("file", help="Path to a .marila source file")
    format_parser.add_argument("-i", "--in-place", action="store_true", help="Rewrite the file")

    lint_parser = subparsers.add_parser("lint", help="Run static diagnostics")
    lint_parser.add_argument("file", help="Path to a .marila source file")

    args = parser.parse_args()

    if args.command == "run":
        compile_file(args.file, args.output)
    elif args.command == "check":
        try:
            with open(args.file, "r", encoding="utf-8") as f:
                code = f.read()
            tokens = Lexer(code).tokenize()
            ast = Parser(tokens).parse()
        except (OSError, LexerError, ParserError, RuntimeError) as error:
            print(f"[Marila Syntax Check] ERROR: {error}", file=sys.stderr)
            sys.exit(1)
        print(f"[Marila Syntax Check] OK! Recognized file type '{ast.file_type}' with {len(ast.statements)} statement(s).")
    elif args.command == "repl":
        from marila.interpreter import Runtime

        runtime = Runtime()
        print("Marila REPL 1.1.0. Type 'exit' or press Ctrl-D to quit.")
        while True:
            try:
                source = input("marila> ")
            except EOFError:
                print()
                break
            if source.strip().lower() in {"exit", "quit"}:
                break
            if not source.strip():
                continue
            try:
                program = Parser(Lexer(source).tokenize()).parse()
                runtime.execute(program.statements)
            except Exception as error:
                print(f"Error: {error}", file=sys.stderr)
    elif args.command == "format":
        try:
            path = Path(args.file)
            formatted = format_source(path.read_text(encoding="utf-8"))
            if args.in_place:
                path.write_text(formatted, encoding="utf-8")
            else:
                print(formatted, end="")
        except OSError as error:
            print(f"[Marila Format] ERROR: {error}", file=sys.stderr)
            sys.exit(1)
    elif args.command == "lint":
        try:
            source = Path(args.file).read_text(encoding="utf-8")
        except OSError as error:
            print(f"[Marila Lint] ERROR: {error}", file=sys.stderr)
            sys.exit(1)
        diagnostics = lint_source(source)
        for diagnostic in diagnostics:
            print(f"[Marila Lint] {diagnostic}")
        if any(diagnostic.severity == "error" for diagnostic in diagnostics):
            sys.exit(1)

if __name__ == "__main__":
    main()
