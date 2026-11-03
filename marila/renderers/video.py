"""
Marila Video Animation Compiler (Python)
Compiles shape morphing & function plotting animations into interactive HTML5 video containers or GIF outputs.
"""

import json

from marila.ast_nodes import MorphNode, PlotNode

class VideoCompiler:
    def __init__(self, program):
        self.program = program

    def compile_to_video(self, output_path: str = "animation_output.html") -> str:
        anims = []
        for stmt in self.program.statements:
            if isinstance(stmt, MorphNode):
                from_size = stmt.from_shape.get('args', {}).get('size', 30)
                to_size = stmt.to_shape.get('args', {}).get('size', 10)
                anims.append({
                    'type': 'morph',
                    'from': stmt.from_shape.get('name', 'square'),
                    'fromSize': from_size,
                    'to': stmt.to_shape.get('name', 'circle'),
                    'toSize': to_size,
                    'duration': stmt.duration
                })
            elif isinstance(stmt, PlotNode):
                anims.append({
                    'type': 'plot',
                    'expression': stmt.expression,
                    'animateTrace': stmt.animate_trace,
                    'dimensions': stmt.dimensions,
                    'duration': '4s'
                })

        if not anims:
            anims.append({
                'type': 'morph',
                'from': 'square', 'fromSize': 30,
                'to': 'circle', 'toSize': 10,
                'duration': '3s'
            })

        video_html = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Marila Video Output</title>
    <style>
        body {{ background: #040711; color: #fff; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; font-family: sans-serif; }}
        canvas {{ border: 1px solid rgba(255,255,255,0.1); border-radius: 12px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }}
        .controls {{ margin-top: 15px; display: flex; gap: 10px; }}
        button {{ background: #6366f1; border: none; color: white; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-weight: bold; }}
    </style>
</head>
<body>
    <h2>Marila Motion Graphics Player</h2>
    <canvas id="cvs" width="800" height="450"></canvas>
    <div class="controls">
        <button onclick="togglePlay()">Play / Pause</button>
    </div>

    <script>
        const canvas = document.getElementById('cvs');
        const ctx = canvas.getContext('2d');
        const animData = {json.dumps(anims)};
        let t = 0, playing = true;

        function render() {{
            ctx.fillStyle = '#040711';
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            
            // Grid
            ctx.strokeStyle = 'rgba(255,255,255,0.05)';
            for(let x=0; x<800; x+=40) {{ ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,450); ctx.stroke(); }}
            for(let y=0; y<450; y+=40) {{ ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(800,y); ctx.stroke(); }}

            const cx = 400, cy = 225;
            const durations = animData.map(item => parseFloat(item.duration) || 3);
            const totalDuration = durations.reduce((sum, duration) => sum + duration, 0);
            const time = (t % totalDuration + totalDuration) % totalDuration;
            let elapsed = 0, activeIndex = 0;
            for (let i = 0; i < durations.length; i++) {{
                if (time <= elapsed + durations[i]) {{ activeIndex = i; break; }}
                elapsed += durations[i];
            }}
            const anim = animData[activeIndex];
            const progress = Math.min(1, Math.max(0, (time - elapsed) / durations[activeIndex]));
            if (anim.type === 'morph') {{
                const s1 = anim.fromSize * 4;
                const s2 = anim.toSize * 6;
                const pts = 100;
                ctx.beginPath();
                for (let i=0; i<=pts; i++) {{
                    const a = (i/pts) * Math.PI * 2;
                    const rCircle = s2;
                    const maxC = Math.max(Math.abs(Math.cos(a)), Math.abs(Math.sin(a)));
                    const rSq = s1 / maxC;
                    const r = rSq + (rCircle - rSq) * progress;
                    const px = cx + Math.cos(a) * r;
                    const py = cy + Math.sin(a) * r;
                    if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
                }}

                if (anim.type === 'plot' && anim.dimensions === 2) {{
                    const start = -6, end = 6;
                    const currentEnd = start + (end - start) * progress;
                    ctx.beginPath();
                    ctx.strokeStyle = '#10b981';
                    ctx.lineWidth = 3;
                    let first = true;
                    for (let x = start; x <= currentEnd; x += 0.05) {{
                        const y = evaluateExpression(anim.expression, x);
                        const px = cx + x * 40, py = cy - y * 40;
                        if (first) {{ ctx.moveTo(px, py); first = false; }}
                        else {{ ctx.lineTo(px, py); }}
                    }}
                    ctx.stroke();
                }}

                if (anim.type === 'plot' && anim.dimensions === 3) {{
                    render3DSurface(anim, progress, cx, cy);
                }}

                if (anim.type === 'plot' && anim.dimensions === 4) {{
                    render4DObject(progress, cx, cy);
                }}
                ctx.closePath();
                ctx.fillStyle = 'rgba(99, 102, 241, 0.2)';
                ctx.fill();
                ctx.strokeStyle = progress < 0.5 ? '#06b6d4' : '#a855f7';
                ctx.lineWidth = 4;
                ctx.stroke();

                ctx.fillStyle = '#fff';
                ctx.font = '16px sans-serif';
                ctx.textAlign = 'center';
                ctx.fillText(`Morphing ${{anim.from}}(${{anim.fromSize}}) -> ${{anim.to}}(${{anim.toSize}})`, cx, cy + 150);
            }}

            if (playing) t += 0.03;
            requestAnimationFrame(render);
        }}
        function evaluateExpression(source, x) {{
            const expression = String(source).replace(/\^/g, '**').replace(/sin/gi, 'Math.sin')
                .replace(/cos/gi, 'Math.cos').replace(/tan/gi, 'Math.tan')
                .replace(/sqrt/gi, 'Math.sqrt').replace(/exp/gi, 'Math.exp')
                .replace(/log/gi, 'Math.log').replace(/abs/gi, 'Math.abs');
            try {{
                return Function('x', `return (${{expression}});`)(x);
            }} catch (error) {{
                return NaN;
            }}
            function evaluateSurface(source, x, y) {{
                const expression = String(source).replace(/\^/g, '**').replace(/sin/gi, 'Math.sin')
                    .replace(/cos/gi, 'Math.cos').replace(/sqrt/gi, 'Math.sqrt')
                    .replace(/exp/gi, 'Math.exp').replace(/abs/gi, 'Math.abs');
                try {{
                    return Function('x', 'y', `return (${{expression}});`)(x, y);
                }} catch (error) {{
                    return Math.sin(x) * Math.cos(y);
                }}
            }}
            function project3D(x, y, z, cx, cy) {{
                const yaw = t * 0.35, pitch = -0.45;
                const xr = x * Math.cos(yaw) - z * Math.sin(yaw);
                const zr = x * Math.sin(yaw) + z * Math.cos(yaw);
                const yr = y * Math.cos(pitch) - zr * Math.sin(pitch);
                const depth = 1 + (zr * Math.sin(pitch) + y * Math.cos(pitch)) / 18;
                return [cx + xr * 25 / depth, cy - yr * 25 / depth];
            }}
            function render3DSurface(anim, progress, cx, cy) {{
                const range = 4, step = 0.4;
                for (let x = -range; x < range; x += step) {{
                    for (let y = -range; y < range; y += step) {{
                        const z1 = evaluateSurface(anim.expression || 'sin(x) * cos(y)', x, y);
                        const z2 = evaluateSurface(anim.expression || 'sin(x) * cos(y)', x + step, y);
                        const z3 = evaluateSurface(anim.expression || 'sin(x) * cos(y)', x, y + step);
                        const p1 = project3D(x, z1, y, cx, cy);
                        const p2 = project3D(x + step, z2, y, cx, cy);
                        const p3 = project3D(x, z3, y + step, cx, cy);
                        ctx.strokeStyle = 'rgba(16, 185, 129, 0.7)';
                        ctx.lineWidth = 1;
                        ctx.beginPath(); ctx.moveTo(p1[0], p1[1]); ctx.lineTo(p2[0], p2[1]); ctx.stroke();
                        ctx.beginPath(); ctx.moveTo(p1[0], p1[1]); ctx.lineTo(p3[0], p3[1]); ctx.stroke();
                    }}
                }}
            }}
            function rotate4D(point, a, b) {{
                const p = point.slice();
                [[0, 3, a], [1, 2, b]].forEach(([i, j, angle]) => {{
                    const u = p[i], v = p[j];
                    p[i] = u * Math.cos(angle) - v * Math.sin(angle);
                    p[j] = u * Math.sin(angle) + v * Math.cos(angle);
                }});
                return p;
            }}
            function project4D(point, cx, cy) {{
                const wDepth = 1 / (4.5 - point[3]);
                const x3 = point[0] * wDepth, y3 = point[1] * wDepth, z3 = point[2] * wDepth;
                return project3D(x3, y3, z3, cx, cy);
            }}
            function render4DObject(progress, cx, cy) {{
                const vertices = [];
                for (let i = 0; i < 16; i++) {{
                    vertices.push([i & 1 ? 1 : -1, i & 2 ? 1 : -1, i & 4 ? 1 : -1, i & 8 ? 1 : -1]);
                }}
                const projected = vertices.map(point => project4D(rotate4D(point, t * 0.5, t * 0.3), cx, cy));
                ctx.strokeStyle = '#a855f7'; ctx.lineWidth = 2;
                for (let i = 0; i < 16; i++) {{
                    for (let bit = 1; bit <= 8; bit <<= 1) {{
                        const j = i ^ bit;
                        if (i < j) {{
                            ctx.beginPath(); ctx.moveTo(projected[i][0], projected[i][1]);
                            ctx.lineTo(projected[j][0], projected[j][1]); ctx.stroke();
                        }}
                    }}
                }}
            }}
        }}
        function togglePlay() {{ playing = !playing; }}
        render();
    </script>
</body>
</html>"""

        with open(output_path, "w", encoding="utf-8") as f:
            f.write(video_html)
        return output_path
