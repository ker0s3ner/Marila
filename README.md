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
ctest --test-dir build/native --output-on-failure
cmake --install build/native --prefix build/stage
```

The executable is `build/native/marila-native`.
An installed executable is placed at `build/stage/bin/marila-native` when using
the staging command above.

## Testing and layout

```bash
ctest --test-dir build/native --output-on-failure   # runs tests/smoke.cmake against the examples
```

```
native/main.cpp          lexer, parser, runtime and CLI
native/renderers.*       document, LaTeX, sheet, image, TikZ, video and scene renderers
native/scene_model.*     shared renderer AST
examples/                .marila demos (hybrid_program, graphs, sheets, calculus, FFT, ...),
                         plus data files and intentionally invalid inputs used by tests
tests/smoke.cmake        CTest smoke tests
```

## CLI

```bash
./build/native/marila-native run examples/hybrid_program.marila
./build/native/marila-native test examples/assert_demo.marila
./build/native/marila-native check examples/geometry_demo.marila
./build/native/marila-native ast examples/higher_order_demo.marila
./build/native/marila-native render examples/document_demo.marila output.html
./build/native/marila-native format examples/graphs_demo.marila
./build/native/marila-native lint examples/video_demo.marila
./build/native/marila-native diagnose examples/hybrid_program.marila
./build/native/marila-native repl
./build/native/marila-native --version
```

`run` executes `file type program` sources. For document, latex, sheet, image,
video, and scene sources it writes the corresponding native-rendered output. `check`
validates program syntax and recognizes renderer source blocks. `diagnose`
emits structured JSON diagnostics for editor and CI integrations. `ast` emits a
machine-readable JSON syntax tree for program files and a shared visual AST for
renderer sources, suitable for editor integrations and tooling.

## Language core

The native runtime supports numbers, strings, booleans, `none`, lists,
arithmetic and comparison operators, logical operators, indexing, user
functions, lambdas, conditionals, loops, imports, indexed assignment, and built-ins including
`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sqrt`, `floor`, `ceil`, `exp`,
`log`, `log2`, `log10`, `pow`, `sign`, `hypot`, `abs`, `len`, `range`, `sum`,
`dot`, `outer`, `transpose`, `linspace`, `zeros`, `ones`, `mean`, `median`, `variance`,
`std`, `sort`, `cumsum`, `min`, `max`, `find`, `argmin`, and `argmax`,
`matmul`, `shape`, `reshape`, `det`, `inverse`, and `solve`, plus the constants `pi`
and `e`, as well as `norm`, `normalize`, `cross`, `differentiate`,
`integrate`, `root`, `ode_euler`, `ode_rk4`, `fft`, `ifft`, `polyfit`, `polyval`, `eye`, `diag`, `trace`, `meshgrid`, `gradient`, `trapz`, and `interp1`. `gradient` handles vectors and rectangular matrices, returning directional gradients for matrices. `root` uses a numerical Newton iteration, while `ode_euler` and `ode_rk4` integrate scalar first-order ODEs. `fft` and `ifft` use `{real, imag}` spectrum objects. `polyfit` returns descending polynomial coefficients and `polyval` evaluates them. `range` accepts floating-point bounds and steps. Text utilities include `str`, `number`,
`lower`, `upper`, `contains`, `split`, `join`, and string-aware `len`; `+`
concatenates strings as well as adding numbers.
`transpose` accepts both matrices and vectors; vector transposition produces a
column matrix.
Loops support `break` and `continue` for early termination and iteration
skipping.
Conditionals support `else if` chains as well as a final `else` branch.
Logical `and`/`or` operators short-circuit their right-hand expressions.
Runtime failures can be handled with `try`/`catch`; `throw` raises a
recoverable value, while native runtime errors are exposed to the catch binding
as error text.
Range syntax is supported with `start:stop` and `start:step:stop`.
The `@` operator performs matrix/vector multiplication; `*` also retains
matrix multiplication for MATLAB-style expressions.
Matrix literals also accept MATLAB row separators, for example `[1 2; 3 4]`.
MATLAB-style `%` comments are accepted when `%` begins a line; `%` remains
the modulo operator inside expressions. Matrix values support scalar indexing
and inclusive row/column slices such as `matrix[0:1, 1:2]`; slices can also be
assigned matrix or scalar values.

```marila
let values = 1:2:5
```

Program files can import reusable source-relative modules:

```marila
import math_utils
print square(5)
```
Imports are normalized to source-relative paths and circular module chains are
reported as runtime errors.
`read_csv(path)` and `write_csv(path, array)` provide CSV interoperability,
preserving numeric and text fields and quoting fields containing commas or
quotes. Paths are resolved relative to the source file.
`read_tsv(path)` and `write_tsv(path, array)` provide tab-separated interchange,
while `read_text(path)` and `write_text(path, string)` handle plain text files.
`read_json(path)` and `write_json(path, value)` support nested arrays, objects,
strings, booleans, numbers, and `none`; object values can be accessed with
string indexing such as `record["name"]`.
JSON string escaping round-trips quotes, backslashes, newlines, tabs, and
other control characters.
JSON imports reject trailing non-whitespace content.
Object fields can also be assigned with indexed assignment before export.
Object literals use `{key: value}` syntax and can be nested with arrays and
other objects. Object utilities include `keys`, `values`, and `has_key`.
Higher-order and collection functions include `map`, `filter`, `reduce`,
`all`, `any`, and `zip`.
Programs can use `assert(condition, "message")` for runtime invariants.
Optional annotations such as `let count: number = 3` validate values at
assignment time; supported types include `number`, `string`, `bool`, `array`,
and `object`.
Function parameters and return values can use the same annotations, for example
`function scale(value: number, factor: number): number`.

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

Graph expressions and domains are evaluated by the native scene renderer using
the same core math vocabulary as programs:

```marila
file type scene:
[
plot2d sin(x) over x in [-pi, pi]
plot2d parametric (cos(t), sin(t)) over t in [0, 6.28]
plot2d polar (1 + cos(t)) over t in [0, 6.28]
plot2d implicit x^2 + y^2 - 1 over x, y in [-2, 2]
plot2d vectorfield (-y, x) over x, y in [-3, 3]
plot3d sin(x) * cos(y) over x, y in [-3, 3]
plot2d data [(-2, 1), (-1, 0), (0, 2), (1, 1)] title "Samples"
plot3d data [[0, 1, 0], [1, 2, 1], [0, 1, 0]] title "Measured"
sphere at (0, 0, 0) radius 2
cube at (3, 0, 0) size 2
]
```

Domain endpoints accept arithmetic expressions involving numbers, `pi`, and
`e`, such as `[-2*pi, 2*pi]` or `[-pi/2, pi/2]`.

Generated scenes preserve geometry, plots, and solids; 3D scenes support
pointer drag rotation for surfaces and solids.
Image sources can also contain 2D and parametric plots; they are sampled into
static SVG paths for export and document embedding. 3D image sources preserve
surface plots as projected wire grids and render spheres and cubes as static
wireframe geometry; interactive scene sources add pointer-driven rotation.
Document sources embed those same static plot paths alongside their prose and
geometry. They also support `bullet1 - "..."` and `code1 - "..."` blocks,
which render as list items and code blocks. `table1 - "| Header | Value |\\n| --- | --- |\\n| A | 1 |"`
renders a structured table in HTML and LaTeX output.

Sheets support cell references, arithmetic, ranges, multi-argument `SUM`,
`AVERAGE`, `COUNT`, `MIN`, `MAX`, `MEDIAN`, and `STDEV`, plus `IF`, `AND`,
`OR`, `NOT`, precision-aware `ROUND`, and mathematical functions such as
`ABS`, `SQRT`, `SIN`, `COS`, `TAN`, `LOG2`, and `POWER`. Quoted cell values are preserved as spreadsheet text, including
escaped quotes. Aggregate functions accept both ranges and scalar arguments,
and `SUMIF`/`COUNTIF` accept comparison criteria over paired ranges.
for example `SUM(A1:C1, 2)`:

```marila
file type sheet:
[
A1 = 12
B1 = 18
C1 = A1 + B1
C2 = SUM(A1:C1)
F1 = COUNT(A1:C1)
G1 = MIN(A1:C1)
H1 = MAX(A1:C1)
chart bar from A1:C1
chart line from A1:C1
chart bar from A1:C1 title "Comparison"
]
```
Chart declarations support `bar`, `line`, and `scatter` types plus optional
titles, which are preserved in SVG output. Scatter charts use the first two
columns of a two-column range as x/y pairs.

TikZ sources can be emitted as standalone compilable LaTeX:

```marila
file type tikz:
[
point A at (0, 0)
point B at (3, 0)
line from A to B
]
```

TikZ output also emits native `plot` commands for 2D, parametric, sampled-data,
and vector-field scene nodes.
When a TikZ source contains a 3D surface or solid, Marila emits a `pgfplots`
axis with `addplot3` commands and preserves plot domains, titles, and colors.
Compiling 3D LaTeX/TikZ output requires a TeX installation with `pgfplots`;
2D TikZ output only requires the standard TikZ package.
LaTeX sources embed the same 2D and 3D plot figures alongside equations,
theorem-style callouts, inline math, and shared geometry.

Animations accept explicit durations:

```marila
square(size 30) morphs to circle(size 10) over 1s
```

Move and morph animations also accept `ease linear`, `ease in`, `ease out`,
or `ease in-out` timing.

Scene and video sources can also animate geometry directly:

```marila
point A at (-3, -1) moves to (3, 1) over 2s
```

Visibility can be animated with `fade in A over 1s` or
`fade out A over 1s`; fade targets refer to named points and use the same
easing modes.

Generated interactive scenes include play/pause, reset, and timeline-scrubbing
controls, while retaining pointer-based 3D rotation.

The implementation is organized around a shared numerical runtime and renderer
AST, so the same values and expressions can feed programs, spreadsheets,
animation timelines, TikZ geometry, LaTeX, static images, and interactive scenes.

The implementation and build configuration live under [`native/`](native/).
