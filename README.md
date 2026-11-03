# Marila

Marila is a hybrid mathematical programming language implemented entirely in
portable C++17. The native executable provides the language lexer, parser,
runtime, CLI, and document, geometry, and scene renderers.

## Building

Requirements:

- CMake 3.16 or newer
- A C++17 compiler

```bash
cmake -S native -B build/native
cmake --build build/native --config Release
```

The executable is `build/native/marila-native`.

## CLI

```bash
./build/native/marila-native run examples/hybrid_program.marila
./build/native/marila-native check examples/geometry_demo.marila
./build/native/marila-native render examples/document_demo.marila output.html
./build/native/marila-native format examples/graphs_demo.marila
./build/native/marila-native lint examples/video_demo.marila
./build/native/marila-native repl
```

`run` executes `file type program` sources. For document, image, video, and
scene sources it writes the corresponding native-rendered output. `check`
validates program syntax and recognizes renderer source blocks.

## Language core

The native runtime supports numbers, strings, booleans, `none`, lists,
arithmetic and comparison operators, logical operators, indexing, user
functions, lambdas, conditionals, loops, imports, and built-ins including
`sin`, `cos`, `tan`, `sqrt`, `exp`, `log`, `abs`, `len`, `range`, `sum`,
`dot`, `transpose`, `linspace`, `zeros`, and `ones`.

```marila
file type program:
[
  let values = [1, 2, 3] .* 2
  let double = x => x * 2
  for value in values {
    print double(value)
  }
]
```

Renderer source blocks retain the existing Marila syntax:

```marila
file type image:
[
point A at (0, 0) label "Origin"
point B at (120, 0) label "Base"
line from A to B
]
```

The implementation and build configuration live under [`native/`](native/).
