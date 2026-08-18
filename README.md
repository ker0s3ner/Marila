# Marila Programming Language

**Marila** is a simple domain-specific programming language built on top of Python for mathematical documents (LaTeX-style), motion graphics/animations (Manim-style), and Euclidean vector diagrams (TikZ-style).

---

## 🌟 Language Syntax Overview

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

## 🚀 Usage CLI Commands

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
