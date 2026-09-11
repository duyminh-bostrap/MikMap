import React, { useState, useRef, useEffect } from 'react';
import { 
  Activity, 
  Cpu, 
  Wifi, 
  Wand2, 
  Target, 
  Crosshair, 
  Sliders, 
  CheckCircle2, 
  AlertCircle,
  Play,
  RotateCcw,
  Sparkles
} from 'lucide-react';
import { SensorDevice, SensorPoint, ParameterRoute, Language } from '../types';
import { translations } from '../utils/localization';
import { solveHomographyDLT, Point2D, Mat3, isPointInPolygon } from '../utils/math';

interface SensorIOViewProps {
  devices: SensorDevice[];
  setDevices: React.Dispatch<React.SetStateAction<SensorDevice[]>>;
  activeTouchPoints: SensorPoint[];
  setActiveTouchPoints: React.Dispatch<React.SetStateAction<SensorPoint[]>>;
  routes: ParameterRoute[];
  setRoutes: React.Dispatch<React.SetStateAction<ParameterRoute[]>>;
  lang: Language;
  onTriggerExplosion?: () => void;
}

export const SensorIOView: React.FC<SensorIOViewProps> = ({
  devices,
  setDevices,
  activeTouchPoints,
  setActiveTouchPoints,
  routes,
  setRoutes,
  lang,
  onTriggerExplosion
}) => {
  const t = translations[lang];

  // Radar Canvas & Sweep state
  const radarCanvasRef = useRef<HTMLCanvasElement | null>(null);
  const animFrameRef = useRef<number | null>(null);
  const sweepAngleRef = useRef<number>(0);

  // ROI Zone polygon in normalized radar coords [-1, 1]
  const [roiPolygon, setRoiPolygon] = useState<Point2D[]>([
    [-0.7, -0.6],
    [0.7, -0.6],
    [0.85, 0.7],
    [-0.85, 0.7]
  ]);
  const [isEditingRoi, setIsEditingRoi] = useState<boolean>(false);

  // Calibration Wizard State
  const [wizardStep, setWizardStep] = useState<number>(0); // 0 = idle, 1..4 = capturing corners
  const [calibrationPairs, setCalibrationPairs] = useState<
    Array<{ target: Point2D; measured: Point2D | null }>
  >([
    { target: [100, 100], measured: [-0.65, -0.55] },
    { target: [1820, 100], measured: [0.65, -0.55] },
    { target: [1820, 980], measured: [0.75, 0.65] },
    { target: [100, 980], measured: [-0.75, 0.65] }
  ]);
  const [computedMatrix, setComputedMatrix] = useState<Mat3 | null>(null);
  const [reprojectionError, setReprojectionError] = useState<number>(0.92);

  // Filter parameters
  const [noiseThreshold, setNoiseThreshold] = useState<number>(1.2);
  const [minBlobSize, setMinBlobSize] = useState<number>(15);

  // Compute Homography matrix H_s when wizard completes
  const calculateCalibration = (pairs = calibrationPairs) => {
    const validPairs = pairs.filter((p) => p.measured !== null);
    if (validPairs.length < 4) return;

    const srcPoints: Point2D[] = validPairs.map((p) => p.measured!);
    const dstPoints: Point2D[] = validPairs.map((p) => p.target);

    const h = solveHomographyDLT(srcPoints, dstPoints);
    if (h) {
      setComputedMatrix(h);
      // Compute RMS error
      let totalErr = 0;
      validPairs.forEach((p) => {
        const mapped = h.transformPoint(p.measured!);
        const dx = mapped[0] - p.target[0];
        const dy = mapped[1] - p.target[1];
        totalErr += Math.sqrt(dx * dx + dy * dy);
      });
      setReprojectionError(parseFloat((totalErr / validPairs.length).toFixed(2)));
    }
  };

  useEffect(() => {
    calculateCalibration();
  }, []);

  // Radar Animation Loop
  useEffect(() => {
    const canvas = radarCanvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let isRunning = true;

    const renderRadar = () => {
      if (!isRunning) return;
      sweepAngleRef.current = (sweepAngleRef.current + 0.04) % (Math.PI * 2);
      const angle = sweepAngleRef.current;
      const w = canvas.width;
      const h = canvas.height;
      const cx = w / 2;
      const cy = h / 2;
      const maxRadius = Math.min(cx, cy) - 25;

      // Clear dark background
      ctx.fillStyle = '#080808';
      ctx.fillRect(0, 0, w, h);

      // Concentric distance rings (1m, 2m, 3m, 4m)
      ctx.strokeStyle = '#1a1a1a';
      ctx.lineWidth = 1;
      const ringSteps = [0.25, 0.5, 0.75, 1.0];
      const ringLabels = ['1.0m', '2.0m', '3.0m', '4.0m'];

      ringSteps.forEach((step, idx) => {
        const r = maxRadius * step;
        ctx.beginPath();
        ctx.arc(cx, cy, r, 0, Math.PI * 2);
        ctx.stroke();

        ctx.fillStyle = '#444';
        ctx.font = '9px monospace';
        ctx.fillText(ringLabels[idx], cx + 4, cy - r + 11);
      });

      // Radial cross lines
      ctx.strokeStyle = '#181818';
      for (let i = 0; i < 8; i++) {
        const rad = (i * Math.PI) / 4;
        ctx.beginPath();
        ctx.moveTo(cx, cy);
        ctx.lineTo(cx + Math.cos(rad) * maxRadius, cy + Math.sin(rad) * maxRadius);
        ctx.stroke();
      }

      // ROI Polygon
      ctx.beginPath();
      roiPolygon.forEach((p, idx) => {
        const px = cx + p[0] * maxRadius;
        const py = cy + p[1] * maxRadius;
        if (idx === 0) ctx.moveTo(px, py);
        else ctx.lineTo(px, py);
      });
      ctx.closePath();
      ctx.fillStyle = isEditingRoi ? 'rgba(255, 209, 102, 0.15)' : 'rgba(17, 138, 178, 0.1)';
      ctx.fill();
      ctx.strokeStyle = isEditingRoi ? '#FFD166' : '#118AB2';
      ctx.lineWidth = 2;
      ctx.stroke();

      // Draw ROI vertices if editing
      if (isEditingRoi) {
        roiPolygon.forEach((p) => {
          const px = cx + p[0] * maxRadius;
          const py = cy + p[1] * maxRadius;
          ctx.fillStyle = '#FFD166';
          ctx.beginPath();
          ctx.arc(px, py, 6, 0, Math.PI * 2);
          ctx.fill();
        });
      }

      // Radar Sweep Line & Beam Gradient
      const beamGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, maxRadius);
      beamGrad.addColorStop(0, 'rgba(6, 214, 160, 0.3)');
      beamGrad.addColorStop(1, 'rgba(6, 214, 160, 0)');

      ctx.save();
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.arc(cx, cy, maxRadius, angle - 0.35, angle);
      ctx.closePath();
      ctx.fillStyle = beamGrad;
      ctx.fill();

      // Main sweep needle
      ctx.strokeStyle = '#06D6A0';
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(cx + Math.cos(angle) * maxRadius, cy + Math.sin(angle) * maxRadius);
      ctx.stroke();
      ctx.restore();

      // Draw active detected touch blobs
      activeTouchPoints.forEach((pt) => {
        const bx = cx + pt.x * maxRadius;
        const by = cy + pt.y * maxRadius;

        // Glowing blip
        ctx.fillStyle = '#FF7F50';
        ctx.beginPath();
        ctx.arc(bx, by, 7, 0, Math.PI * 2);
        ctx.fill();

        ctx.strokeStyle = '#FFFFFF';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.arc(bx, by, 12, 0, Math.PI * 2);
        ctx.stroke();

        ctx.fillStyle = '#FFFFFF';
        ctx.font = 'bold 9px monospace';
        ctx.fillText(`ID:${pt.id} [${(pt.x * 2000).toFixed(0)}mm]`, bx + 12, by + 3);
      });

      // Calibration target crosshair if wizard is active
      if (wizardStep >= 1 && wizardStep <= 4) {
        const activePair = calibrationPairs[wizardStep - 1];
        const tx = cx + (activePair.measured ? activePair.measured[0] : 0) * maxRadius;
        const ty = cy + (activePair.measured ? activePair.measured[1] : 0) * maxRadius;

        ctx.strokeStyle = '#FFD166';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(tx, ty, 18, 0, Math.PI * 2);
        ctx.stroke();

        ctx.fillStyle = '#FFD166';
        ctx.fillText(`TARGET ${wizardStep}/4`, tx + 20, ty - 5);
      }

      animFrameRef.current = requestAnimationFrame(renderRadar);
    };

    animFrameRef.current = requestAnimationFrame(renderRadar);

    return () => {
      isRunning = false;
      if (animFrameRef.current) cancelAnimationFrame(animFrameRef.current);
    };
  }, [roiPolygon, isEditingRoi, activeTouchPoints, wizardStep, calibrationPairs]);

  // Click on Radar to generate or register touch point
  const handleRadarClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = radarCanvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const cx = rect.width / 2;
    const cy = rect.height / 2;
    const maxRadius = Math.min(cx, cy) - 25;

    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;

    const normX = (mouseX - cx) / maxRadius;
    const normY = (mouseY - cy) / maxRadius;

    // Check if within bounds
    const dist = Math.sqrt(normX * normX + normY * normY);
    if (dist > 1.05) return;

    if (wizardStep >= 1 && wizardStep <= 4) {
      // Advance calibration wizard step
      const stepIdx = wizardStep - 1;
      const nextPairs = [...calibrationPairs];
      nextPairs[stepIdx] = {
        ...nextPairs[stepIdx],
        measured: [parseFloat(normX.toFixed(3)), parseFloat(normY.toFixed(3))]
      };
      setCalibrationPairs(nextPairs);

      if (wizardStep === 4) {
        calculateCalibration(nextPairs);
        setWizardStep(0);
      } else {
        setWizardStep(wizardStep + 1);
      }
      return;
    }

    // Normal touch injection
    const newPt: SensorPoint = {
      id: Math.floor(Math.random() * 900) + 100,
      x: normX,
      y: normY,
      z: 0,
      confidence: 0.98,
      state: 'down'
    };

    setActiveTouchPoints([newPt]);

    // Fire explosion callback if route active
    if (routes.find((r) => r.sourceKey === 'touch.down' && r.active) && onTriggerExplosion) {
      onTriggerExplosion();
    }

    // Reset touch down to move after 100ms
    setTimeout(() => {
      setActiveTouchPoints((pts) =>
        pts.map((p) => (p.id === newPt.id ? { ...p, state: 'move' } : p))
      );
    }, 100);
  };

  // Toggle sensor device connection
  const toggleDevice = (id: string) => {
    setDevices((prev) =>
      prev.map((d) => (d.id === id ? { ...d, connected: !d.connected } : d))
    );
  };

  return (
    <div className="flex-grow flex w-full h-full bg-[#0a0a0a] overflow-hidden select-none">
      {/* LEFT: Device Manager & Calibration Wizard */}
      <div className="w-[320px] flex flex-col bg-[#121212] border-r border-[#2a2a2a] shrink-0">
        <div className="p-3 border-b border-[#2a2a2a] bg-[#181818] flex items-center justify-between">
          <span className="text-[10px] font-bold tracking-widest text-[#E0E0E0]">
            {t.deviceManager}
          </span>
          <Cpu size={13} className="text-[#888]" />
        </div>

        <div className="p-3 flex-grow overflow-y-auto space-y-3">
          {/* Devices list */}
          {devices.map((dev) => (
            <div
              key={dev.id}
              className={`p-2.5 rounded-lg border transition-all ${
                dev.connected
                  ? 'bg-[#181818] border-[#383838]'
                  : 'bg-[#141414] border-[#262626] opacity-65'
              }`}
            >
              <div className="flex items-center justify-between mb-1">
                <div className="flex items-center gap-2">
                  <div
                    className={`w-2 h-2 rounded-full ${
                      dev.connected ? 'bg-[#06D6A0] shadow-[0_0_6px_#06D6A0]' : 'bg-[#555]'
                    }`}
                  />
                  <span className="text-xs font-bold text-white">{dev.name}</span>
                </div>
                <button
                  onClick={() => toggleDevice(dev.id)}
                  className={`px-2 py-0.5 rounded text-[9px] font-bold transition-colors ${
                    dev.connected
                      ? 'bg-red-950/40 text-red-400 hover:bg-red-900/60 border border-red-800/40'
                      : 'bg-[#06D6A0]/15 text-[#06D6A0] hover:bg-[#06D6A0]/25 border border-[#06D6A0]/40'
                  }`}
                >
                  {dev.connected ? t.disconnect : t.connect}
                </button>
              </div>

              <div className="text-[10px] font-mono text-[#888] truncate">{dev.endpoint}</div>

              {dev.connected && (
                <div className="mt-2 pt-2 border-t border-[#252525] grid grid-cols-3 gap-1 text-[9px] font-mono text-[#aaa]">
                  <div>
                    <span className="text-[#666] block">FPS</span>
                    <span className="text-[#06D6A0] font-bold">{dev.fps}</span>
                  </div>
                  <div>
                    <span className="text-[#666] block">LATENCY</span>
                    <span>{dev.latencyMs}ms</span>
                  </div>
                  <div>
                    <span className="text-[#666] block">PACKETS</span>
                    <span>{dev.packets}</span>
                  </div>
                </div>
              )}
            </div>
          ))}

          {/* Calibration Wizard Section (DLT 4-point) */}
          <div className="pt-3 border-t border-[#2a2a2a] space-y-2">
            <div className="flex items-center justify-between">
              <span className="text-[10px] font-bold text-[#FFD166] uppercase tracking-wider flex items-center gap-1.5">
                <Wand2 size={12} />
                {t.calibrationWizard}
              </span>
              <span className="text-[9px] font-mono text-[#06D6A0]">RMS: {reprojectionError}px</span>
            </div>

            <p className="text-[10px] text-[#888] leading-relaxed">
              Maps sensor coordinates to projector coordinates via 3x3 Homography (DLT).
            </p>

            {wizardStep === 0 ? (
              <button
                onClick={() => setWizardStep(1)}
                className="w-full py-2 rounded bg-gradient-to-r from-[#FF7F50] to-[#FFD166] text-black font-bold text-xs shadow-md hover:brightness-110 flex items-center justify-center gap-2 transition-all cursor-pointer"
              >
                <Crosshair size={13} />
                <span>{t.startWizard}</span>
              </button>
            ) : (
              <div className="bg-[#1c1c1c] p-2.5 rounded border border-[#FFD166]/50 space-y-2">
                <div className="flex justify-between items-center text-xs font-bold text-[#FFD166]">
                  <span>STEP {wizardStep} OF 4</span>
                  <span className="text-[10px] text-[#888] font-mono">
                    {wizardStep === 1
                      ? 'TOP-LEFT'
                      : wizardStep === 2
                      ? 'TOP-RIGHT'
                      : wizardStep === 3
                      ? 'BOTTOM-RIGHT'
                      : 'BOTTOM-LEFT'}
                  </span>
                </div>
                <div className="text-[10px] text-white">
                  {t.calibCrosshair} or click target on radar canvas.
                </div>
                <button
                  onClick={() => setWizardStep(0)}
                  className="w-full py-1 text-[10px] text-[#888] hover:text-white bg-[#141414] rounded border border-[#333]"
                >
                  Cancel Wizard
                </button>
              </div>
            )}

            {/* Matrix Hs Display */}
            {computedMatrix && (
              <div className="bg-[#0f0f0f] p-2 rounded border border-[#222] font-mono text-[9px] text-[#888] space-y-1">
                <div className="text-[#FF7F50] font-bold">{t.matrixHs}</div>
                <div className="grid grid-cols-3 gap-1 text-center bg-black/50 p-1.5 rounded">
                  {computedMatrix.m.map((val, i) => (
                    <span key={i} className="text-[#aaa]">
                      {val.toFixed(2)}
                    </span>
                  ))}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* CENTER: 2D Radar Canvas & Controls */}
      <div className="flex-grow flex flex-col bg-[#050505] overflow-hidden">
        {/* Radar Toolbar */}
        <div className="h-11 bg-[#121212] border-b border-[#2a2a2a] flex items-center justify-between px-4 shrink-0">
          <div className="flex items-center gap-3">
            <span className="text-xs font-bold tracking-wider text-white flex items-center gap-2">
              <Activity size={13} className="text-[#06D6A0]" />
              {t.radarView}
            </span>
            <span className="text-[10px] font-mono text-[#888]">40.0 Hz Real-Time Sweep</span>
          </div>

          <div className="flex items-center gap-2">
            <button
              onClick={() => setIsEditingRoi(!isEditingRoi)}
              className={`px-3 py-1 rounded text-xs font-bold flex items-center gap-1.5 transition-colors ${
                isEditingRoi
                  ? 'bg-[#FFD166] text-black shadow-sm'
                  : 'bg-[#1c1c1c] text-[#888] hover:text-white border border-[#333]'
              }`}
            >
              <Target size={12} />
              <span>{isEditingRoi ? t.finishRoi : t.editRoi}</span>
            </button>
          </div>
        </div>

        {/* Interactive Radar Display */}
        <div className="flex-grow p-4 flex flex-col items-center justify-center relative overflow-hidden">
          <div className="relative aspect-square w-full max-w-[540px] max-h-[540px] rounded-full overflow-hidden border border-[#2a2a2a] shadow-2xl">
            <canvas
              ref={radarCanvasRef}
              width={640}
              height={640}
              onClick={handleRadarClick}
              className="w-full h-full cursor-crosshair"
            />
          </div>
          <span className="text-[10px] font-mono text-[#666] mt-3">{t.simulatedTouch}</span>
        </div>
      </div>

      {/* RIGHT: Blob Tracking & Parameter Routing Patch Bay */}
      <div className="w-[300px] flex flex-col bg-[#121212] border-l border-[#2a2a2a] shrink-0">
        <div className="p-3 border-b border-[#2a2a2a] bg-[#181818] flex items-center justify-between">
          <span className="text-[10px] font-bold tracking-widest text-[#E0E0E0]">
            {t.parameterRouting}
          </span>
          <Sliders size={13} className="text-[#888]" />
        </div>

        <div className="p-3.5 flex-grow overflow-y-auto space-y-5">
          {/* Tracking & One-Euro Filter sliders */}
          <div className="space-y-3">
            <div className="text-[10px] font-bold text-[#888] uppercase tracking-wider">
              {t.blobTracking}
            </div>

            <div>
              <div className="flex justify-between text-[10px] text-[#888] mb-1 font-bold">
                <span>{t.noiseThreshold}</span>
                <span className="font-mono text-[#06D6A0]">{noiseThreshold}</span>
              </div>
              <input
                type="range"
                min="0.2"
                max="4.0"
                step="0.1"
                value={noiseThreshold}
                onChange={(e) => setNoiseThreshold(parseFloat(e.target.value))}
                className="w-full accent-[#06D6A0] h-1.5 bg-[#050505] rounded-lg cursor-pointer"
              />
            </div>

            <div>
              <div className="flex justify-between text-[10px] text-[#888] mb-1 font-bold">
                <span>{t.minBlobSize}</span>
                <span className="font-mono text-[#06D6A0]">{minBlobSize}mm</span>
              </div>
              <input
                type="range"
                min="5"
                max="50"
                step="1"
                value={minBlobSize}
                onChange={(e) => setMinBlobSize(parseInt(e.target.value))}
                className="w-full accent-[#06D6A0] h-1.5 bg-[#050505] rounded-lg cursor-pointer"
              />
            </div>
          </div>

          {/* Parameter Routing Patch Bay */}
          <div className="pt-3 border-t border-[#222] space-y-3">
            <div className="flex items-center justify-between">
              <span className="text-[10px] font-bold text-[#888] uppercase tracking-wider">
                PATCH CORDS (ROUTING)
              </span>
              <span className="text-[9px] font-mono text-[#FF7F50]">
                {routes.filter((r) => r.active).length} ACTIVE
              </span>
            </div>

            <div className="space-y-2">
              {routes.map((route) => (
                <div
                  key={route.id}
                  className={`p-2.5 rounded-lg border transition-all ${
                    route.active
                      ? 'bg-[#181818] border-[#FF7F50]/40 shadow-sm'
                      : 'bg-[#141414] border-[#252525] opacity-60'
                  }`}
                >
                  <div className="flex items-center justify-between mb-1.5">
                    <span className="text-[10px] font-bold text-white truncate max-w-[190px]">
                      {route.sourceLabel}
                    </span>
                    <input
                      type="checkbox"
                      checked={route.active}
                      onChange={(e) =>
                        setRoutes((prev) =>
                          prev.map((r) => (r.id === route.id ? { ...r, active: e.target.checked } : r))
                        )
                      }
                      className="accent-[#FF7F50]"
                    />
                  </div>

                  {/* Flow arrow */}
                  <div className="flex items-center gap-1.5 text-[9px] font-mono text-[#FF7F50] pl-1">
                    <span>➔</span>
                    <span className="text-[#06D6A0]">{route.targetLabel}</span>
                  </div>
                </div>
              ))}
            </div>

            <button
              onClick={() => {
                if (onTriggerExplosion) onTriggerExplosion();
              }}
              className="w-full py-2 rounded bg-[#1c1c1c] hover:bg-[#252525] text-[#FF7F50] hover:text-white border border-[#333] text-xs font-bold flex items-center justify-center gap-2 transition-colors cursor-pointer"
            >
              <Sparkles size={13} />
              <span>Simulate Particle Trigger</span>
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
