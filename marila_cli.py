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
from marila.lexer import Lexer
from marila.parser import Parser
from marila.interpreter import Interpreter

def compile_file(source_path: str, output_path: str = None):
    if not os.path.exists(source_path):
        print(f"Error: File '{source_path}' not found.", file=sys.stderr)
        sys.exit(1)

    with open(source_path, "r", encoding="utf-8") as f:
        source_code = f.read()

    print(f"[Marila] Lexing & Parsing '{source_path}'...")
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()

    parser = Parser(tokens)
    ast = parser.parse()

    print(f"[Marila] Executing program of type '{ast.file_type}' ({len(ast.statements)} statements)...")
    interpreter = Interpreter(ast)
    result_path = interpreter.execute(output_path)
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

    args = parser.parse_args()

    if args.command == "run":
        compile_file(args.file, args.output)
    elif args.command == "check":
        with open(args.file, "r", encoding="utf-8") as f:
            code = f.read()
        tokens = Lexer(code).tokenize()
        ast = Parser(tokens).parse()
        print(f"[Marila Syntax Check] OK! Recognized file type '{ast.file_type}' with {len(ast.statements)} statement(s).")

if __name__ == "__main__":
    main()
