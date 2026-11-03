# Marila Programming Language

**Marila 1.1** is a hybrid mathematical programming language. It combines:

- MATLAB-like values, arithmetic, arrays, numerical functions, and plotting.
- LaTeX-like mathematical documents and inline equations.
- Manim-like scenes, shape morphs, timelines, and animation output.
- TikZ-like Euclidean geometry and SVG diagrams.

The implementation is intentionally layered: the core language evaluates computations;
the document, scene, and geometry features are libraries/backends that consume those
computations.

## Current language core

The Python implementation now has a real lexer, recursive-descent expression parser,
AST, runtime environments, built-in math functions, variables, assignments, lists,
conditionals, loops, user-defined functions, `return`, and `print`.

```marila
let samples = [0, 1, 2, 3]
let scale = 2
let squared = scale ^ 2
print sin(squared)
```

Core expressions support numbers, strings, booleans, lists, arithmetic and comparison
operators, logical operators, function calls, and parentheses. The runtime includes
`sin`, `cos`, `tan`, `sqrt`, `exp`, `log`, `abs`, `len`, `range`, `sum`, `dot`,
`transpose`, `zeros`, and `ones`.

MATLAB-style array operations are available with indexing and elementwise operators:

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

`file type program` executes the computational core without selecting a renderer.
`import "module.marila"` loads public definitions from another Marila source file.

The existing renderer blocks remain supported. `file type scene` is an
animation-oriented alias for `file type video`:

---

##  Language Syntax Overview

Marila uses concise symbol syntax involving `:: / \ ^ {} [] <> , . () & $ | ^ !` and simple keyword blocks similar to BASIC.

### 1. Document File Type (`file type document`)
Used for typesetting mathematical papers, definitions, and equations.

```marila
file type document:
[
p1 - "What is Euclidean Geometry?" :: By Benas Petronis 

p2 - "We define integration over space as <integral |0||1|| x^2 dx> which measures area under parabolic curves."

theorem1 - "Pythagorean Theorem" :: "In any right triangle <a^2 + b^2 = c^2> holds true."
]

render document
```

### 2. Video Animation File Type (`file type video`)
Used for Manim-style vector shape morphing and math graph trace animations.

```marila
file type video:
[ 
 square(size 30) morphs to circle(size 10) over 3s
 plot sin(x) & animate trace
]

render video
```

### 3. Image Geometry File Type (`file type image`)
Used for TikZ-style Euclidean vector diagrams.

```marila
file type image:
[
point A at (0, 0) label "Origin"
point B at (120, 0) label "Base"
point C at (120, 90) label "Apex"
line from A to B
line from B to C
line from C to A
triangle ABC with color "amber" & show angles
circle at A radius 40 & dash
]

render image
```

---

##  Usage CLI Commands

Execute Marila `.marila` scripts using Python:

```bash
# Run and compile video animation
python marila_cli.py run examples/video_demo.marila -o output_video.html

# Run and compile document
python marila_cli.py run examples/document_demo.marila -o output_doc.html

# Run and compile vector diagram
python marila_cli.py run examples/geometry_demo.marila -o output_diagram.svg

# Check syntax of a script
python marila_cli.py check examples/video_demo.marila
```

Run the regression suite with:

```bash
python -m unittest discover -s tests -v
```

Install the language locally with:

```bash
python -m pip install -e .
marila repl
```

The REPL preserves its runtime between input lines, which is useful for exploring
numeric expressions and plotting helpers interactively.

Tooling commands:

```bash
marila format examples/graphs_demo.marila
marila format examples/graphs_demo.marila --in-place
marila lint examples/graphs_demo.marila
```

## Graphing and higher-dimensional rendering

Scenes support three graph/rendering dimensions:

```marila
file type scene:
[
  plot2d sin(x) & animate trace
  plot3d sin(x) * cos(y)
  plot4d tesseract
]
render scene
```

- `plot2d` renders an animated function trace in Cartesian coordinates.
- `plot3d` renders a sampled height surface `z = f(x, y)` with a rotating
  perspective projection.
- `plot4d` renders a rotating tesseract using a 4D rotation, perspective
  projection from 4D to 3D, and then a 3D-to-2D canvas projection.

The 3D and 4D modes are deliberately renderer-independent language commands:
the Python backend emits a self-contained HTML canvas renderer, while the browser
studio has matching projection code. This makes 4D visualization useful for
hypercubes, rotating point clouds, projections, and other unusual geometric
experiments even though a physical display remains two-dimensional.

The current video backend intentionally exports an interactive HTML canvas player.
MP4/WebM export is not silently claimed: it needs a browser capture step (for
example Playwright or a desktop browser) before FFmpeg can encode the captured
frames. The generated HTML is the stable, portable output today.

## Native runtime bootstrap

The repository also contains a C++17 native core under `native/`. It is the
low-latency execution path for the computational language and is intentionally
kept separate from the Python/browser frontends while the remaining language
features are ported:

```bash
cmake -S native -B build/native
cmake --build build/native --config Release
./build/native/marila-native run examples/hybrid_program.marila
```

The native bootstrap currently covers fast lexing, arithmetic expressions,
comparisons, booleans, arrays, `let`, assignment, `print`, and built-in math
functions. It reports unsupported higher-level constructs instead of silently
changing their meaning. The Python implementation remains the compatibility
reference until the document, scene, graph, import, and tooling layers have
native equivalents.

## Hybrid roadmap

The next compatible layers are first-class `scene` and `document` declarations, a
shared intermediate representation for Python and the browser studio, richer module
namespaces, and editor tooling. The existing renderers are the first backends; they
are not the semantic definition of the language.
