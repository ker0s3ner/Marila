/**
 * Marila Example Code Presets
 */

const MARILA_EXAMPLES = {
  video_morph: `file type video:
[
  square(size 30) morphs to circle(size 10) over 3s
]
render video`,

  video_calculus: `file type video:
[
  plot sin(x) & animate trace
]
render video`,

  doc_euclid: `file type document:
[
  p1 - "What is Euclidean Geometry?" :: By Benas Petronis

  p2 - "Euclidean geometry is a mathematical system attributed to ancient Greek mathematician Euclid of Alexandria. We define geometric relationships and formulas cleanly."

  theorem1 - "Pythagorean Theorem" :: "In any right-angled triangle, the square of the hypotenuse is equal to the sum of the squares of the other two sides: <a^2 + b^2 = c^2>."

  p3 - "We define integration over space as <integral |0||1|| x^2 dx> which measures area under parabolic curves."
]
render document`,

  doc_calculus: `file type document:
[
  p1 - "Calculus & Infinite Series" :: Marila Formal Math Proof

  p2 - "Consider the infinite summation <sum |k=1||n|| k = (n(n+1))/2> representing triangular numbers."

  p3 - "Evaluating trigonometric limits yields <integral |a||b|| sin(x) dx = -cos(b) + cos(a)>."
]
render document`,

  image_triangle: `file type image:
[
  point A at (0, 0) label "Origin"
  point B at (120, 0) label "Base"
  point C at (120, 90) label "Apex"
  line from A to B
  line from B to C
  line from C to A
  triangle ABC with color "amber" & show angles
]
render image`,

  image_circle_tangent: `file type image:
[
  point O at (0, 0) label "Center"
  point P at (100, 75) label "Point P"
  circle at O radius 80 & dash
  line from O to P
  point T at (80, 0) label "Tangent T"
]
render image`
};

if (typeof module !== 'undefined') {
  module.exports = { MARILA_EXAMPLES };
}
