import React, { useRef, useEffect } from 'react';
import { Layer, Clip, SensorPoint } from '../types';

interface LiveCanvasProps {
  layers: Layer[];
  clips: Record<number, Record<number, Clip>>;
  activeTouchPoints?: SensorPoint[];
  showTestGrid?: boolean;
  className?: string;
  interactive?: boolean;
  onCanvasClick?: (x: number, y: number) => void;
}

export const LiveCanvas: React.FC<LiveCanvasProps> = ({
  layers,
  clips,
  activeTouchPoints = [],
  showTestGrid = false,
  className = '',
  interactive = false,
  onCanvasClick
}) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const animFrameRef = useRef<number | null>(null);
  const timeRef = useRef<number>(0);
  const ripplesRef = useRef<Array<{ x: number; y: number; r: number; alpha: number; color: string }>>([]);

  // Trigger ripple when touch points change
  useEffect(() => {
    if (activeTouchPoints.length > 0) {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const w = canvas.width;
      const h = canvas.height;

      activeTouchPoints.forEach((pt) => {
        if (pt.state === 'down') {
          // Normalize to canvas coordinates
          const cx = pt.x <= 1.0 ? pt.x * w : (pt.x / 1920) * w;
          const cy = pt.y <= 1.0 ? pt.y * h : (pt.y / 1080) * h;
          ripplesRef.current.push({
            x: cx,
            y: cy,
            r: 5,
            alpha: 1.0,
            color: '#FF7F50'
          });
        }
      });
    }
  }, [activeTouchPoints]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let isRunning = true;

    const render = () => {
      if (!isRunning) return;
      timeRef.current += 0.02;
      const t = timeRef.current;
      const width = canvas.width;
      const height = canvas.height;

      // Base background
      ctx.fillStyle = '#050505';
      ctx.fillRect(0, 0, width, height);

      // Render layers from bottom (Layer 1) to top (Layer 3)
      const sortedLayers = [...layers].sort((a, b) => a.id - b.id);

      sortedLayers.forEach((layer) => {
        if (layer.bypass || layer.opacity <= 0) return;

        const activeCol = layer.activeCol;
        const activeClip = activeCol ? clips[layer.id]?.[activeCol] : null;

        // If active slot is empty (no loaded clip), clear output for this layer
        if (!activeClip || !activeClip.loaded) {
          return;
        }

        ctx.save();
        ctx.globalAlpha = layer.opacity;

        // Composite mode
        if (layer.blendMode === 'ADDITIVE') {
          ctx.globalCompositeOperation = 'lighter';
        } else if (layer.blendMode === 'SCREEN') {
          ctx.globalCompositeOperation = 'screen';
        } else if (layer.blendMode === 'MULTIPLY') {
          ctx.globalCompositeOperation = 'multiply';
        } else if (layer.blendMode === 'DIFFERENCE') {
          ctx.globalCompositeOperation = 'difference';
        } else {
          ctx.globalCompositeOperation = 'source-over';
        }

        // Clip transform
        const scale = activeClip ? activeClip.scale : 1.0;
        const rot = activeClip ? (activeClip.rotation * Math.PI) / 180 : 0;
        const cx = width / 2 + (activeClip ? activeClip.posX : 0);
        const cy = height / 2 + (activeClip ? activeClip.posY : 0);

        ctx.translate(cx, cy);
        ctx.rotate(rot);
        ctx.scale(scale, scale);

        // Draw animated visuals based on layer
        if (layer.id === 1) {
          // Layer 1: Ambient background grid & slow plasma glow
          const grad = ctx.createRadialGradient(0, 0, 10, 0, 0, width * 0.7);
          grad.addColorStop(0, 'rgba(255, 127, 80, 0.35)');
          grad.addColorStop(0.5, 'rgba(17, 138, 178, 0.2)');
          grad.addColorStop(1, 'rgba(0, 0, 0, 0)');
          ctx.fillStyle = grad;
          ctx.fillRect(-width / 2, -height / 2, width, height);

          // Subtle animated grid lines
          ctx.strokeStyle = 'rgba(255, 255, 255, 0.04)';
          ctx.lineWidth = 1;
          const gridSize = 40;
          for (let x = -width / 2; x <= width / 2; x += gridSize) {
            ctx.beginPath();
            ctx.moveTo(x, -height / 2);
            ctx.lineTo(x, height / 2);
            ctx.stroke();
          }
          for (let y = -height / 2; y <= height / 2; y += gridSize) {
            ctx.beginPath();
            ctx.moveTo(-width / 2, y);
            ctx.lineTo(width / 2, y);
            ctx.stroke();
          }
        } else if (layer.id === 2) {
          // Layer 2: Rotating geometric polygons / wireframe
          const polyCount = 5;
          ctx.strokeStyle = activeClip?.color || '#118AB2';
          ctx.lineWidth = 2;

          for (let i = 1; i <= polyCount; i++) {
            const rad = 40 * i + Math.sin(t * 1.5 + i) * 15;
            const sides = 6;
            ctx.beginPath();
            for (let s = 0; s < sides; s++) {
              const angle = (s * 2 * Math.PI) / sides + (t * (i % 2 === 0 ? 0.3 : -0.3));
              const px = Math.cos(angle) * rad;
              const py = Math.sin(angle) * rad;
              if (s === 0) ctx.moveTo(px, py);
              else ctx.lineTo(px, py);
            }
            ctx.closePath();
            ctx.stroke();
          }
        } else if (layer.id === 3) {
          // Layer 3: High energy particle vortex / audio reactive sine waves
          ctx.strokeStyle = activeClip?.color || '#FF7F50';
          ctx.lineWidth = 2.5;

          // Flowing audio wave lines
          for (let wave = 0; wave < 3; wave++) {
            ctx.beginPath();
            const wOffset = wave * 0.8;
            ctx.strokeStyle = wave === 0 ? '#FF7F50' : wave === 1 ? '#FFD166' : '#06D6A0';
            for (let x = -width / 2; x <= width / 2; x += 10) {
              const freq = 0.008;
              const amp = 40 + wave * 15;
              const y = Math.sin(x * freq + t * 2 + wOffset) * Math.cos(x * 0.003 + t) * amp;
              if (x === -width / 2) ctx.moveTo(x, y);
              else ctx.lineTo(x, y);
            }
            ctx.stroke();
          }

          // Floating energy sparks
          ctx.fillStyle = '#FFD166';
          for (let p = 0; p < 24; p++) {
            const angle = p * (Math.PI / 12) + t * 0.5;
            const dist = 120 + Math.sin(t * 2 + p) * 60;
            const px = Math.cos(angle) * dist;
            const py = Math.sin(angle) * dist;
            ctx.beginPath();
            ctx.arc(px, py, 2.5, 0, Math.PI * 2);
            ctx.fill();
          }
        }

        ctx.restore();
      });

      // Render sensor touch points & explosion ripples
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';

      // Update & render ripples
      for (let i = ripplesRef.current.length - 1; i >= 0; i--) {
        const r = ripplesRef.current[i];
        r.r += 3.5;
        r.alpha *= 0.94;

        ctx.beginPath();
        ctx.arc(r.x, r.y, r.r, 0, Math.PI * 2);
        ctx.strokeStyle = `rgba(255, 127, 80, ${r.alpha})`;
        ctx.lineWidth = 3;
        ctx.stroke();

        ctx.beginPath();
        ctx.arc(r.x, r.y, r.r * 0.5, 0, Math.PI * 2);
        ctx.strokeStyle = `rgba(255, 209, 102, ${r.alpha * 0.8})`;
        ctx.lineWidth = 1.5;
        ctx.stroke();

        if (r.alpha < 0.02) {
          ripplesRef.current.splice(i, 1);
        }
      }

      // Render active sensor points (G13 - Green sensor dots on output)
      activeTouchPoints.forEach((pt) => {
        const cx = pt.x <= 1.0 ? pt.x * width : (pt.x / 1920) * width;
        const cy = pt.y <= 1.0 ? pt.y * height : (pt.y / 1080) * height;

        // Glowing halo
        const grad = ctx.createRadialGradient(cx, cy, 2, cx, cy, 20);
        grad.addColorStop(0, 'rgba(6, 214, 160, 0.9)');
        grad.addColorStop(0.5, 'rgba(6, 214, 160, 0.4)');
        grad.addColorStop(1, 'rgba(6, 214, 160, 0)');
        ctx.fillStyle = grad;
        ctx.beginPath();
        ctx.arc(cx, cy, 20, 0, Math.PI * 2);
        ctx.fill();

        // Target crosshair dot
        ctx.fillStyle = '#FFFFFF';
        ctx.beginPath();
        ctx.arc(cx, cy, 3, 0, Math.PI * 2);
        ctx.fill();

        ctx.fillStyle = '#06D6A0';
        ctx.font = 'bold 9px monospace';
        ctx.fillText(`ID:${pt.id}`, cx + 8, cy - 6);
      });

      // Test Grid Card overlay if enabled (G key)
      if (showTestGrid) {
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.35)';
        ctx.lineWidth = 1;

        // 16x9 grid
        const cols = 16;
        const rows = 9;
        for (let c = 0; c <= cols; c++) {
          const gx = (c / cols) * width;
          ctx.beginPath();
          ctx.moveTo(gx, 0);
          ctx.lineTo(gx, height);
          ctx.stroke();
        }
        for (let r = 0; r <= rows; r++) {
          const gy = (r / rows) * height;
          ctx.beginPath();
          ctx.moveTo(0, gy);
          ctx.lineTo(width, gy);
          ctx.stroke();
        }

        // Center crosshair and circle
        ctx.strokeStyle = '#FF7F50';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(width / 2, height / 2, Math.min(width, height) * 0.25, 0, Math.PI * 2);
        ctx.stroke();

        ctx.beginPath();
        ctx.moveTo(width / 2 - 30, height / 2);
        ctx.lineTo(width / 2 + 30, height / 2);
        ctx.moveTo(width / 2, height / 2 - 30);
        ctx.lineTo(width / 2, height / 2 + 30);
        ctx.stroke();

        // Corner diagonally crossing lines
        ctx.strokeStyle = 'rgba(17, 138, 178, 0.4)';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.moveTo(0, 0);
        ctx.lineTo(width, height);
        ctx.moveTo(width, 0);
        ctx.lineTo(0, height);
        ctx.stroke();

        // Test card label
        ctx.fillStyle = '#FFD166';
        ctx.font = 'bold 12px monospace';
        ctx.fillText('TEST CARD // 1920x1080 60Hz', 16, 28);
      }

      ctx.restore();

      animFrameRef.current = requestAnimationFrame(render);
    };

    animFrameRef.current = requestAnimationFrame(render);

    return () => {
      isRunning = false;
      if (animFrameRef.current) {
        cancelAnimationFrame(animFrameRef.current);
      }
    };
  }, [layers, clips, activeTouchPoints, showTestGrid]);

  const handleClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!interactive) return;
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const x = ((e.clientX - rect.left) / rect.width) * canvas.width;
    const y = ((e.clientY - rect.top) / rect.height) * canvas.height;

    ripplesRef.current.push({
      x,
      y,
      r: 6,
      alpha: 1.0,
      color: '#FF7F50'
    });

    if (onCanvasClick) {
      onCanvasClick(x, y);
    }
  };

  return (
    <canvas
      ref={canvasRef}
      width={960}
      height={540}
      onClick={handleClick}
      className={`w-full h-full object-contain ${interactive ? 'cursor-crosshair' : ''} ${className}`}
    />
  );
};
