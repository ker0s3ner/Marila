# Marila

Marila is a hybrid mathematical programming language implemented entirely in
portable C++17. The native executable provides the language lexer, parser,
runtime, CLI, and document, geometry, and scene renderers.

## What Marila replaces today

Marila is an early, useful integration layer for numerical programming,
visualization, spreadsheets, and mathematical documents. A single `.marila`
project can calculate with arrays, solve common numerical problems, plot in
2D/3D/4D, build sheets, and export documents to HTML, SVG, or LaTeX/TikZ. This
can replace parts of a workflow that would otherwise move between a scripting
language, MATLAB-style numerical code, a plotting tool, a spreadsheet, and a
LaTeX document. It does not yet replace those mature tools for every workload.

Marila is **not a Lean replacement**. Its `number`, `vector`, `matrix`, `array`, and related type
annotations are checked at runtime; it has no dependent type system, proof
terms, theorem prover, tactic framework, or kernel that checks mathematical
proofs. An `assert` checks a program condition on an execution, not a theorem
for all inputs. Numerical differentiation, integration, roots, and ODE solvers
are approximations and do not establish exact results.

Marila is also **not yet a full LaTeX replacement**. Its `latex` and `tikz`
file types provide a convenient subset for notes, equations, callouts, tables,
plots, and geometry, with TeX output where needed. It does not implement the
full LaTeX macro/package ecosystem or arbitrary TeX compatibility. Compiling
the generated `.tex` still requires a TeX distribution.

The practical target is to make Marila a strong unified environment for
scientific work first, then grow a separate formal mathematics layer with
explicit proof checking. Until that layer exists, treat plotted and numerical
results as computational outputs rather than machine-verified proofs.

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
./build/native/marila-native tokens examples/higher_order_demo.marila
./build/native/marila-native render examples/document_demo.marila output.html
./build/native/marila-native export examples/static_3d_demo.marila obj surface.obj
./build/native/marila-native export examples/static_3d_demo.marila gltf surface.gltf
./build/native/marila-native export examples/sheet_demo.marila csv sheet.csv
./build/native/marila-native export examples/hybrid_program.marila json program.json
./build/native/marila-native format examples/graphs_demo.marila
./build/native/marila-native lint examples/video_demo.marila
./build/native/marila-native diagnose examples/hybrid_program.marila
./build/native/marila-native deps examples/modules_demo.marila
./build/native/marila-native deps examples/modules_demo.marila --dot
./build/native/marila-native repl
./build/native/marila-native --version
```

`run` executes `file type program` sources. For document, latex, sheet, image,
video, and scene sources it writes the corresponding native-rendered output. `check`
validates program syntax and recognizes renderer source blocks. `diagnose`
emits structured JSON diagnostics for editor and CI integrations. `ast` emits a
machine-readable JSON syntax tree for program files and a shared visual AST for
renderer sources, suitable for editor integrations and tooling.
`deps` emits a resolved JSON import graph and reports missing or circular module
dependencies without executing the program.
`format` prints normalized indentation; append `--check` for a CI-friendly
validation or `--write` to update the source in place.
For image, video, and scene sources, `check`, `lint`, `format`, `render`, and
`diagnose` also validate visual statements and point references. Diagnostics
include source line and column locations for unknown statements and unresolved
geometry references.

`export` supports machine-readable JSON AST and sheet-model output, CSV/TSV sheet interchange, SVG, PNG, HTML, Markdown, TeX/LaTeX, TikZ, and PDF publication output, Wavefront OBJ,
ASCII STL, ASCII PLY, and self-contained glTF 2.0 for image, scene, and video sources, plus MP4,
GIF, and WebM animation export when FFmpeg is installed. OBJ exports named 2D
geometry, analytic and sampled plots, implicit contours, vector fields, projected
4D curves, spheres, cubes, and sampled 3D surfaces
as mesh or line geometry. Video export rasterizes the shared animation timeline
and encodes the frames with FFmpeg.

## Language core

The native runtime supports numbers, strings, booleans, `none`, lists,
arithmetic and comparison operators, logical operators, indexing, user
functions, lambdas, conditionals, loops, imports, indexed assignment, and built-ins including
`sin`, `cos`, `tan`, `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`, `asin`, `acos`, `atan`, `atan2`, `sqrt`, `floor`, `ceil`, `exp`,
`log`, `log2`, `log10`, `pow`, `sign`, `hypot`, `abs`, `len`, `range`, `sum`,
`isfinite`, `isnan`, `isinf`,
`isclose`, and `clamp`,
`dot`, `outer`, `transpose`, `linspace`, `zeros`, `ones`, `mean`, `median`, `variance`,
`vstack`, and `hstack` compose vectors and rectangular matrices by rows or columns.
`std`, `quantile`, `cov`, `corr`, `sort`, `cumsum`, `cumprod`, `diff`, `min`, `max`, `find`, `argmin`, and `argmax`,
`matmul`, `shape`, `reshape`, `det`, `inverse`, `solve`, `rank`, `pinv`, `lstsq`, and `kron`, plus the constants `pi`
and `e`, as well as `eigvals`, `eig`, `norm`, `normalize`, `cross`, `differentiate`,
where `eigvals` and `eig` expose symmetric-matrix spectral decompositions as
values and eigenvector matrices, and `svd` returns `U`, singular values `S`,
and right-singular vectors `V` for rectangular matrices. `qr` returns an
orthonormal `Q` and upper-triangular `R` for full-column-rank matrices, and
`cholesky` returns the lower-triangular factor of a positive-definite matrix.
`norm` accepts vectors and matrices, with matrix orders 1 and 2 available.
`diag` accepts an optional diagonal offset for extraction and construction.
`solve` accepts either a vector or a matrix of right-hand sides.
`zeros(n)` and `ones(n)` create square matrices; passing two dimensions creates
rectangular matrices.
`eigvals` also handles general 2×2 real matrices, returning complex conjugate
pairs when necessary; larger `eig`/`eigvals` inputs use the symmetric path.
`integrate`, `root`, `ode_euler`, `ode_rk4`, `fft`, `ifft`, `polyfit`, `polyval`, `eye`, `diag`, `trace`, `meshgrid`, `gradient`, `trapz`, and `interp1`. `gradient` handles vectors and rectangular matrices, returning directional gradients for matrices. `root` uses a numerical Newton iteration, while `ode_euler` and `ode_rk4` integrate scalar first-order ODEs. `fft` and `ifft` use `{real, imag}` spectrum objects. `polyfit` returns descending polynomial coefficients and `polyval` evaluates them. Complex values are available with `complex(real, imag)` and support arithmetic, `real`, `imag`, `conj`, `arg`, complex-aware trigonometric/exponential/logarithm functions, and magnitude via `abs`. They display as `a + bi` and serialize to JSON as `{\"real\":a,\"imag\":b}`. `range` accepts floating-point bounds and steps. Text utilities include `str`, `number`,
`convolve` performs one-dimensional discrete convolution with `full`, `same`, and `valid` modes.
`cumtrapz` returns cumulative trapezoidal integrals using uniform spacing or explicit coordinates.
`root` supports Newton iteration from a scalar guess or bracketed bisection from `[lower, upper]`.
`fftshift` and `ifftshift` center and uncenter Fourier spectra, including odd-length vectors.
`polyval` evaluates a polynomial at either a scalar or a vector of inputs.
`ode_euler` and `ode_rk4` also accept vector-valued states and vector-valued derivatives.
`histogram` returns an object containing `counts` and `edges` for numeric data.
`interp1` accepts scalar or vector queries and supports `linear`, `nearest`, `previous`, and `next` modes. `interp2` performs bilinear interpolation over a rectangular grid, accepts scalar or vector queries with broadcasting, and clamps queries to the sample bounds.
`logspace` generates logarithmically spaced powers of ten for plotting and numerical sampling.
`jacobian` finite-differentiates a vector-valued function over a point vector.
`expm` computes the exponential of a real square matrix using a stable scaling-and-squaring series.
`rand` and `randn` generate reproducible scalar or matrix samples after `seed`.
Unary math functions such as `sin`, `cos`, `tan`, `sqrt`, `exp`, `log`, `abs`,
`floor`, and `ceil` map recursively over vectors and matrices. Binary numeric
functions including `pow`, `atan2`, `hypot`, and `complex` broadcast scalar
operands over arrays and require equal array shapes when both operands are
arrays. `real`, `imag`, `conj`, and `arg` are vectorized over complex arrays.
`lower`, `upper`, `trim`, `replace`, `starts_with`, `ends_with`, `contains`, `split`, `join`, `type_of`/`typeof`, and string-aware `len`; `+`
concatenates strings as well as adding numbers. Assignments also support `+=`,
`-=`, `*=`, `/=`, and `%=` for named values; prefix and postfix `++`/`--`
are available for named numeric values.
`transpose` accepts both matrices and vectors; vector transposition produces a
column matrix.
Loops support `break` and `continue` for early termination and iteration
skipping. `for item in values` iterates arrays directly; `for index, item in
values` also exposes zero-based indices, while `for key, value in record`
iterates object entries in key order. Strings can also be iterated as
single-character strings.
Conditionals support `else if` chains as well as a final `else` branch.
Expressions also support `condition ? when_true : when_false` selection.
Logical `and`/`or` operators short-circuit their right-hand expressions.
Runtime failures can be handled with `try`/`catch`; `throw` raises a
recoverable value, while native runtime errors are exposed to the catch binding
as error text. An optional `finally` block always runs before control leaves the
construct, including when the body returns, breaks, continues, throws, or
raises an uncaught runtime error.
Range syntax is supported with `start:stop` and `start:step:stop`.
The `@` operator performs matrix/vector multiplication; `*` also retains
matrix multiplication for MATLAB-style expressions.
Matrix literals also accept MATLAB row separators, for example `[1 2; 3 4]`.
MATLAB-style `%` comments are accepted when `%` begins a line; `%` remains
the modulo operator inside expressions. `#`, `//`, and `/* ... */` comments
are also supported. Matrix values support scalar indexing
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
Use `import math_utils as math` with `math.square(5)` or `math["scale"]` to
keep a module's public bindings under a namespace and avoid name collisions.
`read_csv(path)` and `write_csv(path, array)` provide CSV interoperability,
preserving numeric and text fields, escaped quotes, and multiline quoted fields.
Paths are resolved relative to the source file.
`read_tsv(path)` and `write_tsv(path, array)` provide tab-separated interchange,
while `read_text(path)` and `write_text(path, string)` handle plain text files.
`read_json(path)` and `write_json(path, value)` support nested arrays, objects,
strings, booleans, numbers, and `none`; object values can be accessed with
string indexing such as `record["name"]`.
`read_jsonl(path)` and `write_jsonl(path, array)` exchange one JSON value per
line for streaming-friendly datasets.
JSON string escaping round-trips quotes, backslashes, newlines, tabs, Unicode
escapes, and other control characters.
JSON imports reject trailing non-whitespace content.
Object fields can also be assigned with indexed assignment before export.
Object literals use `{key: value}` syntax and can be nested with arrays and
other objects. Object utilities include `keys`, `values`, and `has_key`.
Higher-order and collection functions include `map`, `filter`, `reduce`,
`all`, `any`, and `zip`.
Programs can use `assert(condition, "message")` for runtime invariants.
Optional annotations such as `let count: number = 3` validate values at
assignment time; supported types include `number`, `string`, `bool`, `vector`,
`matrix`, `array`, and `object`. `vector` requires a numeric one-dimensional
array; `matrix` requires a non-empty rectangular numeric array.
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
plot2d sin(x) over x in [-pi, pi] y in [-1.5, 1.5]
plot2d parametric (cos(t), sin(t)) over t in [0, 6.28]
plot2d polar (1 + cos(t)) over t in [0, 6.28]
plot2d implicit x^2 + y^2 - 1 over x, y in [-2, 2]
plot2d vectorfield (-y, x) over x, y in [-3, 3]
plot3d sin(x) * cos(y) over x, y in [-3, 3]
plot4d parametric (cos(t), sin(t), cos(2*t), sin(2*t)) over t in [0, 2*pi] title "4D Lissajous curve"
plot2d data [(-2, 1), (-1, 0), (0, 2), (1, 1)] title "Samples"
plot3d data [[0, 1, 0], [1, 2, 1], [0, 1, 0]] title "Measured"
sphere at (0, 0, 0) radius 2
cube at (3, 0, 0) size 2
]
```

Domain endpoints accept arithmetic expressions involving numbers, `pi`, and
`e`, such as `[-2*pi, 2*pi]` or `[-pi/2, pi/2]`.

Generated scenes preserve geometry, plots, and solids; 3D scenes support
pointer drag rotation for surfaces and solids. 4D parametric curves accept
four coordinate functions of `t`; the interactive renderer projects them into
3D with perspective along the fourth coordinate, and dragging rotates the
projection. A 4D plot is necessarily a projection into the display; it is not
a direct view of four-dimensional space.
2D function and parametric plots can set an explicit vertical viewing range
with `y in [minimum, maximum]` after the x or t domain. Image sources can also contain 2D and parametric plots; they are sampled into
static SVG paths for export and document embedding. 3D image sources preserve
surface plots as projected wire grids and render spheres and cubes as static
wireframe geometry; interactive scene sources add pointer-driven rotation.
Document sources embed those same static plot paths alongside their prose and
geometry. They also support `bullet1 - "..."` and `code1 - "..."` blocks,
which render as list items and code blocks. `table1 - "| Header | Value |\\n| --- | --- |\\n| A | 1 |"`
renders a structured table in HTML and LaTeX output. `section1`, `subsection1`,
`quote1`, and `link1 - "Label" :: "https://..."` preserve headings,
quotations, and hyperlinks in both document targets.

Sheets support cell references, arithmetic, ranges, multi-argument `SUM`,
`AVERAGE`, `COUNT`, `MIN`, `MAX`, `MEDIAN`, and `STDEV`, plus `IF`, `AND`,
`OR`, `NOT`, precision-aware `ROUND`, and mathematical functions such as
`ABS`, `SQRT`, `SIN`, `COS`, `TAN`, `LOG2`, `POWER`, `MOD`, and `IFERROR`.
`PRODUCT` and `SUMPRODUCT` support common modeling and weighted calculations;
`SUMIF`, `COUNTIF`, and `AVERAGEIF` also accept quoted text or comparison criteria.
`SUMIFS`, `COUNTIFS`, and `AVERAGEIFS` accept multiple paired criteria ranges.
`MINIFS` and `MAXIFS` select extrema from a result range under multiple criteria.
`INDEX` retrieves a numeric value by one-based row and column, while `MATCH`
supports exact (`0`) and sorted approximate (`1` or `-1`) numeric lookups.
`VLOOKUP` searches the first column of a range and returns a selected column,
supporting exact mode by default or sorted approximate mode with `1`.
`HLOOKUP` provides the corresponding first-row search and selected-row return.
`ISBLANK`, `ISNUMBER`, and `ISTEXT` inspect cell contents without coercing text.
`IF`, `IFERROR`, and conditional criteria arguments support nested formulas and quoted commas.
Quoted cell values are preserved as spreadsheet text, including
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
or `ease in-out` timing. Use `at 1s` to place an animation on the shared
timeline and `wait 0.5s at 3s` to reserve a timeline interval.

Scene and video sources can also animate geometry directly:

```marila
point A at (-3, -1) moves to (3, 1) over 2s
```

Visibility can be animated with `fade in A over 1s` or
`fade out A over 1s`; fade targets refer to named points and use the same
easing modes.
Move, fade, and morph segments can repeat with `repeat N`; adding `yoyo`
reverses every other cycle, so `repeat 2 yoyo` returns to the starting state.

Generated interactive scenes include play/pause, reset, and timeline-scrubbing
controls, while retaining pointer-based 3D rotation.

The implementation is organized around a shared numerical runtime and renderer
AST, so the same values and expressions can feed programs, spreadsheets,
animation timelines, TikZ geometry, LaTeX, static images, and interactive scenes.

The implementation and build configuration live under [`native/`](native/).
