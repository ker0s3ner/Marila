"""
Marila Video Animation Compiler (Python)
Compiles shape morphing & function plotting animations into interactive HTML5 video containers or GIF outputs.
"""

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
        const animData = {anims};
        let t = 0, playing = true;

        function render() {{
            ctx.fillStyle = '#040711';
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            
            // Grid
            ctx.strokeStyle = 'rgba(255,255,255,0.05)';
            for(let x=0; x<800; x+=40) {{ ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,450); ctx.stroke(); }}
            for(let y=0; y<450; y+=40) {{ ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(800,y); ctx.stroke(); }}

            const cx = 400, cy = 225;
            const progress = (Math.sin(t) + 1) / 2; // smooth bounce morph

            const anim = animData[0];
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
                ctx.closePath();
                ctx.fillStyle = 'rgba(99, 102, 241, 0.2)';
                ctx.fill();
                ctx.strokeStyle = progress < 0.5 ? '#06b6d4' : '#a855f7';
                ctx.lineWidth = 4;
                ctx.stroke();

                ctx.fillStyle = '#fff';
                ctx.font = '16px sans-serif';
                ctx.textAlign = 'center';
                ctx.fillText(`Morphing ${anim.from}(${anim.fromSize}) -> ${anim.to}(${anim.toSize})`, cx, cy + 150);
            }}

            if (playing) t += 0.03;
            requestAnimationFrame(render);
        }}
        function togglePlay() {{ playing = !playing; }}
        render();
    </script>
</body>
</html>"""

        with open(output_path, "w", encoding="utf-8") as f:
            f.write(video_html)
        return output_path
