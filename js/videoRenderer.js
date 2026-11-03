/**
 * Marila Video Renderer (Manim-style 2D Motion Graphics Engine)
 * Features shape morphing (square to circle), graph plotting, and timeline playback.
 */

class VideoRenderer {
  constructor(canvasEl, scrubberEl, timeDisplayEl, playPauseBtn, loopBtn) {
    this.canvas = canvasEl;
    this.ctx = canvasEl.getContext('2d');
    this.scrubber = scrubberEl;
    this.timeDisplay = timeDisplayEl;
    this.playPauseBtn = playPauseBtn;
    this.loopBtn = loopBtn;

    this.isPlaying = false;
    this.isLooping = true;
    this.currentTime = 0; // seconds
    this.totalDuration = 4; // seconds
    this.lastTimestamp = null;
    this.animId = null;

    this.evalData = null;

    this.initControls();
  }

  initControls() {
    if (this.playPauseBtn) {
      this.playPauseBtn.addEventListener('click', () => this.togglePlay());
    }
    if (this.loopBtn) {
      this.loopBtn.addEventListener('click', () => {
        this.isLooping = !this.isLooping;
        this.loopBtn.classList.toggle('active', this.isLooping);
      });
    }
    if (this.scrubber) {
      this.scrubber.addEventListener('input', (e) => {
        const percent = parseFloat(e.target.value);
        this.currentTime = (percent / 100) * this.totalDuration;
        this.renderFrame();
      });
    }
  }

  load(evalData) {
    this.evalData = evalData;
    this.currentTime = 0;
    this.totalDuration = 4; // default

    if (evalData && evalData.animations && evalData.animations.length > 0) {
      this.totalDuration = evalData.animations.reduce((sum, anim) => sum + (anim.duration || 3), 0);
    }

    this.updateTimelineUI();
    this.play();
  }

  play() {
    this.isPlaying = true;
    this.lastTimestamp = performance.now();
    if (this.playPauseBtn) {
      this.playPauseBtn.innerHTML = '<i data-lucide="pause"></i>';
      if (window.lucide) window.lucide.createIcons();
    }
    this.tick();
  }

  pause() {
    this.isPlaying = false;
    if (this.animId) cancelAnimationFrame(this.animId);
    if (this.playPauseBtn) {
      this.playPauseBtn.innerHTML = '<i data-lucide="play"></i>';
      if (window.lucide) window.lucide.createIcons();
    }
  }

  togglePlay() {
    if (this.isPlaying) this.pause();
    else {
      if (this.currentTime >= this.totalDuration) this.currentTime = 0;
      this.play();
    }
  }

  tick() {
    if (!this.isPlaying) return;

    const now = performance.now();
    const dt = (now - (this.lastTimestamp || now)) / 1000;
    this.lastTimestamp = now;

    this.currentTime += dt;
    if (this.currentTime >= this.totalDuration) {
      if (this.isLooping) {
        this.currentTime = 0;
      } else {
        this.currentTime = this.totalDuration;
        this.pause();
      }
    }

    this.renderFrame();
    this.updateTimelineUI();

    if (this.isPlaying) {
      this.animId = requestAnimationFrame(() => this.tick());
    }
  }

  updateTimelineUI() {
    if (this.scrubber) {
      const pct = (this.currentTime / this.totalDuration) * 100;
      this.scrubber.value = pct;
    }
    if (this.timeDisplay) {
      const curSec = this.currentTime.toFixed(1);
      const totSec = this.totalDuration.toFixed(1);
      this.timeDisplay.textContent = `00:0${Math.floor(curSec)} / 00:0${Math.floor(totSec)}`;
    }
  }

  renderFrame() {
    const width = this.canvas.width;
    const height = this.canvas.height;
    const ctx = this.ctx;

    // Clear background
    ctx.fillStyle = '#040711';
    ctx.fillRect(0, 0, width, height);

    // Draw coordinate grid lines
    this.drawGrid(ctx, width, height);

    if (!this.evalData || !this.evalData.animations) return;

    // Determine current active animation
    let elapsed = 0;
    for (const anim of this.evalData.animations) {
      const dur = anim.duration || 3;
      if (this.currentTime >= elapsed && this.currentTime <= elapsed + dur) {
        const animProgress = (this.currentTime - elapsed) / dur;
        this.renderAnimation(ctx, anim, animProgress, width, height);
        break;
      }
      elapsed += dur;
    }
  }

  drawGrid(ctx, w, h) {
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.lineWidth = 1;
    const gridSize = 40;
    for (let x = 0; x < w; x += gridSize) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }
    for (let y = 0; y < h; y += gridSize) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }

    // Axes
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.2)';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(w / 2, 0); ctx.lineTo(w / 2, h); // Y axis
    ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); // X axis
    ctx.stroke();
  }

  renderAnimation(ctx, anim, progress, w, h) {
    const cx = w / 2;
    const cy = h / 2;

    if (anim.type === 'morph') {
      // Easing curve
      const t = this.easeInOutCubic(progress);

      // Morph square to circle
      const numPoints = 120;
      const size1 = parseFloat(anim.from.args.size || 60) * 2;
      const size2 = parseFloat(anim.to.args.size || 50) * 2;

      ctx.beginPath();
      for (let i = 0; i <= numPoints; i++) {
        const angle = (i / numPoints) * Math.PI * 2;

        // Circle vertex at angle
        const circleR = size2;
        const circleX = cx + Math.cos(angle) * circleR;
        const circleY = cy + Math.sin(angle) * circleR;

        // Square vertex at angle
        const sqHalf = size1;
        const cosA = Math.cos(angle);
        const sinA = Math.sin(angle);
        const maxC = Math.max(Math.abs(cosA), Math.abs(sinA));
        const sqR = sqHalf / maxC;
        const sqX = cx + cosA * sqR;
        const sqY = cy + sinA * sqR;

        // Interpolated vertex
        const currX = sqX + (circleX - sqX) * t;
        const currY = sqY + (circleY - sqY) * t;

        if (i === 0) ctx.moveTo(currX, currY);
        else ctx.lineTo(currX, currY);
      }
      ctx.closePath();

      // Gradient Fill & Glow
      const fillGrad = ctx.createRadialGradient(cx, cy, 10, cx, cy, 150);
      fillGrad.addColorStop(0, `rgba(6, 182, 212, ${0.4 - t * 0.1})`);
      fillGrad.addColorStop(1, `rgba(168, 85, 247, ${0.1 + t * 0.3})`);

      ctx.fillStyle = fillGrad;
      ctx.fill();

      ctx.strokeStyle = t < 0.5 ? '#06b6d4' : '#a855f7';
      ctx.lineWidth = 4;
      ctx.shadowColor = ctx.strokeStyle;
      ctx.shadowBlur = 15;
      ctx.stroke();
      ctx.shadowBlur = 0; // reset

      // Label Overlay
      ctx.fillStyle = '#f8fafc';
      ctx.font = '600 16px Inter, sans-serif';
      ctx.textAlign = 'center';
      const labelText = t < 0.5 ? 'Square' : 'Circle';
      ctx.fillText(`Shape: ${labelText} (${(t * 100).toFixed(0)}% Morph)`, cx, cy + 140);
    } else if (anim.type === 'plot' && (anim.dimensions || 2) === 2) {
      // Function Plot Animation
      const t = progress;
      ctx.beginPath();
      ctx.strokeStyle = '#10b981';
      ctx.lineWidth = 3;

      const xMin = -6, xMax = 6;
      const step = 0.05;
      let first = true;

      const currentMaxX = xMin + (xMax - xMin) * t;

      for (let x = xMin; x <= currentMaxX; x += step) {
        let y = Math.sin(x);
        if (anim.expression.includes('x^2')) y = (x * x) / 10 - 2;

        const screenX = cx + x * 40;
        const screenY = cy - y * 40;

        if (first) {
          ctx.moveTo(screenX, screenY);
          first = false;
        } else {
          ctx.lineTo(screenX, screenY);
        }
      }
      ctx.stroke();

      // Moving Tracer Point
      const tracerX = currentMaxX;
      let tracerY = Math.sin(tracerX);
      if (anim.expression.includes('x^2')) tracerY = (tracerX * tracerX) / 10 - 2;

      const pX = cx + tracerX * 40;
      const pY = cy - tracerY * 40;

      ctx.beginPath();
      ctx.arc(pX, pY, 8, 0, Math.PI * 2);
      ctx.fillStyle = '#6366f1';
      ctx.shadowColor = '#6366f1';
      ctx.shadowBlur = 20;
      ctx.fill();
      ctx.shadowBlur = 0;
    } else if (anim.type === 'plot' && anim.dimensions === 3) {
      this.render3DSurface(ctx, anim, progress, cx, cy);
    } else if (anim.type === 'plot' && anim.dimensions === 4) {
      this.render4DHypercube(ctx, progress, cx, cy);
    }
  }

  evaluateSurface(expression, x, y) {
    if (expression.includes('x^2') && !expression.includes('y')) return (x * x) / 10 - 2;
    if (expression.includes('sin') && expression.includes('cos')) return Math.sin(x) * Math.cos(y);
    return Math.sin(x) * Math.cos(y);
  }

  project3D(x, y, z, cx, cy) {
    const yaw = performance.now() / 4000;
    const xr = x * Math.cos(yaw) - z * Math.sin(yaw);
    const zr = x * Math.sin(yaw) + z * Math.cos(yaw);
    const depth = 1 + zr / 18;
    return [cx + xr * 25 / depth, cy - y * 25 / depth];
  }

  render3DSurface(ctx, anim, progress, cx, cy) {
    const range = 4;
    const step = 0.4;
    for (let x = -range; x < range; x += step) {
      for (let y = -range; y < range; y += step) {
        const p1 = this.project3D(x, this.evaluateSurface(anim.expression, x, y), y, cx, cy);
        const p2 = this.project3D(x + step, this.evaluateSurface(anim.expression, x + step, y), y, cx, cy);
        const p3 = this.project3D(x, this.evaluateSurface(anim.expression, x, y + step), y + step, cx, cy);
        ctx.strokeStyle = 'rgba(16, 185, 129, 0.7)';
        ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(...p1); ctx.lineTo(...p2); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(...p1); ctx.lineTo(...p3); ctx.stroke();
      }
    }
  }

  render4DHypercube(ctx, progress, cx, cy) {
    const vertices = Array.from({ length: 16 }, (_, index) => [
      index & 1 ? 1 : -1, index & 2 ? 1 : -1,
      index & 4 ? 1 : -1, index & 8 ? 1 : -1
    ]);
    const projected = vertices.map(point => {
      const angle = performance.now() / 3000;
      const x = point[0] * Math.cos(angle) - point[3] * Math.sin(angle);
      const w = point[0] * Math.sin(angle) + point[3] * Math.cos(angle);
      const depth = 1 / (4.5 - w);
      return this.project3D(x * depth, point[1] * depth, point[2] * depth, cx, cy);
    });
    ctx.strokeStyle = '#a855f7';
    ctx.lineWidth = 2;
    vertices.forEach((_, index) => {
      [1, 2, 4, 8].forEach(bit => {
        const other = index ^ bit;
        if (index < other) {
          ctx.beginPath(); ctx.moveTo(...projected[index]);
          ctx.lineTo(...projected[other]); ctx.stroke();
        }
      });
    });
  }

  easeInOutCubic(t) {
    return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
  }
}
